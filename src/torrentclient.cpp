#include "torrentclient.h"
#include "torrentdatabase.h"
#include "p2pnetwork.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QDateTime>
#include <optional>

// Neutralise Qt's `emit` macro across librats includes (EventBus::emit collides).
// librats' BitTorrent headers use `emit`, `slots` and `signals` as ordinary
// identifiers, which collide with Qt's keyword macros — neutralise them across the
// includes and restore afterwards.
#pragma push_macro("emit")
#pragma push_macro("slots")
#pragma push_macro("signals")
#undef emit
#undef slots
#undef signals
#ifdef RATS_SEARCH_FEATURES
#include "subsystems/bittorrent.h"
#include "bittorrent/client.h"
#include "bittorrent/torrent.h"
#include "bittorrent/torrent_info.h"
#include "bittorrent/torrent_creator.h"
#include "bittorrent/types.h"
#endif
#pragma pop_macro("signals")
#pragma pop_macro("slots")
#pragma pop_macro("emit")

#ifdef RATS_SEARCH_FEATURES
namespace {
namespace bt = librats::bittorrent;

// Parse a 40-char hex info hash into librats' 20-byte form (nullopt if malformed).
std::optional<bt::InfoHash> toInfoHash(const QString& hex)
{
    return bt::info_hash_from_hex(hex.toStdString());
}

// Adapt our (int, int) creation-progress callback to librats' piece-hash callback,
// so the UI progress bar is driven while create_from_path hashes each piece.
bt::PieceHashProgress toPieceHashProgress(const TorrentClient::CreationProgressCallback& cb)
{
    if (!cb) {
        return {};
    }
    return [cb](std::uint32_t done, std::uint32_t total) {
        cb(static_cast<int>(done), static_cast<int>(total));
    };
}

// Write the .torrent bytes a creator already produced (from create_from_path) to
// disk. No hashing happens here — the pieces are reused.
bool writeTorrentFile(const bt::TorrentCreator& creator, const QString& outputFile)
{
    const auto& bytes = creator.torrent_file();
    QFile out(outputFile);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "TorrentClient: Failed to open output file:" << outputFile;
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()));
    out.close();
    return true;
}
} // namespace
#endif

// Resolve the BitTorrent client from the P2P network's Bittorrent subsystem.
librats::bittorrent::Client* TorrentClient::btClient() const
{
#ifdef RATS_SEARCH_FEATURES
    if (!p2pNetwork_) {
        return nullptr;
    }
    librats::Bittorrent* bt = p2pNetwork_->bittorrent();
    return bt ? bt->client() : nullptr;
#else
    return nullptr;
#endif
}

// ============================================================================
// TorrentFileInfo
// ============================================================================

QJsonObject TorrentFileInfo::toJson() const
{
    QJsonObject obj;
    obj["path"] = path;
    obj["size"] = size;
    obj["index"] = index;
    obj["selected"] = selected;
    obj["progress"] = progress;
    return obj;
}

// ============================================================================
// ActiveTorrent
// ============================================================================

QJsonObject ActiveTorrent::toJson() const
{
    QJsonObject obj;
    obj["hash"] = hash;
    obj["name"] = name;
    obj["savePath"] = savePath;
    obj["totalSize"] = totalSize;
    obj["downloadedBytes"] = downloadedBytes;
    obj["progress"] = progress;
    obj["downloadSpeed"] = downloadSpeed;
    obj["peersConnected"] = peersConnected;
    obj["paused"] = paused;
    obj["removeOnDone"] = removeOnDone;
    obj["ready"] = ready;
    obj["completed"] = completed;

    QJsonArray filesArr;
    for (const TorrentFileInfo& f : files) {
        filesArr.append(f.toJson());
    }
    obj["files"] = filesArr;

    return obj;
}

QJsonObject ActiveTorrent::toProgressJson() const
{
    QJsonObject obj;
    obj["received"] = downloadedBytes;
    obj["downloaded"] = downloadedBytes;
    obj["progress"] = progress;
    obj["downloadSpeed"] = downloadSpeed;
    obj["timeRemaining"] = downloadSpeed > 0 ?
        static_cast<qint64>((totalSize - downloadedBytes) / downloadSpeed) : 0;
    obj["paused"] = paused;
    obj["removeOnDone"] = removeOnDone;
    return obj;
}

// ============================================================================
// TorrentClient - Constructor / Destructor
// ============================================================================

TorrentClient::TorrentClient(QObject *parent)
    : QObject(parent)
    , defaultDownloadPath_(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation))
{
    // Update timer for progress polling
    updateTimer_ = new QTimer(this);
    updateTimer_->setInterval(1000);  // Update every second
    connect(updateTimer_, &QTimer::timeout, this, &TorrentClient::onUpdateTimer);
}

TorrentClient::~TorrentClient()
{
    if (updateTimer_) {
        updateTimer_->stop();
    }

#ifdef RATS_SEARCH_FEATURES
    // Persist resume data for every active torrent so downloaded pieces survive a
    // restart. librats keeps ownership of the torrents; we only ask it to save.
    if (auto* client = btClient()) {
        QMutexLocker lock(&torrentsMutex_);
        for (const ActiveTorrent& torrent : torrents_) {
            if (auto ih = toInfoHash(torrent.hash)) {
                client->save_resume_data(*ih);
            }
        }
    }
#endif

    QMutexLocker lock(&torrentsMutex_);
    torrents_.clear();
}

bool TorrentClient::initialize(P2PNetwork* p2pNetwork, TorrentDatabase* database, const QString& dataDirectory)
{
    p2pNetwork_ = p2pNetwork;
    database_ = database;
    dataDirectory_ = dataDirectory;

#ifdef RATS_SEARCH_FEATURES
    if (!p2pNetwork_) {
        qWarning() << "TorrentClient: P2PNetwork not provided";
        return false;
    }

    // BitTorrent is attached to the node at start(); make sure it is up.
    if (!p2pNetwork_->isBitTorrentEnabled() || !btClient()) {
        qWarning() << "TorrentClient: BitTorrent subsystem not available";
        return false;
    }

    initialized_ = true;
    updateTimer_->start();

    qInfo() << "TorrentClient initialized successfully";
    return true;
#else
    Q_UNUSED(dataDirectory);
    qWarning() << "TorrentClient: RATS_SEARCH_FEATURES not enabled at compile time";
    return false;
#endif
}

bool TorrentClient::isReady() const
{
#ifdef RATS_SEARCH_FEATURES
    if (!initialized_ || !p2pNetwork_) {
        return false;
    }
    return p2pNetwork_->isBitTorrentEnabled() && btClient() != nullptr;
#else
    return false;
#endif
}

// ============================================================================
// Download Management
// ============================================================================

bool TorrentClient::downloadTorrent(const QString& magnetLink, const QString& savePath)
{
#ifdef RATS_SEARCH_FEATURES
    if (!isReady()) {
        qWarning() << "TorrentClient: Not ready";
        return false;
    }

    QString hash = parseInfoHash(magnetLink);
    if (hash.isEmpty() || hash.length() != 40) {
        qWarning() << "TorrentClient: Invalid magnet link or hash:" << magnetLink;
        return false;
    }

    hash = hash.toLower();

    // Check if already downloading
    {
        QMutexLocker lock(&torrentsMutex_);
        if (torrents_.contains(hash)) {
            qInfo() << "TorrentClient: Already downloading:" << hash;
            return false;
        }
    }

    QString path = savePath.isEmpty() ? defaultDownloadPath_ : savePath;

    // Ensure download directory exists
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    auto* client = btClient();

    qInfo() << "TorrentClient: Adding torrent" << hash << "to" << path;

    // Add torrent by magnet (uses DHT for peer discovery). The torrent starts
    // metadata-less and auto-starts; metadata is fetched asynchronously via BEP 9
    // and picked up by onUpdateTimer's status poll.
    auto* download = client->add_magnet("magnet:?xt=urn:btih:" + hash.toStdString(),
                                        path.toStdString());
    if (!download) {
        qWarning() << "TorrentClient: Failed to add magnet:" << hash;
        return false;
    }

    ActiveTorrent torrent;
    torrent.hash = hash;
    torrent.name = hash;  // placeholder until metadata arrives
    torrent.savePath = path;
    torrent.added = true;
    torrent.ready = false;

    {
        QMutexLocker lock(&torrentsMutex_);
        torrents_[hash] = torrent;
    }

    // UI can show "Fetching metadata..." until ready=true.
    emit downloadStarted(hash);
    return true;
#else
    Q_UNUSED(magnetLink);
    Q_UNUSED(savePath);
    qWarning() << "TorrentClient: BitTorrent not available (RATS_SEARCH_FEATURES not enabled)";
    return false;
#endif
}

bool TorrentClient::downloadTorrentFile(const QString& torrentFile, const QString& savePath)
{
#ifdef RATS_SEARCH_FEATURES
    if (!isReady()) {
        qWarning() << "TorrentClient: Not ready";
        return false;
    }

    QString path = savePath.isEmpty() ? defaultDownloadPath_ : savePath;

    // Ensure download directory exists
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    auto* client = btClient();

    qInfo() << "TorrentClient: Adding torrent file" << torrentFile << "to" << path;

    // Parse the .torrent up front (pure, thread-safe) so we have full metadata
    // without reaching into the reactor-owned Torrent object.
    auto info = bt::TorrentInfo::from_file(torrentFile.toStdString());
    if (!info || !info->is_valid()) {
        qWarning() << "TorrentClient: Failed to parse torrent file:" << torrentFile;
        return false;
    }

    QString hash = QString::fromStdString(info->info_hash_hex()).toLower();

    {
        QMutexLocker lock(&torrentsMutex_);
        if (torrents_.contains(hash)) {
            qInfo() << "TorrentClient: Already downloading:" << hash;
            return false;
        }
    }

    if (!client->add_torrent(*info, path.toStdString())) {
        qWarning() << "TorrentClient: Failed to add torrent file:" << torrentFile;
        return false;
    }

    ActiveTorrent torrent;
    torrent.hash = hash;
    torrent.name = QString::fromStdString(info->name());
    torrent.savePath = path;
    torrent.totalSize = static_cast<qint64>(info->total_size());
    torrent.added = true;
    torrent.ready = true;

    for (size_t i = 0; i < info->files().files().size(); ++i) {
        const auto& file = info->files().files()[i];
        TorrentFileInfo fi;
        fi.path = QString::fromStdString(file.path);
        fi.size = static_cast<qint64>(file.size);
        fi.index = static_cast<int>(i);
        fi.selected = true;
        torrent.files.append(fi);
    }

    {
        QMutexLocker lock(&torrentsMutex_);
        torrents_[hash] = torrent;
    }

    emit downloadStarted(hash);
    emit filesReady(hash, torrent.files);

    return true;
#else
    Q_UNUSED(torrentFile);
    Q_UNUSED(savePath);
    return false;
#endif
}

void TorrentClient::stopTorrent(const QString& infoHash, bool saveResumeData)
{
#ifdef RATS_SEARCH_FEATURES
    QString hash = infoHash.toLower();

    {
        QMutexLocker lock(&torrentsMutex_);
        auto it = torrents_.find(hash);
        if (it == torrents_.end()) {
            qWarning() << "TorrentClient: Torrent not found:" << hash;
            return;
        }
        torrents_.erase(it);
    }

    // Save resume data (preserves downloaded pieces) then remove from librats.
    if (auto* client = btClient()) {
        if (auto ih = toInfoHash(hash)) {
            if (saveResumeData) {
                qInfo() << "TorrentClient: Saving resume data for:" << hash;
                client->save_resume_data(*ih);
            }
            client->remove_torrent(*ih);
        }
    }

    emit torrentRemoved(hash);
    qInfo() << "TorrentClient: Stopped and removed torrent:" << hash;
#else
    Q_UNUSED(infoHash);
    Q_UNUSED(saveResumeData);
#endif
}

bool TorrentClient::pauseTorrent(const QString& infoHash)
{
#ifdef RATS_SEARCH_FEATURES
    QString hash = infoHash.toLower();

    auto* client = btClient();
    auto ih = toInfoHash(hash);
    if (!client || !ih) {
        return false;
    }

    {
        QMutexLocker lock(&torrentsMutex_);
        auto it = torrents_.find(hash);
        if (it == torrents_.end() || it->paused) {
            return false;
        }
        it->paused = true;
    }

    client->pause_torrent(*ih);
    emit pauseStateChanged(hash, true);
    emit stateChanged(hash, QJsonObject{{"paused", true}});
    return true;
#else
    Q_UNUSED(infoHash);
    return false;
#endif
}

bool TorrentClient::resumeTorrent(const QString& infoHash)
{
#ifdef RATS_SEARCH_FEATURES
    QString hash = infoHash.toLower();

    auto* client = btClient();
    auto ih = toInfoHash(hash);
    if (!client || !ih) {
        return false;
    }

    {
        QMutexLocker lock(&torrentsMutex_);
        auto it = torrents_.find(hash);
        if (it == torrents_.end() || !it->paused) {
            return false;
        }
        it->paused = false;
    }

    client->resume_torrent(*ih);
    emit pauseStateChanged(hash, false);
    emit stateChanged(hash, QJsonObject{{"paused", false}});
    return true;
#else
    Q_UNUSED(infoHash);
    return false;
#endif
}

bool TorrentClient::togglePause(const QString& infoHash)
{
    QString hash = infoHash.toLower();

    QMutexLocker lock(&torrentsMutex_);
    auto it = torrents_.find(hash);
    if (it == torrents_.end()) {
        return false;
    }

    bool isPaused = it->paused;
    lock.unlock();

    if (isPaused) {
        return resumeTorrent(hash);
    } else {
        return pauseTorrent(hash);
    }
}

bool TorrentClient::selectFiles(const QString& infoHash, const QVector<bool>& fileSelection)
{
#ifdef RATS_SEARCH_FEATURES
    QString hash = infoHash.toLower();

    QMutexLocker lock(&torrentsMutex_);

    auto it = torrents_.find(hash);
    if (it == torrents_.end()) {
        return false;
    }

    // Update local file selection state
    for (int i = 0; i < fileSelection.size() && i < it->files.size(); ++i) {
        it->files[i].selected = fileSelection[i];
    }

    // TODO: When librats supports file selection, call it here
    // Currently librats downloads all files

    return true;
#else
    Q_UNUSED(infoHash);
    Q_UNUSED(fileSelection);
    return false;
#endif
}

void TorrentClient::setRemoveOnDone(const QString& infoHash, bool removeOnDone)
{
    QString hash = infoHash.toLower();

    QMutexLocker lock(&torrentsMutex_);

    auto it = torrents_.find(hash);
    if (it != torrents_.end()) {
        it->removeOnDone = removeOnDone;
        lock.unlock();
        emit stateChanged(hash, QJsonObject{{"removeOnDone", removeOnDone}});
    }
}

// ============================================================================
// Query Methods
// ============================================================================

bool TorrentClient::isDownloading(const QString& infoHash) const
{
    QString hash = infoHash.toLower();
    QMutexLocker lock(&torrentsMutex_);
    return torrents_.contains(hash);
}

ActiveTorrent TorrentClient::getTorrent(const QString& infoHash) const
{
    QString hash = infoHash.toLower();
    QMutexLocker lock(&torrentsMutex_);
    return torrents_.value(hash);
}

QVector<ActiveTorrent> TorrentClient::getAllTorrents() const
{
    QMutexLocker lock(&torrentsMutex_);
    QVector<ActiveTorrent> result;
    for (const auto& t : torrents_) {
        result.append(t);
    }
    return result;
}

int TorrentClient::count() const
{
    QMutexLocker lock(&torrentsMutex_);
    return torrents_.size();
}

// ============================================================================
// Configuration
// ============================================================================

void TorrentClient::setDefaultDownloadPath(const QString& path)
{
    defaultDownloadPath_ = path;
}

QString TorrentClient::defaultDownloadPath() const
{
    return defaultDownloadPath_;
}

void TorrentClient::setDatabase(TorrentDatabase* database)
{
    database_ = database;
}

bool TorrentClient::downloadWithInfo(const QString& hash, const QString& name, qint64 size,
                                      const QString& savePath)
{
    if (hash.length() != 40) {
        qWarning() << "TorrentClient: Invalid hash for download:" << hash;
        return false;
    }

    QString normalizedHash = hash.toLower();

    // Check if already downloading
    {
        QMutexLocker lock(&torrentsMutex_);
        if (torrents_.contains(normalizedHash)) {
            qInfo() << "TorrentClient: Already downloading:" << normalizedHash;
            return false;
        }
    }

    // Try to get more info from database if provided
    QString torrentName = name;
    qint64 torrentSize = size;

    if (database_ && (torrentName.isEmpty() || torrentName == hash)) {
        // Use ::TorrentInfo to reference the Qt app's TorrentInfo struct
        // (not librats' bittorrent::TorrentInfo).
        ::TorrentInfo dbInfo = database_->getTorrent(normalizedHash);
        if (dbInfo.isValid()) {
            torrentName = dbInfo.name;
            if (torrentSize == 0) {
                torrentSize = dbInfo.size;
            }
        }
    }

    // Now do the actual download
    QString path = savePath.isEmpty() ? defaultDownloadPath_ : savePath;

#ifdef RATS_SEARCH_FEATURES
    if (!isReady()) {
        qWarning() << "TorrentClient: Not ready";
        return false;
    }

    // Ensure download directory exists
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    auto* client = btClient();
    qInfo() << "TorrentClient: Adding torrent with info" << normalizedHash << torrentName.left(50);

    // add_magnet auto-starts a metadata-less torrent; metadata is fetched from DHT
    // peers via BEP 9 and picked up by the status poll.
    auto* download = client->add_magnet("magnet:?xt=urn:btih:" + normalizedHash.toStdString(),
                                        path.toStdString());
    if (!download) {
        qWarning() << "TorrentClient: Failed to add magnet:" << normalizedHash;
        return false;
    }

    {
        QMutexLocker lock(&torrentsMutex_);
        ActiveTorrent torrent;
        torrent.hash = normalizedHash;
        torrent.name = torrentName.isEmpty() ? normalizedHash : torrentName;
        torrent.totalSize = torrentSize;
        torrent.savePath = path;
        torrent.added = true;
        torrent.ready = false;   // metadata (authoritative name/files) not yet known
        torrent.completed = false;
        torrents_[normalizedHash] = torrent;
    }

    emit downloadStarted(normalizedHash);
    return true;
#else
    Q_UNUSED(path);
    Q_UNUSED(torrentName);
    Q_UNUSED(torrentSize);
    return false;
#endif
}

bool TorrentClient::restoreTorrent(const QString& hash, const QString& name, const QString& savePath, bool wasCompleted)
{
#ifdef RATS_SEARCH_FEATURES
    if (!isReady()) {
        qWarning() << "TorrentClient: Not ready for restore";
        return false;
    }

    QString normalizedHash = hash.toLower();

    // Check if already downloading
    {
        QMutexLocker lock(&torrentsMutex_);
        if (torrents_.contains(normalizedHash)) {
            qInfo() << "TorrentClient: Already active:" << normalizedHash.left(8);
            return false;
        }
    }

    QString path = savePath.isEmpty() ? defaultDownloadPath_ : savePath;

    // Ensure download directory exists
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    auto* client = btClient();

    qInfo() << "TorrentClient: Restoring torrent" << normalizedHash.left(8) << name.left(30) << "from" << path;

    // add_magnet_resumed loads any resume file saved next to the download: it
    // restores the downloaded pieces and, if the resume file embeds the info dict,
    // completes the metadata without a re-fetch.
    auto* download = client->add_magnet_resumed("magnet:?xt=urn:btih:" + normalizedHash.toStdString(),
                                                path.toStdString());
    if (!download) {
        qWarning() << "TorrentClient: Failed to restore torrent:" << normalizedHash.left(8);
        return false;
    }

    ActiveTorrent torrent;
    torrent.hash = normalizedHash;
    torrent.savePath = path;
    torrent.name = name.isEmpty() ? normalizedHash : name;
    torrent.added = true;
    torrent.ready = false;

    // If resume data already brought back the metadata, populate immediately.
    if (auto ih = toInfoHash(normalizedHash)) {
        bt::TorrentStatus status = client->torrent_status(*ih);
        if (status.exists && status.has_metadata) {
            applyStatusToTorrent(torrent, status);
        }
    }

    {
        QMutexLocker lock(&torrentsMutex_);
        torrents_[normalizedHash] = torrent;
    }

    emit downloadStarted(normalizedHash);

    if (torrent.ready) {
        emit filesReady(normalizedHash, torrent.files);

        if (torrent.completed) {
            emit downloadCompleted(normalizedHash);
        }
    }

    Q_UNUSED(wasCompleted);
    return true;
#else
    Q_UNUSED(hash);
    Q_UNUSED(name);
    Q_UNUSED(savePath);
    Q_UNUSED(wasCompleted);
    return false;
#endif
}

QJsonArray TorrentClient::toJsonArray() const
{
    QMutexLocker lock(&torrentsMutex_);
    QJsonArray arr;
    for (const ActiveTorrent& torrent : torrents_) {
        arr.append(torrent.toJson());
    }
    return arr;
}

bool TorrentClient::selectFilesJson(const QString& hash, const QJsonValue& selection)
{
    QString normalizedHash = hash.toLower();

    QMutexLocker lock(&torrentsMutex_);
    auto it = torrents_.find(normalizedHash);
    if (it == torrents_.end()) {
        return false;
    }

    ActiveTorrent& torrent = *it;

    if (selection.isArray()) {
        QJsonArray arr = selection.toArray();
        for (int i = 0; i < arr.size() && i < torrent.files.size(); ++i) {
            torrent.files[i].selected = arr[i].toBool(true);
        }
    } else if (selection.isObject()) {
        QJsonObject obj = selection.toObject();
        for (auto selIt = obj.begin(); selIt != obj.end(); ++selIt) {
            bool ok;
            int idx = selIt.key().toInt(&ok);
            if (ok && idx >= 0 && idx < torrent.files.size()) {
                torrent.files[idx].selected = selIt.value().toBool(true);
            }
        }
    }

    // Emit files ready with updated selection
    QJsonArray filesArr;
    for (const TorrentFileInfo& f : torrent.files) {
        filesArr.append(f.toJson());
    }

    lock.unlock();
    emit filesReadyJson(normalizedHash, filesArr);

    return true;
}

void TorrentClient::emitProgressJson(const QString& hash, const ActiveTorrent& torrent)
{
    QJsonObject progress;
    progress["received"] = torrent.downloadedBytes;
    progress["downloaded"] = torrent.downloadedBytes;
    progress["total"] = torrent.totalSize;
    progress["progress"] = torrent.progress;
    progress["speed"] = static_cast<int>(torrent.downloadSpeed);
    progress["downloadSpeed"] = static_cast<int>(torrent.downloadSpeed);
    progress["paused"] = torrent.paused;
    progress["removeOnDone"] = torrent.removeOnDone;

    if (torrent.downloadSpeed > 0 && torrent.totalSize > torrent.downloadedBytes) {
        progress["timeRemaining"] = static_cast<qint64>((torrent.totalSize - torrent.downloadedBytes) / torrent.downloadSpeed);
    } else {
        progress["timeRemaining"] = 0;
    }

    emit progressUpdated(hash, progress);
}

// ============================================================================
// Session Persistence
// ============================================================================

bool TorrentClient::saveSession(const QString& filePath)
{
#ifdef RATS_SEARCH_FEATURES
    auto* client = btClient();
#endif

    QMutexLocker lock(&torrentsMutex_);

    if (torrents_.isEmpty()) {
        QFile::remove(filePath);
        return true;
    }

    QJsonArray sessionsArray;

    for (auto& torrent : torrents_) {
        QJsonObject session;
        session["hash"] = torrent.hash;
        session["name"] = torrent.name;
        session["savePath"] = torrent.savePath;
        session["totalSize"] = torrent.totalSize;
        session["paused"] = torrent.paused;
        session["removeOnDone"] = torrent.removeOnDone;
        session["completed"] = torrent.completed;  // Also save completed torrents for seeding
        session["downloadedBytes"] = torrent.downloadedBytes;
        session["progress"] = torrent.progress;

#ifdef RATS_SEARCH_FEATURES
        // Save resume data for each torrent (preserves downloaded pieces)
        if (client) {
            if (auto ih = toInfoHash(torrent.hash)) {
                qInfo() << "TorrentClient: Saving resume data for" << torrent.hash.left(8);
                client->save_resume_data(*ih);
            }
        }
#endif

        // Save file selection
        QJsonArray filesArr;
        for (const TorrentFileInfo& f : torrent.files) {
            QJsonObject fileObj;
            fileObj["path"] = f.path;
            fileObj["size"] = f.size;
            fileObj["index"] = f.index;
            fileObj["selected"] = f.selected;
            filesArr.append(fileObj);
        }
        session["files"] = filesArr;

        sessionsArray.append(session);
    }

    if (sessionsArray.isEmpty()) {
        QFile::remove(filePath);
        return true;
    }

    // Ensure directory exists
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "TorrentClient: Failed to save session to" << filePath;
        return false;
    }

    QJsonDocument doc(sessionsArray);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qInfo() << "TorrentClient: Saved" << sessionsArray.size() << "torrents to session file";
    return true;
}

int TorrentClient::loadSession(const QString& filePath)
{
    QFile file(filePath);
    if (!file.exists()) {
        return 0;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "TorrentClient: Failed to open session file:" << filePath;
        return 0;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "TorrentClient: Failed to parse session file:" << parseError.errorString();
        return 0;
    }

    if (!doc.isArray()) {
        qWarning() << "TorrentClient: Invalid session file format";
        return 0;
    }

    QJsonArray sessions = doc.array();
    int restored = 0;

    for (const QJsonValue& val : sessions) {
        if (!val.isObject()) {
            continue;
        }

        QJsonObject session = val.toObject();
        QString hash = session["hash"].toString();

        if (hash.length() != 40) {
            continue;
        }

        QString name = session["name"].toString();
        QString savePath = session["savePath"].toString();
        bool paused = session["paused"].toBool();
        bool removeOnDone = session["removeOnDone"].toBool();
        bool wasCompleted = session["completed"].toBool();

        qInfo() << "TorrentClient: Restoring torrent:" << hash.left(8) << name.left(30)
                << (wasCompleted ? "(completed/seeding)" : "(downloading)");

        if (restoreTorrent(hash, name, savePath, wasCompleted)) {
            // Apply saved settings
            if (paused) {
                pauseTorrent(hash);
            }
            setRemoveOnDone(hash, removeOnDone);

            // Restore file selection
            QJsonArray filesArr = session["files"].toArray();
            if (!filesArr.isEmpty()) {
                QVector<bool> selection;
                for (const QJsonValue& fVal : filesArr) {
                    selection.append(fVal.toObject()["selected"].toBool(true));
                }
                selectFiles(hash, selection);
            }

            restored++;
        }
    }

    if (restored > 0) {
        qInfo() << "TorrentClient: Restored" << restored << "torrents from session";
    }

    return restored;
}

// ============================================================================
// Private Slots
// ============================================================================

void TorrentClient::onUpdateTimer()
{
#ifdef RATS_SEARCH_FEATURES
    auto* client = btClient();
    if (!client) {
        return;
    }

    // Snapshot the hashes to poll (don't hold the mutex across librats calls).
    QStringList hashes;
    {
        QMutexLocker lock(&torrentsMutex_);
        hashes = torrents_.keys();
    }

    QStringList newlyCompleted;
    QStringList newlyReady;
    QStringList toRemove;

    for (const QString& hash : hashes) {
        auto ih = toInfoHash(hash);
        if (!ih) {
            continue;
        }

        // Thread-safe snapshot from the reactor thread.
        bt::TorrentStatus status = client->torrent_status(*ih);

        QMutexLocker lock(&torrentsMutex_);
        auto it = torrents_.find(hash);
        if (it == torrents_.end()) {
            continue;
        }
        ActiveTorrent& torrent = *it;

        if (!status.exists) {
            continue;  // librats no longer tracks it (e.g. removed) — leave as-is
        }

        const bool wasReady = torrent.ready;
        const bool wasCompleted = torrent.completed;

        // Metadata just arrived for a magnet torrent → populate name/size/files.
        if (status.has_metadata && !wasReady) {
            applyStatusToTorrent(torrent, status);
            newlyReady.append(hash);
        } else {
            // Live counters.
            torrent.downloadedBytes = static_cast<qint64>(status.downloaded);
            torrent.peersConnected = static_cast<int>(status.num_peers);
            torrent.progress = status.progress;
            if (torrent.totalSize == 0 && status.total_size > 0) {
                torrent.totalSize = static_cast<qint64>(status.total_size);
            }
        }

        // Derive download speed from the byte-counter delta between polls.
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (torrent.lastSampledMs > 0 && nowMs > torrent.lastSampledMs) {
            const qint64 deltaBytes = torrent.downloadedBytes - torrent.lastSampledBytes;
            const double deltaSec = (nowMs - torrent.lastSampledMs) / 1000.0;
            torrent.downloadSpeed = deltaBytes > 0 ? deltaBytes / deltaSec : 0.0;
        }
        torrent.lastSampledBytes = torrent.downloadedBytes;
        torrent.lastSampledMs = nowMs;

        if (status.is_complete && !wasCompleted) {
            torrent.completed = true;
            torrent.progress = 1.0;
            torrent.downloadSpeed = 0.0;
            newlyCompleted.append(hash);
            if (torrent.removeOnDone) {
                toRemove.append(hash);
            }
        }

        emitProgressJson(hash, torrent);
    }

    // Emit signals outside the mutex.
    for (const QString& hash : newlyReady) {
        QVector<TorrentFileInfo> filesCopy;
        {
            QMutexLocker lock(&torrentsMutex_);
            auto it = torrents_.find(hash);
            if (it != torrents_.end()) {
                filesCopy = it->files;
            }
        }
        qInfo() << "TorrentClient: Metadata received for:" << hash.left(8);
        emit downloadStarted(hash);
        emit filesReady(hash, filesCopy);

        QJsonArray filesJson;
        for (const TorrentFileInfo& f : filesCopy) {
            filesJson.append(f.toJson());
        }
        emit filesReadyJson(hash, filesJson);
    }

    for (const QString& hash : newlyCompleted) {
        qInfo() << "TorrentClient: Download completed:" << hash.left(8);
        emit downloadCompleted(hash);
    }

    for (const QString& hash : toRemove) {
        stopTorrent(hash);
    }
#endif
}

// ============================================================================
// Private Methods
// ============================================================================

#ifdef RATS_SEARCH_FEATURES
void TorrentClient::applyStatusToTorrent(ActiveTorrent& torrent, const bt::TorrentStatus& status)
{
    if (!status.name.empty()) {
        torrent.name = QString::fromStdString(status.name);
    }
    torrent.totalSize = static_cast<qint64>(status.total_size);
    torrent.downloadedBytes = static_cast<qint64>(status.downloaded);
    torrent.progress = status.progress;
    torrent.peersConnected = static_cast<int>(status.num_peers);
    torrent.completed = status.is_complete;
    torrent.ready = status.has_metadata;

    torrent.files.clear();
    for (size_t i = 0; i < status.files.size(); ++i) {
        TorrentFileInfo fi;
        fi.path = QString::fromStdString(status.files[i].path);
        fi.size = static_cast<qint64>(status.files[i].size);
        fi.index = static_cast<int>(i);
        fi.selected = true;
        if (status.is_complete) {
            fi.progress = 1.0;
        }
        torrent.files.append(fi);
    }
}
#endif

QString TorrentClient::parseInfoHash(const QString& magnetLink) const
{
    // If it's already a hash (40 hex chars), return it
    static QRegularExpression hashRegex("^[0-9a-fA-F]{40}$");
    if (hashRegex.match(magnetLink).hasMatch()) {
        return magnetLink.toLower();
    }

    // Parse magnet link: magnet:?xt=urn:btih:HASH...
    static QRegularExpression magnetRegex("(?:magnet:\\?.*?)?(?:xt=urn:btih:)?([0-9a-fA-F]{40})",
                                          QRegularExpression::CaseInsensitiveOption);
    auto match = magnetRegex.match(magnetLink);
    if (match.hasMatch()) {
        return match.captured(1).toLower();
    }

    // Try base32 encoded hash (32 chars)
    static QRegularExpression base32Regex("(?:magnet:\\?.*?)?(?:xt=urn:btih:)?([A-Z2-7]{32})",
                                          QRegularExpression::CaseInsensitiveOption);
    match = base32Regex.match(magnetLink);
    if (match.hasMatch()) {
        // TODO: Decode base32 to hex
        qWarning() << "TorrentClient: Base32 encoded hashes not yet supported";
        return QString();
    }

    return QString();
}

// ============================================================================
// Torrent Creation
// ============================================================================

QString TorrentClient::createAndSeedTorrent(const QString& path,
                                             const QStringList& trackers,
                                             const QString& comment,
                                             const QString& saveTorrentFilePath,
                                             CreationProgressCallback progressCallback)
{
#ifdef RATS_SEARCH_FEATURES
    if (!isReady()) {
        qWarning() << "TorrentClient: Not ready for torrent creation";
        return QString();
    }

    auto* client = btClient();
    if (!client) {
        qWarning() << "TorrentClient: BitTorrent client not available";
        return QString();
    }

    qInfo() << "TorrentClient: Creating and seeding torrent from:" << path;

    // Build the torrent metadata (this hashes all file content).
    bt::TorrentCreator creator;
    for (const QString& tracker : trackers) {
        creator.add_tracker(tracker.toStdString());
    }
    if (!comment.isEmpty()) {
        creator.set_comment(comment.toStdString());
    }

    std::string createError;
    auto infoOpt = creator.create_from_path(path.toStdString(), &createError,
                                            toPieceHashProgress(progressCallback));
    if (!infoOpt) {
        qWarning() << "TorrentClient: Failed to create torrent from:" << path
                   << QString::fromStdString(createError);
        return QString();
    }
    const bt::TorrentInfo& info = *infoOpt;

    // The torrent's files live in the parent directory of the source path (the
    // path's own name becomes the single file / top-level directory entry).
    auto* download = client->add_torrent_for_seeding(info, QFileInfo(path).absolutePath().toStdString());
    if (!download) {
        qWarning() << "TorrentClient: Failed to start seeding torrent from:" << path;
        return QString();
    }

    // Reuse the just-hashed pieces to write the .torrent file (avoids re-hashing).
    if (!saveTorrentFilePath.isEmpty()) {
        writeTorrentFile(creator, saveTorrentFilePath);
    }

    QString hash = QString::fromStdString(info.info_hash_hex()).toLower();
    QString name = QString::fromStdString(info.name());
    qint64 totalSize = static_cast<qint64>(info.total_size());

    // Check if already exists
    {
        QMutexLocker lock(&torrentsMutex_);
        if (torrents_.contains(hash)) {
            qInfo() << "TorrentClient: Torrent already exists:" << hash;
            return hash;  // Return existing hash - not an error
        }
    }

    // Create ActiveTorrent entry
    ActiveTorrent torrent;
    torrent.hash = hash;
    torrent.name = name;
    // Get the save path from the source path's parent directory
    QFileInfo pathInfo(path);
    torrent.savePath = pathInfo.isDir() ? pathInfo.absolutePath() : pathInfo.dir().absolutePath();
    torrent.totalSize = totalSize;
    torrent.downloadedBytes = totalSize;  // Already complete for seeding
    torrent.progress = 1.0;
    torrent.added = true;
    torrent.ready = true;
    torrent.completed = true;

    // Populate files
    const auto& files = info.files().files();
    for (size_t i = 0; i < files.size(); ++i) {
        TorrentFileInfo fi;
        fi.path = QString::fromStdString(files[i].path);
        fi.size = static_cast<qint64>(files[i].size);
        fi.index = static_cast<int>(i);
        fi.selected = true;
        fi.progress = 1.0;  // Complete
        torrent.files.append(fi);
    }

    // Store torrent
    {
        QMutexLocker lock(&torrentsMutex_);
        torrents_[hash] = torrent;
    }

    qInfo() << "TorrentClient: Created and seeding torrent:" << name << "hash:" << hash.left(8);

    emit downloadStarted(hash);
    emit filesReady(hash, torrent.files);
    emit downloadCompleted(hash);
    emit torrentCreated(hash, name, totalSize);

    return hash;
#else
    Q_UNUSED(path);
    Q_UNUSED(trackers);
    Q_UNUSED(comment);
    Q_UNUSED(saveTorrentFilePath);
    Q_UNUSED(progressCallback);
    qWarning() << "TorrentClient: RATS_SEARCH_FEATURES not enabled";
    return QString();
#endif
}

bool TorrentClient::createTorrentFile(const QString& path,
                                       const QString& outputFile,
                                       const QStringList& trackers,
                                       const QString& comment,
                                       CreationProgressCallback progressCallback)
{
#ifdef RATS_SEARCH_FEATURES
    if (!isReady()) {
        qWarning() << "TorrentClient: Not ready for torrent creation";
        return false;
    }

    qInfo() << "TorrentClient: Creating torrent file from:" << path << "to:" << outputFile;

    bt::TorrentCreator creator;
    for (const QString& tracker : trackers) {
        creator.add_tracker(tracker.toStdString());
    }
    if (!comment.isEmpty()) {
        creator.set_comment(comment.toStdString());
    }

    std::string createError;
    auto infoOpt = creator.create_from_path(path.toStdString(), &createError,
                                            toPieceHashProgress(progressCallback));
    if (!infoOpt) {
        qWarning() << "TorrentClient: create_from_path error:" << QString::fromStdString(createError);
        return false;
    }

    if (!writeTorrentFile(creator, outputFile)) {
        return false;
    }

    qInfo() << "TorrentClient: Torrent file created:" << outputFile;
    return true;
#else
    Q_UNUSED(path);
    Q_UNUSED(outputFile);
    Q_UNUSED(trackers);
    Q_UNUSED(comment);
    Q_UNUSED(progressCallback);
    qWarning() << "TorrentClient: RATS_SEARCH_FEATURES not enabled";
    return false;
#endif
}

void TorrentClient::updateTorrentStatus(const QString& hash)
{
    // Status polling is handled inline in onUpdateTimer() via a single thread-safe
    // torrent_status() snapshot per torrent; this method is retained for source
    // compatibility but is no longer used.
    Q_UNUSED(hash);
}
