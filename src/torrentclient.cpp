#include "torrentclient.h"
#include "torrentdatabase.h"
#include "p2pnetwork.h"
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QMutexLocker>
#include <QRegularExpression>

#ifdef RATS_SEARCH_FEATURES
#include "librats/src/librats.h"
#include "librats/src/bittorrent.h"
#endif

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
    
    // Stop all active torrents
    QMutexLocker lock(&torrentsMutex_);
    for (auto& torrent : torrents_) {
        if (torrent.download) {
            torrent.download->stop();
        }
    }
    torrents_.clear();
}

bool TorrentClient::initialize(P2PNetwork* p2pNetwork, TorrentDatabase* database)
{
    p2pNetwork_ = p2pNetwork;
    database_ = database;
    
#ifdef RATS_SEARCH_FEATURES
    if (!p2pNetwork_) {
        qWarning() << "TorrentClient: P2PNetwork not provided";
        return false;
    }
    
    auto* client = p2pNetwork_->getRatsClient();
    if (!client) {
        qWarning() << "TorrentClient: RatsClient not available";
        return false;
    }
    
    // Ensure BitTorrent is enabled using P2PNetwork (uses configured DHT port)
    if (!p2pNetwork_->isBitTorrentEnabled()) {
        qInfo() << "TorrentClient: Enabling BitTorrent via P2PNetwork...";
        if (!p2pNetwork_->enableBitTorrent()) {
            qWarning() << "TorrentClient: Failed to enable BitTorrent";
            return false;
        }
    }
    
    initialized_ = true;
    updateTimer_->start();
    
    qInfo() << "TorrentClient initialized successfully";
    return true;
#else
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
    auto* client = p2pNetwork_->getRatsClient();
    return client && client->is_bittorrent_enabled();
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
    
    auto* client = p2pNetwork_->getRatsClient();
    
    qInfo() << "TorrentClient: Adding torrent" << hash << "to" << path;
    
    // Add torrent by hash (uses DHT for peer discovery)
    auto download = client->add_torrent_by_hash(hash.toStdString(), path.toStdString());
    
    if (!download) {
        qWarning() << "TorrentClient: Failed to add torrent:" << hash;
        return false;
    }
    
    // Create ActiveTorrent entry
    ActiveTorrent torrent;
    torrent.hash = hash;
    torrent.savePath = path;
    torrent.download = download;
    
    // Get info if available
    const auto& info = download->get_torrent_info();
    if (info.is_valid() && !info.is_metadata_only()) {
        torrent.name = QString::fromStdString(info.get_name());
        torrent.totalSize = static_cast<qint64>(info.get_total_length());
        torrent.ready = true;
        
        // Populate files
        const auto& files = info.get_files();
        for (size_t i = 0; i < files.size(); ++i) {
            TorrentFileInfo fi;
            fi.path = QString::fromStdString(files[i].path);
            fi.size = static_cast<qint64>(files[i].length);
            fi.index = static_cast<int>(i);
            fi.selected = true;
            torrent.files.append(fi);
        }
    } else {
        torrent.name = hash;  // Use hash as placeholder name
    }
    
    // Setup callbacks
    setupTorrentCallbacks(hash, download);
    
    // Store and start
    {
        QMutexLocker lock(&torrentsMutex_);
        torrents_[hash] = torrent;
    }
    
    download->start();
    
    // Emit signals
    if (torrent.ready) {
        emit downloadStarted(hash);
        emit filesReady(hash, torrent.files);
    }
    
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
    
    auto* client = p2pNetwork_->getRatsClient();
    
    qInfo() << "TorrentClient: Adding torrent file" << torrentFile << "to" << path;
    
    auto download = client->add_torrent(torrentFile.toStdString(), path.toStdString());
    
    if (!download) {
        qWarning() << "TorrentClient: Failed to add torrent file:" << torrentFile;
        return false;
    }
    
    const auto& info = download->get_torrent_info();
    QString hash = QString::fromStdString(librats::info_hash_to_hex(info.get_info_hash()));
    hash = hash.toLower();
    
    // Check if already downloading
    {
        QMutexLocker lock(&torrentsMutex_);
        if (torrents_.contains(hash)) {
            qInfo() << "TorrentClient: Already downloading:" << hash;
            client->remove_torrent(info.get_info_hash());
            return false;
        }
    }
    
    // Create ActiveTorrent entry
    ActiveTorrent torrent;
    torrent.hash = hash;
    torrent.name = QString::fromStdString(info.get_name());
    torrent.savePath = path;
    torrent.totalSize = static_cast<qint64>(info.get_total_length());
    torrent.download = download;
    torrent.ready = true;
    
    // Populate files
    const auto& files = info.get_files();
    for (size_t i = 0; i < files.size(); ++i) {
        TorrentFileInfo fi;
        fi.path = QString::fromStdString(files[i].path);
        fi.size = static_cast<qint64>(files[i].length);
        fi.index = static_cast<int>(i);
        fi.selected = true;
        torrent.files.append(fi);
    }
    
    // Setup callbacks
    setupTorrentCallbacks(hash, download);
    
    // Store and start
    {
        QMutexLocker lock(&torrentsMutex_);
        torrents_[hash] = torrent;
    }
    
    download->start();
    
    emit downloadStarted(hash);
    emit filesReady(hash, torrent.files);
    
    return true;
#else
    Q_UNUSED(torrentFile);
    Q_UNUSED(savePath);
    return false;
#endif
}

void TorrentClient::stopTorrent(const QString& infoHash)
{
#ifdef RATS_SEARCH_FEATURES
    QString hash = infoHash.toLower();
    
    QMutexLocker lock(&torrentsMutex_);
    
    auto it = torrents_.find(hash);
    if (it == torrents_.end()) {
        qWarning() << "TorrentClient: Torrent not found:" << hash;
        return;
    }
    
    if (it->download) {
        it->download->stop();
    }
    
    // Remove from librats
    if (p2pNetwork_ && p2pNetwork_->getRatsClient()) {
        auto* client = p2pNetwork_->getRatsClient();
        librats::InfoHash infoHashBytes = librats::hex_to_info_hash(hash.toStdString());
        client->remove_torrent(infoHashBytes);
    }
    
    torrents_.erase(it);
    lock.unlock();
    
    emit torrentRemoved(hash);
    qInfo() << "TorrentClient: Stopped and removed torrent:" << hash;
#else
    Q_UNUSED(infoHash);
#endif
}

bool TorrentClient::pauseTorrent(const QString& infoHash)
{
#ifdef RATS_SEARCH_FEATURES
    QString hash = infoHash.toLower();
    
    QMutexLocker lock(&torrentsMutex_);
    
    auto it = torrents_.find(hash);
    if (it == torrents_.end()) {
        return false;
    }
    
    if (it->download && !it->paused) {
        it->download->pause();
        it->paused = true;
        
        lock.unlock();
        emit pauseStateChanged(hash, true);
        emit stateChanged(hash, QJsonObject{{"paused", true}});
        return true;
    }
    
    return false;
#else
    Q_UNUSED(infoHash);
    return false;
#endif
}

bool TorrentClient::resumeTorrent(const QString& infoHash)
{
#ifdef RATS_SEARCH_FEATURES
    QString hash = infoHash.toLower();
    
    QMutexLocker lock(&torrentsMutex_);
    
    auto it = torrents_.find(hash);
    if (it == torrents_.end()) {
        return false;
    }
    
    if (it->download && it->paused) {
        it->download->resume();
        it->paused = false;
        
        lock.unlock();
        emit pauseStateChanged(hash, false);
        emit stateChanged(hash, QJsonObject{{"paused", false}});
        return true;
    }
    
    return false;
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
        TorrentInfo dbInfo = database_->getTorrent(normalizedHash);
        if (dbInfo.isValid()) {
            torrentName = dbInfo.name;
            if (torrentSize == 0) {
                torrentSize = dbInfo.size;
            }
        }
    }
    
    // Use downloadTorrent but first pre-populate the entry with known info
    {
        QMutexLocker lock(&torrentsMutex_);
        ActiveTorrent torrent;
        torrent.hash = normalizedHash;
        torrent.name = torrentName.isEmpty() ? normalizedHash : torrentName;
        torrent.totalSize = torrentSize;
        torrent.savePath = savePath.isEmpty() ? defaultDownloadPath_ : savePath;
        torrent.ready = false;
        torrent.completed = false;
        torrents_[normalizedHash] = torrent;
    }
    
    // Now do the actual download
    QString path = savePath.isEmpty() ? defaultDownloadPath_ : savePath;
    
#ifdef RATS_SEARCH_FEATURES
    if (!isReady()) {
        qWarning() << "TorrentClient: Not ready";
        QMutexLocker lock(&torrentsMutex_);
        torrents_.remove(normalizedHash);
        return false;
    }
    
    // Ensure download directory exists
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    auto* client = p2pNetwork_->getRatsClient();
    qInfo() << "TorrentClient: Adding torrent with info" << normalizedHash << torrentName.left(50);
    
    auto download = client->add_torrent_by_hash(normalizedHash.toStdString(), path.toStdString());
    
    if (!download) {
        qWarning() << "TorrentClient: Failed to add torrent:" << normalizedHash;
        QMutexLocker lock(&torrentsMutex_);
        torrents_.remove(normalizedHash);
        return false;
    }
    
    // Update with librats download handle
    {
        QMutexLocker lock(&torrentsMutex_);
        auto it = torrents_.find(normalizedHash);
        if (it != torrents_.end()) {
            it->download = download;
            it->savePath = path;
        }
    }
    
    setupTorrentCallbacks(normalizedHash, download);
    download->start();
    
    return true;
#else
    Q_UNUSED(path);
    QMutexLocker lock(&torrentsMutex_);
    torrents_.remove(normalizedHash);
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
    QMutexLocker lock(&torrentsMutex_);
    
    if (torrents_.isEmpty()) {
        QFile::remove(filePath);
        return true;
    }
    
    QJsonArray sessionsArray;
    
    for (const auto& torrent : torrents_) {
        // Only save incomplete torrents
        if (torrent.completed) {
            continue;
        }
        
        QJsonObject session;
        session["hash"] = torrent.hash;
        session["name"] = torrent.name;
        session["savePath"] = torrent.savePath;
        session["totalSize"] = torrent.totalSize;
        session["paused"] = torrent.paused;
        session["removeOnDone"] = torrent.removeOnDone;
        
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
        
        QString savePath = session["savePath"].toString();
        bool paused = session["paused"].toBool();
        bool removeOnDone = session["removeOnDone"].toBool();
        
        qInfo() << "TorrentClient: Restoring torrent:" << hash.left(8);
        
        if (downloadTorrent(hash, savePath)) {
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
    QMutexLocker lock(&torrentsMutex_);
    
    QStringList toRemove;
    
    for (auto it = torrents_.begin(); it != torrents_.end(); ++it) {
        const QString& hash = it.key();
        ActiveTorrent& torrent = it.value();
        
        if (!torrent.download) {
            continue;
        }
        
        // Update status from librats
        updateTorrentStatus(hash);
        
        // Emit progress JSON for API consumers
        emitProgressJson(hash, torrent);
        
        // Check for completion
        if (torrent.download->is_complete() && !torrent.completed) {
            torrent.completed = true;
            torrent.progress = 1.0;
            
            qInfo() << "TorrentClient: Download completed:" << torrent.name;
            
            // Emit on main thread
            QMetaObject::invokeMethod(this, [this, hash]() {
                emit downloadCompleted(hash);
            }, Qt::QueuedConnection);
            
            // Mark for removal if removeOnDone
            if (torrent.removeOnDone) {
                toRemove.append(hash);
            }
        }
    }
    
    lock.unlock();
    
    // Remove completed torrents marked for removal
    for (const QString& hash : toRemove) {
        stopTorrent(hash);
    }
}

// ============================================================================
// Private Methods
// ============================================================================

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

void TorrentClient::setupTorrentCallbacks(const QString& hash, std::shared_ptr<librats::TorrentDownload> download)
{
#ifdef RATS_SEARCH_FEATURES
    // Progress callback
    download->set_progress_callback([this, hash](uint64_t downloaded, uint64_t total, double percentage) {
        QMutexLocker lock(&torrentsMutex_);
        auto it = torrents_.find(hash);
        if (it != torrents_.end()) {
            it->downloadedBytes = static_cast<qint64>(downloaded);
            it->totalSize = static_cast<qint64>(total);
            it->progress = percentage / 100.0;
        }
        lock.unlock();
        
        // Emit progress on main thread
        QMetaObject::invokeMethod(this, [this, hash, percentage]() {
            emit downloadProgress(hash, static_cast<int>(percentage));
        }, Qt::QueuedConnection);
    });
    
    // Torrent complete callback
    download->set_torrent_complete_callback([this, hash](const std::string& /*name*/) {
        QMutexLocker lock(&torrentsMutex_);
        auto it = torrents_.find(hash);
        if (it != torrents_.end()) {
            it->completed = true;
            it->progress = 1.0;
        }
        lock.unlock();
        
        QMetaObject::invokeMethod(this, [this, hash]() {
            emit downloadCompleted(hash);
        }, Qt::QueuedConnection);
    });
    
    // Metadata complete callback (for torrents added by hash)
    download->set_metadata_complete_callback([this, hash](const librats::TorrentInfo& info) {
        QMutexLocker lock(&torrentsMutex_);
        auto it = torrents_.find(hash);
        if (it != torrents_.end() && !it->ready) {
            it->name = QString::fromStdString(info.get_name());
            it->totalSize = static_cast<qint64>(info.get_total_length());
            it->ready = true;
            
            // Populate files
            it->files.clear();
            const auto& files = info.get_files();
            for (size_t i = 0; i < files.size(); ++i) {
                TorrentFileInfo fi;
                fi.path = QString::fromStdString(files[i].path);
                fi.size = static_cast<qint64>(files[i].length);
                fi.index = static_cast<int>(i);
                fi.selected = true;
                it->files.append(fi);
            }
            
            QVector<TorrentFileInfo> filesCopy = it->files;
            
            // Build JSON array for API consumers
            QJsonArray filesJson;
            for (const TorrentFileInfo& f : filesCopy) {
                filesJson.append(f.toJson());
            }
            
            lock.unlock();
            
            // Emit on main thread
            QMetaObject::invokeMethod(this, [this, hash, filesCopy, filesJson]() {
                emit downloadStarted(hash);
                emit filesReady(hash, filesCopy);
                emit filesReadyJson(hash, filesJson);
            }, Qt::QueuedConnection);
        }
    });
    
    // Peer connected callback
    download->set_peer_connected_callback([this, hash](const librats::Peer& /*peer*/) {
        QMutexLocker lock(&torrentsMutex_);
        auto it = torrents_.find(hash);
        if (it != torrents_.end()) {
            it->peersConnected = static_cast<int>(it->download->get_peer_count());
        }
    });
    
    // Peer disconnected callback  
    download->set_peer_disconnected_callback([this, hash](const librats::Peer& /*peer*/) {
        QMutexLocker lock(&torrentsMutex_);
        auto it = torrents_.find(hash);
        if (it != torrents_.end()) {
            it->peersConnected = static_cast<int>(it->download->get_peer_count());
        }
    });
#else
    Q_UNUSED(hash);
    Q_UNUSED(download);
#endif
}

void TorrentClient::updateTorrentStatus(const QString& hash)
{
#ifdef RATS_SEARCH_FEATURES
    // Must be called with torrentsMutex_ held
    auto it = torrents_.find(hash);
    if (it == torrents_.end() || !it->download) {
        return;
    }
    
    ActiveTorrent& torrent = *it;
    
    torrent.downloadedBytes = static_cast<qint64>(torrent.download->get_downloaded_bytes());
    torrent.progress = torrent.download->get_progress_percentage() / 100.0;
    torrent.downloadSpeed = torrent.download->get_download_speed();
    torrent.peersConnected = static_cast<int>(torrent.download->get_peer_count());
    
    // Get total size if not set yet
    if (torrent.totalSize == 0) {
        const auto& info = torrent.download->get_torrent_info();
        if (info.is_valid()) {
            torrent.totalSize = static_cast<qint64>(info.get_total_length());
        }
    }
#else
    Q_UNUSED(hash);
#endif
}
