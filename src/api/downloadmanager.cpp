#include "downloadmanager.h"
#include "../torrentclient.h"
#include "../torrentdatabase.h"
#include <QStandardPaths>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDir>

// ============================================================================
// DownloadFile
// ============================================================================

QJsonObject DownloadFile::toJson() const
{
    QJsonObject obj;
    obj["path"] = path;
    obj["size"] = size;
    obj["downloadIndex"] = index;
    obj["downloadSelected"] = selected;
    return obj;
}

// ============================================================================
// DownloadInfo
// ============================================================================

QJsonObject DownloadInfo::toJson() const
{
    QJsonObject obj;
    obj["hash"] = hash;
    obj["name"] = name;
    obj["size"] = size;
    obj["savePath"] = savePath;
    obj["received"] = received;
    obj["downloaded"] = downloaded;
    obj["progress"] = progress;
    obj["downloadSpeed"] = downloadSpeed;
    obj["timeRemaining"] = timeRemaining;
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

QJsonObject DownloadInfo::toProgressJson() const
{
    QJsonObject obj;
    obj["received"] = received;
    obj["downloaded"] = downloaded;
    obj["progress"] = progress;
    obj["downloadSpeed"] = downloadSpeed;
    obj["timeRemaining"] = timeRemaining;
    obj["paused"] = paused;
    obj["removeOnDone"] = removeOnDone;
    return obj;
}

// ============================================================================
// DownloadManager
// ============================================================================

DownloadManager::DownloadManager(TorrentClient* client, 
                                 TorrentDatabase* database,
                                 QObject *parent)
    : QObject(parent)
    , client_(client)
    , database_(database)
    , downloadPath_(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation))
{
    if (client_) {
        connectClientSignals();
    }
}

DownloadManager::~DownloadManager() = default;

void DownloadManager::setDownloadPath(const QString& path)
{
    downloadPath_ = path;
}

QString DownloadManager::downloadPath() const
{
    return downloadPath_;
}

void DownloadManager::connectClientSignals()
{
    connect(client_, &TorrentClient::downloadStarted,
            this, &DownloadManager::onTorrentReady);
    connect(client_, &TorrentClient::downloadProgress,
            this, [this](const QString& hash, int progress) {
        // Convert simple progress to detailed progress
        if (downloads_.contains(hash)) {
            DownloadInfo& info = downloads_[hash];
            info.progress = progress / 100.0;
            emit progressUpdated(hash, info.toProgressJson());
        }
    });
    connect(client_, &TorrentClient::downloadCompleted,
            this, &DownloadManager::onTorrentCompleted);
    connect(client_, &TorrentClient::downloadFailed,
            this, &DownloadManager::onTorrentError);
}

// ============================================================================
// Download Operations
// ============================================================================

bool DownloadManager::add(const QString& hash, const QString& savePath)
{
    if (hash.length() != 40) {
        qWarning() << "Invalid hash for download:" << hash;
        return false;
    }
    
    if (downloads_.contains(hash)) {
        qInfo() << "Already downloading:" << hash;
        return false;
    }
    
    // Get torrent info from database
    QString name = hash;
    qint64 size = 0;
    if (database_) {
        TorrentInfo torrent = database_->getTorrent(hash);
        if (torrent.isValid()) {
            name = torrent.name;
            size = torrent.size;
        }
    }
    
    return addWithInfo(hash, name, size, savePath);
}

bool DownloadManager::addWithInfo(const QString& hash, 
                                   const QString& name,
                                   qint64 size,
                                   const QString& savePath)
{
    if (hash.length() != 40) {
        return false;
    }
    
    if (downloads_.contains(hash)) {
        return false;
    }
    
    QString path = savePath.isEmpty() ? downloadPath_ : savePath;
    
    // Create download info
    DownloadInfo info;
    info.hash = hash;
    info.name = name;
    info.size = size;
    info.savePath = path;
    info.ready = false;
    info.completed = false;
    
    downloads_[hash] = info;
    
    // Start download via client
    if (client_) {
        QString magnet = QString("magnet:?xt=urn:btih:%1").arg(hash);
        client_->downloadTorrent(magnet);
    }
    
    qInfo() << "Download added:" << hash << name;
    return true;
}

bool DownloadManager::cancel(const QString& hash)
{
    if (!downloads_.contains(hash)) {
        return false;
    }
    
    if (client_) {
        client_->stopTorrent(hash);
    }
    
    downloads_.remove(hash);
    emit downloadCancelled(hash);
    
    qInfo() << "Download cancelled:" << hash;
    return true;
}

bool DownloadManager::pause(const QString& hash)
{
    if (!downloads_.contains(hash)) {
        return false;
    }
    
    DownloadInfo& info = downloads_[hash];
    if (info.paused) {
        return true;  // Already paused
    }
    
    info.paused = true;
    
    // TODO: Tell client to pause
    
    emit stateChanged(hash, QJsonObject{{"paused", true}});
    return true;
}

bool DownloadManager::resume(const QString& hash)
{
    if (!downloads_.contains(hash)) {
        return false;
    }
    
    DownloadInfo& info = downloads_[hash];
    if (!info.paused) {
        return true;  // Already running
    }
    
    info.paused = false;
    
    // TODO: Tell client to resume
    
    emit stateChanged(hash, QJsonObject{{"paused", false}});
    return true;
}

bool DownloadManager::togglePause(const QString& hash)
{
    if (!downloads_.contains(hash)) {
        return false;
    }
    
    DownloadInfo& info = downloads_[hash];
    if (info.paused) {
        return resume(hash);
    } else {
        return pause(hash);
    }
}

bool DownloadManager::setRemoveOnDone(const QString& hash, bool remove)
{
    if (!downloads_.contains(hash)) {
        return false;
    }
    
    downloads_[hash].removeOnDone = remove;
    emit stateChanged(hash, QJsonObject{{"removeOnDone", remove}});
    return true;
}

bool DownloadManager::toggleRemoveOnDone(const QString& hash)
{
    if (!downloads_.contains(hash)) {
        return false;
    }
    
    bool newVal = !downloads_[hash].removeOnDone;
    return setRemoveOnDone(hash, newVal);
}

bool DownloadManager::selectFiles(const QString& hash, const QJsonValue& selection)
{
    if (!downloads_.contains(hash)) {
        return false;
    }
    
    DownloadInfo& info = downloads_[hash];
    
    if (selection.isArray()) {
        QJsonArray arr = selection.toArray();
        if (arr.size() != info.files.size()) {
            qWarning() << "File selection size mismatch";
            return false;
        }
        
        for (int i = 0; i < arr.size() && i < info.files.size(); ++i) {
            info.files[i].selected = arr[i].toBool(true);
        }
    } else if (selection.isObject()) {
        QJsonObject obj = selection.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            bool ok;
            int idx = it.key().toInt(&ok);
            if (ok && idx >= 0 && idx < info.files.size()) {
                info.files[idx].selected = it.value().toBool(true);
            }
        }
    }
    
    // TODO: Tell client about selection change
    
    // Emit files ready with updated selection
    QJsonArray filesArr;
    for (const DownloadFile& f : info.files) {
        filesArr.append(f.toJson());
    }
    emit filesReady(hash, filesArr);
    
    return true;
}

// ============================================================================
// Query Operations
// ============================================================================

bool DownloadManager::isDownloading(const QString& hash) const
{
    return downloads_.contains(hash);
}

DownloadInfo DownloadManager::getDownload(const QString& hash) const
{
    return downloads_.value(hash);
}

QVector<DownloadInfo> DownloadManager::getAllDownloads() const
{
    return QVector<DownloadInfo>(downloads_.values().begin(), downloads_.values().end());
}

QJsonArray DownloadManager::toJsonArray() const
{
    QJsonArray arr;
    for (const DownloadInfo& info : downloads_) {
        arr.append(info.toJson());
    }
    return arr;
}

int DownloadManager::count() const
{
    return downloads_.size();
}

// ============================================================================
// Slots
// ============================================================================

void DownloadManager::onTorrentReady(const QString& hash)
{
    if (!downloads_.contains(hash)) {
        return;
    }
    
    DownloadInfo& info = downloads_[hash];
    info.ready = true;
    
    emit downloadStarted(hash);
    
    // Emit files ready
    QJsonArray filesArr;
    for (const DownloadFile& f : info.files) {
        filesArr.append(f.toJson());
    }
    emit filesReady(hash, filesArr);
}

void DownloadManager::onTorrentProgress(const QString& hash, qint64 downloaded, qint64 total, int speed)
{
    if (!downloads_.contains(hash)) {
        return;
    }
    
    DownloadInfo& info = downloads_[hash];
    info.downloaded = downloaded;
    info.size = total;
    info.downloadSpeed = speed;
    info.progress = total > 0 ? static_cast<double>(downloaded) / total : 0.0;
    
    if (speed > 0 && total > downloaded) {
        info.timeRemaining = (total - downloaded) / speed;
    }
    
    emit progressUpdated(hash, info.toProgressJson());
}

void DownloadManager::onTorrentCompleted(const QString& hash)
{
    if (!downloads_.contains(hash)) {
        return;
    }
    
    DownloadInfo& info = downloads_[hash];
    info.completed = true;
    info.progress = 1.0;
    
    emit downloadCompleted(hash);
    
    if (info.removeOnDone) {
        downloads_.remove(hash);
    }
}

void DownloadManager::onTorrentError(const QString& hash, const QString& error)
{
    qWarning() << "Download error for" << hash << ":" << error;
    emit downloadError(hash, error);
}

// ============================================================================
// Session Persistence
// ============================================================================

bool DownloadManager::saveSession(const QString& filePath)
{
    if (downloads_.isEmpty()) {
        // No downloads to save - remove old session file if exists
        QFile::remove(filePath);
        return true;
    }
    
    QJsonArray sessionsArray;
    
    for (auto it = downloads_.constBegin(); it != downloads_.constEnd(); ++it) {
        const DownloadInfo& info = it.value();
        
        // Only save incomplete downloads
        if (info.completed) {
            continue;
        }
        
        QJsonObject session;
        session["hash"] = info.hash;
        session["name"] = info.name;
        session["size"] = info.size;
        session["savePath"] = info.savePath;
        session["paused"] = info.paused;
        session["removeOnDone"] = info.removeOnDone;
        session["downloaded"] = info.downloaded;
        session["progress"] = info.progress;
        
        // Save file selection
        QJsonArray filesArr;
        for (const DownloadFile& f : info.files) {
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
        qWarning() << "Failed to save download session to" << filePath;
        return false;
    }
    
    QJsonDocument doc(sessionsArray);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    qInfo() << "Saved" << sessionsArray.size() << "downloads to session file";
    return true;
}

int DownloadManager::restoreSession(const QString& filePath)
{
    QFile file(filePath);
    if (!file.exists()) {
        return 0;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open download session file:" << filePath;
        return 0;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse download session file:" << parseError.errorString();
        return 0;
    }
    
    if (!doc.isArray()) {
        qWarning() << "Invalid download session file format";
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
        
        // Check if already downloading
        if (downloads_.contains(hash)) {
            continue;
        }
        
        QString name = session["name"].toString();
        qint64 size = session["size"].toVariant().toLongLong();
        QString savePath = session["savePath"].toString();
        bool paused = session["paused"].toBool();
        bool removeOnDone = session["removeOnDone"].toBool();
        
        qInfo() << "Restoring download:" << name.left(50) << "hash:" << hash.left(8);
        
        // Add the download
        if (addWithInfo(hash, name, size, savePath)) {
            DownloadInfo& info = downloads_[hash];
            info.paused = paused;
            info.removeOnDone = removeOnDone;
            
            // Restore file selection
            QJsonArray filesArr = session["files"].toArray();
            if (!filesArr.isEmpty() && info.files.isEmpty()) {
                // Populate files from session
                for (const QJsonValue& fVal : filesArr) {
                    QJsonObject fObj = fVal.toObject();
                    DownloadFile df;
                    df.path = fObj["path"].toString();
                    df.size = fObj["size"].toVariant().toLongLong();
                    df.index = fObj["index"].toInt();
                    df.selected = fObj["selected"].toBool(true);
                    info.files.append(df);
                }
            }
            
            // If was paused, pause it
            if (paused && client_) {
                pause(hash);
            }
            
            restored++;
        }
    }
    
    if (restored > 0) {
        qInfo() << "Restored" << restored << "downloads from session";
    }
    
    return restored;
}
