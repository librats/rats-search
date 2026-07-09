#include "services/download_service.h"

#include "common/infohash.h"
#include "services/torrent_session_store.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStandardPaths>

namespace rats::service {

// ============================================================================
// DownloadFile / Download — JSON
// ============================================================================

QJsonObject DownloadFile::toJson() const
{
    QJsonObject obj;
    obj["path"] = path;
    obj["size"] = size;
    obj["index"] = index;
    obj["selected"] = selected;
    obj["progress"] = progress;
    return obj;
}

QJsonObject Download::toJson() const
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
    for (const DownloadFile& f : files) {
        filesArr.append(f.toJson());
    }
    obj["files"] = filesArr;
    return obj;
}

// ============================================================================
// Construction
// ============================================================================

DownloadService::DownloadService(net::TorrentEngine* engine, QObject* parent)
    : QObject(parent)
    , engine_(engine)
    , sessionStore_(std::make_unique<TorrentSessionStore>())
    , defaultDownloadPath_(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation))
{
    updateTimer_ = new QTimer(this);
    updateTimer_->setInterval(1000); // poll once per second
    connect(updateTimer_, &QTimer::timeout, this, &DownloadService::onUpdateTimer);
    // Safe to start now: the poll is a no-op until the engine is ready.
    updateTimer_->start();
}

DownloadService::~DownloadService()
{
    if (updateTimer_) {
        updateTimer_->stop();
    }

    // Persist resume data for every active download so downloaded pieces survive
    // a restart. librats keeps ownership of the torrents; we only ask it to save.
    if (engine_) {
        QMutexLocker lock(&mutex_);
        for (const Download& d : downloads_) {
            engine_->saveResumeData(d.hash);
        }
    }

    QMutexLocker lock(&mutex_);
    downloads_.clear();
}

bool DownloadService::isReady() const
{
    return engine_ && engine_->isReady();
}

// ============================================================================
// Download lifecycle
// ============================================================================

bool DownloadService::add(const QString& magnetLink, const QString& savePath)
{
    if (!isReady()) {
        qWarning() << "DownloadService: Not ready";
        return false;
    }

    QString hash = net::TorrentEngine::parseInfoHash(magnetLink);
    if (!infohash::isValid(hash)) {
        qWarning() << "DownloadService: Invalid magnet link or hash:" << magnetLink;
        return false;
    }
    hash = infohash::normalize(hash);

    if (contains(hash)) {
        qInfo() << "DownloadService: Already downloading:" << hash;
        return false;
    }

    const QString path = resolveSavePath(savePath);
    ensureDir(path);

    qInfo() << "DownloadService: Adding torrent" << hash << "to" << path;
    if (!engine_->addMagnet(hash, path)) {
        qWarning() << "DownloadService: Failed to add magnet:" << hash;
        return false;
    }

    Download d;
    d.hash = hash;
    d.name = hash; // placeholder until metadata arrives
    d.savePath = path;
    d.added = true;
    d.ready = false;
    {
        QMutexLocker lock(&mutex_);
        downloads_[hash] = d;
    }

    emit downloadStarted(hash);
    return true;
}

bool DownloadService::addWithInfo(const domain::Torrent& info, const QString& savePath)
{
    if (!infohash::isValid(info.hash)) {
        qWarning() << "DownloadService: Invalid hash for download:" << info.hash;
        return false;
    }
    if (!isReady()) {
        qWarning() << "DownloadService: Not ready";
        return false;
    }

    const QString hash = infohash::normalize(info.hash);
    if (contains(hash)) {
        qInfo() << "DownloadService: Already downloading:" << hash;
        return false;
    }

    const QString path = resolveSavePath(savePath);
    ensureDir(path);

    qInfo() << "DownloadService: Adding torrent with info" << hash << info.name.left(50);
    if (!engine_->addMagnet(hash, path)) {
        qWarning() << "DownloadService: Failed to add magnet:" << hash;
        return false;
    }

    Download d;
    d.hash = hash;
    d.name = info.name.isEmpty() ? hash : info.name;
    d.totalSize = info.size;
    d.savePath = path;
    d.added = true;
    d.ready = false; // metadata (authoritative name/files) not yet known
    d.completed = false;
    {
        QMutexLocker lock(&mutex_);
        downloads_[hash] = d;
    }

    emit downloadStarted(hash);
    return true;
}

bool DownloadService::addFromFile(const QString& torrentFile, const QString& savePath)
{
    if (!isReady()) {
        qWarning() << "DownloadService: Not ready";
        return false;
    }

    // Parse up front so we know the hash (for dedup) and the file list.
    net::TorrentMetadata meta = engine_->readTorrentFile(torrentFile);
    if (!meta.valid) {
        qWarning() << "DownloadService: Failed to parse torrent file:" << torrentFile;
        return false;
    }

    const QString hash = infohash::normalize(meta.hash);
    if (contains(hash)) {
        qInfo() << "DownloadService: Already downloading:" << hash;
        return false;
    }

    const QString path = resolveSavePath(savePath);
    ensureDir(path);

    qInfo() << "DownloadService: Adding torrent file" << torrentFile << "to" << path;
    if (!engine_->addTorrentFile(torrentFile, path)) {
        qWarning() << "DownloadService: Failed to add torrent file:" << torrentFile;
        return false;
    }

    Download d;
    d.hash = hash;
    d.name = meta.name;
    d.savePath = path;
    d.totalSize = meta.totalSize;
    d.added = true;
    d.ready = true;
    for (int i = 0; i < meta.files.size(); ++i) {
        DownloadFile f;
        f.path = meta.files[i].path;
        f.size = meta.files[i].size;
        f.index = i;
        f.selected = true;
        d.files.append(f);
    }
    {
        QMutexLocker lock(&mutex_);
        downloads_[hash] = d;
    }

    emit downloadStarted(hash);
    emit filesReady(hash, filesToJson(d.files));
    return true;
}

bool DownloadService::restore(const QString& hash, const QString& name, const QString& savePath, bool wasCompleted)
{
    Q_UNUSED(wasCompleted);

    if (!isReady()) {
        qWarning() << "DownloadService: Not ready for restore";
        return false;
    }

    const QString h = infohash::normalize(hash);
    if (contains(h)) {
        qInfo() << "DownloadService: Already active:" << h.left(8);
        return false;
    }

    const QString path = resolveSavePath(savePath);
    ensureDir(path);

    qInfo() << "DownloadService: Restoring torrent" << h.left(8) << name.left(30) << "from" << path;
    if (!engine_->addMagnetResumed(h, path)) {
        qWarning() << "DownloadService: Failed to restore torrent:" << h.left(8);
        return false;
    }

    Download d;
    d.hash = h;
    d.savePath = path;
    d.name = name.isEmpty() ? h : name;
    d.added = true;
    d.ready = false;

    // If resume data already brought back the metadata, populate immediately.
    net::TorrentSnapshot snap = engine_->status(h);
    if (snap.exists && snap.hasMetadata) {
        applySnapshot(d, snap);
    }

    {
        QMutexLocker lock(&mutex_);
        downloads_[h] = d;
    }

    emit downloadStarted(h);
    if (d.ready) {
        emit filesReady(h, filesToJson(d.files));
        if (d.completed) {
            emit downloadCompleted(h);
        }
    }
    return true;
}

void DownloadService::remove(const QString& hash, bool saveResumeData)
{
    const QString h = infohash::normalize(hash);

    {
        QMutexLocker lock(&mutex_);
        auto it = downloads_.find(h);
        if (it == downloads_.end()) {
            qWarning() << "DownloadService: Torrent not found:" << h;
            return;
        }
        downloads_.erase(it);
    }

    if (engine_) {
        if (saveResumeData) {
            qInfo() << "DownloadService: Saving resume data for:" << h;
            engine_->saveResumeData(h);
        }
        engine_->remove(h);
    }

    emit torrentRemoved(h);
    qInfo() << "DownloadService: Stopped and removed torrent:" << h;
}

bool DownloadService::pause(const QString& hash)
{
    if (!engine_) {
        return false;
    }
    const QString h = infohash::normalize(hash);

    {
        QMutexLocker lock(&mutex_);
        auto it = downloads_.find(h);
        if (it == downloads_.end() || it->paused) {
            return false;
        }
        it->paused = true;
    }

    engine_->pause(h);
    emit stateChanged(h, QJsonObject { { "paused", true } });
    return true;
}

bool DownloadService::resume(const QString& hash)
{
    if (!engine_) {
        return false;
    }
    const QString h = infohash::normalize(hash);

    {
        QMutexLocker lock(&mutex_);
        auto it = downloads_.find(h);
        if (it == downloads_.end() || !it->paused) {
            return false;
        }
        it->paused = false;
    }

    engine_->resume(h);
    emit stateChanged(h, QJsonObject { { "paused", false } });
    return true;
}

bool DownloadService::togglePause(const QString& hash)
{
    const QString h = infohash::normalize(hash);

    bool isPaused = false;
    {
        QMutexLocker lock(&mutex_);
        auto it = downloads_.find(h);
        if (it == downloads_.end()) {
            return false;
        }
        isPaused = it->paused;
    }

    return isPaused ? resume(h) : pause(h);
}

bool DownloadService::selectFiles(const QString& hash, const QVector<bool>& selection)
{
    const QString h = infohash::normalize(hash);

    QMutexLocker lock(&mutex_);
    auto it = downloads_.find(h);
    if (it == downloads_.end()) {
        return false;
    }

    for (int i = 0; i < selection.size() && i < it->files.size(); ++i) {
        it->files[i].selected = selection[i];
    }

    // librats currently downloads all files; the selection is tracked locally
    // until it gains per-file selection support.
    return true;
}

bool DownloadService::selectFilesJson(const QString& hash, const QJsonValue& selection)
{
    const QString h = infohash::normalize(hash);

    QJsonArray filesArr;
    {
        QMutexLocker lock(&mutex_);
        auto it = downloads_.find(h);
        if (it == downloads_.end()) {
            return false;
        }
        Download& d = *it;

        if (selection.isArray()) {
            QJsonArray arr = selection.toArray();
            for (int i = 0; i < arr.size() && i < d.files.size(); ++i) {
                d.files[i].selected = arr[i].toBool(true);
            }
        } else if (selection.isObject()) {
            QJsonObject obj = selection.toObject();
            for (auto selIt = obj.begin(); selIt != obj.end(); ++selIt) {
                bool ok = false;
                int idx = selIt.key().toInt(&ok);
                if (ok && idx >= 0 && idx < d.files.size()) {
                    d.files[idx].selected = selIt.value().toBool(true);
                }
            }
        }

        filesArr = filesToJson(d.files);
    }

    emit filesReady(h, filesArr);
    return true;
}

void DownloadService::setRemoveOnDone(const QString& hash, bool removeOnDone)
{
    const QString h = infohash::normalize(hash);

    bool found = false;
    {
        QMutexLocker lock(&mutex_);
        auto it = downloads_.find(h);
        if (it != downloads_.end()) {
            it->removeOnDone = removeOnDone;
            found = true;
        }
    }

    if (found) {
        emit stateChanged(h, QJsonObject { { "removeOnDone", removeOnDone } });
    }
}

QString DownloadService::registerSeed(const net::SeedResult& seed)
{
    const QString hash = infohash::normalize(seed.hash);

    Download d;
    d.hash = hash;
    d.name = seed.name;
    d.savePath = seed.savePath;
    d.totalSize = seed.totalSize;
    d.downloadedBytes = seed.totalSize; // already complete for seeding
    d.progress = 1.0;
    d.added = true;
    d.ready = true;
    d.completed = true;
    for (int i = 0; i < seed.files.size(); ++i) {
        DownloadFile f;
        f.path = seed.files[i].path;
        f.size = seed.files[i].size;
        f.index = i;
        f.selected = true;
        f.progress = 1.0;
        d.files.append(f);
    }

    {
        QMutexLocker lock(&mutex_);
        if (downloads_.contains(hash)) {
            qInfo() << "DownloadService: Torrent already exists:" << hash;
            return hash; // existing hash — not an error
        }
        downloads_[hash] = d;
    }

    qInfo() << "DownloadService: Seeding torrent:" << d.name << "hash:" << hash.left(8);
    emit downloadStarted(hash);
    emit filesReady(hash, filesToJson(d.files));
    emit downloadCompleted(hash);
    return hash;
}

// ============================================================================
// Queries
// ============================================================================

bool DownloadService::isDownloading(const QString& hash) const
{
    return contains(infohash::normalize(hash));
}

Download DownloadService::getDownload(const QString& hash) const
{
    const QString h = infohash::normalize(hash);
    QMutexLocker lock(&mutex_);
    return downloads_.value(h);
}

QVector<Download> DownloadService::allDownloads() const
{
    QMutexLocker lock(&mutex_);
    QVector<Download> result;
    for (const Download& d : downloads_) {
        result.append(d);
    }
    return result;
}

int DownloadService::count() const
{
    QMutexLocker lock(&mutex_);
    return downloads_.size();
}

QJsonArray DownloadService::toJsonArray() const
{
    QMutexLocker lock(&mutex_);
    QJsonArray arr;
    for (const Download& d : downloads_) {
        arr.append(d.toJson());
    }
    return arr;
}

// ============================================================================
// Configuration
// ============================================================================

void DownloadService::setDefaultDownloadPath(const QString& path)
{
    defaultDownloadPath_ = path;
}

// ============================================================================
// Session persistence
// ============================================================================

bool DownloadService::saveSession(const QString& filePath)
{
    QVector<Download> snapshot;
    {
        QMutexLocker lock(&mutex_);
        for (const Download& d : downloads_) {
            snapshot.append(d);
        }
    }

    // Ask librats to persist resume data (downloaded pieces) for each torrent.
    if (engine_) {
        for (const Download& d : snapshot) {
            qInfo() << "DownloadService: Saving resume data for" << d.hash.left(8);
            engine_->saveResumeData(d.hash);
        }
    }

    return sessionStore_->save(filePath, snapshot);
}

int DownloadService::loadSession(const QString& filePath)
{
    QVector<Download> entries = sessionStore_->load(filePath);
    int restored = 0;

    for (const Download& e : entries) {
        if (!infohash::isValid(e.hash)) {
            continue;
        }

        qInfo() << "DownloadService: Restoring torrent:" << e.hash.left(8) << e.name.left(30)
                << (e.completed ? "(completed/seeding)" : "(downloading)");

        if (restore(e.hash, e.name, e.savePath, e.completed)) {
            if (e.paused) {
                pause(e.hash);
            }
            setRemoveOnDone(e.hash, e.removeOnDone);

            if (!e.files.isEmpty()) {
                QVector<bool> selection;
                for (const DownloadFile& f : e.files) {
                    selection.append(f.selected);
                }
                selectFiles(e.hash, selection);
            }
            restored++;
        }
    }

    if (restored > 0) {
        qInfo() << "DownloadService: Restored" << restored << "torrents from session";
    }
    return restored;
}

// ============================================================================
// Progress polling
// ============================================================================

void DownloadService::onUpdateTimer()
{
    if (!isReady()) {
        return;
    }
    Transitions t = pollStatus();
    flushTransitions(t);
}

DownloadService::Transitions DownloadService::pollStatus()
{
    Transitions t;

    // Snapshot the hashes to poll (don't hold the mutex across engine calls).
    QStringList hashes;
    {
        QMutexLocker lock(&mutex_);
        hashes = downloads_.keys();
    }

    for (const QString& hash : hashes) {
        // Thread-safe snapshot from the reactor thread (no mutex held).
        net::TorrentSnapshot snap = engine_->status(hash);

        QJsonObject progress;
        bool emitProgress = false;
        {
            QMutexLocker lock(&mutex_);
            auto it = downloads_.find(hash);
            if (it == downloads_.end()) {
                continue;
            }
            Download& d = *it;

            if (!snap.exists) {
                continue; // librats no longer tracks it — leave as-is
            }

            const bool wasReady = d.ready;
            const bool wasCompleted = d.completed;

            if (snap.hasMetadata && !wasReady) {
                // Metadata just arrived for a magnet torrent → populate details.
                applySnapshot(d, snap);
                t.newlyReady.append(hash);
            } else {
                // Live counters only.
                d.downloadedBytes = snap.downloaded;
                d.peersConnected = snap.numPeers;
                d.progress = snap.progress;
                if (d.totalSize == 0 && snap.totalSize > 0) {
                    d.totalSize = snap.totalSize;
                }
            }

            computeSpeed(d, QDateTime::currentMSecsSinceEpoch());

            if (snap.isComplete && !wasCompleted) {
                d.completed = true;
                d.progress = 1.0;
                d.downloadSpeed = 0.0;
                t.newlyCompleted.append(hash);
                if (d.removeOnDone) {
                    t.toRemove.append(hash);
                }
            }

            progress = progressJson(d);
            // Only broadcast when the snapshot actually changed; an idle, paused
            // or finished-seeding download otherwise re-emits an identical payload
            // to every WebSocket client every second.
            if (progress != d.lastProgress) {
                d.lastProgress = progress;
                emitProgress = true;
            }
        }

        if (emitProgress) {
            emit progressUpdated(hash, progress);
        }
    }

    return t;
}

void DownloadService::computeSpeed(Download& d, qint64 nowMs)
{
    if (d.lastSampledMs > 0 && nowMs > d.lastSampledMs) {
        const qint64 deltaBytes = d.downloadedBytes - d.lastSampledBytes;
        const double deltaSec = (nowMs - d.lastSampledMs) / 1000.0;
        d.downloadSpeed = deltaBytes > 0 ? deltaBytes / deltaSec : 0.0;
    }
    d.lastSampledBytes = d.downloadedBytes;
    d.lastSampledMs = nowMs;
}

void DownloadService::flushTransitions(const Transitions& t)
{
    for (const QString& hash : t.newlyReady) {
        QVector<DownloadFile> filesCopy;
        {
            QMutexLocker lock(&mutex_);
            auto it = downloads_.find(hash);
            if (it != downloads_.end()) {
                filesCopy = it->files;
            }
        }
        qInfo() << "DownloadService: Metadata received for:" << hash.left(8);
        emit downloadStarted(hash);
        emit filesReady(hash, filesToJson(filesCopy));
    }

    for (const QString& hash : t.newlyCompleted) {
        qInfo() << "DownloadService: Download completed:" << hash.left(8);
        emit downloadCompleted(hash);
    }

    for (const QString& hash : t.toRemove) {
        remove(hash);
    }
}

// ============================================================================
// Private helpers
// ============================================================================

void DownloadService::applySnapshot(Download& d, const net::TorrentSnapshot& snap)
{
    if (!snap.name.isEmpty()) {
        d.name = snap.name;
    }
    d.totalSize = snap.totalSize;
    d.downloadedBytes = snap.downloaded;
    d.progress = snap.progress;
    d.peersConnected = snap.numPeers;
    d.completed = snap.isComplete;
    d.ready = snap.hasMetadata;

    d.files.clear();
    for (int i = 0; i < snap.files.size(); ++i) {
        DownloadFile f;
        f.path = snap.files[i].path;
        f.size = snap.files[i].size;
        f.index = i;
        f.selected = true;
        if (snap.isComplete) {
            f.progress = 1.0;
        }
        d.files.append(f);
    }
}

QString DownloadService::resolveSavePath(const QString& savePath) const
{
    return savePath.isEmpty() ? defaultDownloadPath_ : savePath;
}

bool DownloadService::ensureDir(const QString& path)
{
    QDir dir(path);
    if (dir.exists()) {
        return true;
    }
    return dir.mkpath(".");
}

bool DownloadService::contains(const QString& hash) const
{
    QMutexLocker lock(&mutex_);
    return downloads_.contains(hash);
}

QJsonArray DownloadService::filesToJson(const QVector<DownloadFile>& files)
{
    QJsonArray arr;
    for (const DownloadFile& f : files) {
        arr.append(f.toJson());
    }
    return arr;
}

QJsonObject DownloadService::progressJson(const Download& d)
{
    QJsonObject o;
    o["received"] = d.downloadedBytes;
    o["downloaded"] = d.downloadedBytes;
    o["total"] = d.totalSize;
    o["progress"] = d.progress;
    o["speed"] = static_cast<int>(d.downloadSpeed);
    o["downloadSpeed"] = static_cast<int>(d.downloadSpeed);
    o["paused"] = d.paused;
    o["removeOnDone"] = d.removeOnDone;
    if (d.downloadSpeed > 0 && d.totalSize > d.downloadedBytes) {
        o["timeRemaining"] = static_cast<qint64>((d.totalSize - d.downloadedBytes) / d.downloadSpeed);
    } else {
        o["timeRemaining"] = 0;
    }
    return o;
}

} // namespace rats::service
