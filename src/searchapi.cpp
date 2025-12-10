#include "searchapi.h"
#include "p2pnetwork.h"
#include <QJsonDocument>
#include <QDebug>
#include <QtConcurrent>

SearchAPI::SearchAPI(TorrentDatabase* database, P2PNetwork* p2p, QObject *parent)
    : QObject(parent)
    , database_(database)
    , p2p_(p2p)
{
}

SearchAPI::~SearchAPI()
{
}

void SearchAPI::searchTorrent(const QString& text,
                               const QJsonObject& navigation,
                               TorrentsCallback callback)
{
    if (!database_) {
        if (callback) callback(QJsonArray());
        return;
    }

    SearchOptions options = jsonToSearchOptions(text, navigation);
    
    // Run search in background thread
    QtConcurrent::run([this, options, callback]() {
        QVector<TorrentInfo> results = database_->searchTorrents(options);
        
        QJsonArray torrents;
        for (const TorrentInfo& torrent : results) {
            torrents.append(torrentToJson(torrent));
        }
        
        if (callback) {
            QMetaObject::invokeMethod(this, [callback, torrents]() {
                callback(torrents);
            }, Qt::QueuedConnection);
        }
    });

    // Also search P2P network if available
    if (p2p_ && p2p_->isConnected()) {
        p2p_->searchTorrents(text);
    }
}

void SearchAPI::searchFiles(const QString& text,
                             const QJsonObject& navigation,
                             TorrentsCallback callback)
{
    if (!database_) {
        if (callback) callback(QJsonArray());
        return;
    }

    SearchOptions options = jsonToSearchOptions(text, navigation);
    
    QtConcurrent::run([this, options, callback]() {
        QVector<TorrentInfo> results = database_->searchFiles(options);
        
        QJsonArray torrents;
        for (const TorrentInfo& torrent : results) {
            QJsonObject json = torrentToJson(torrent);
            
            // Add matched file paths
            if (!torrent.filesList.isEmpty()) {
                QJsonArray paths;
                for (const TorrentFile& file : torrent.filesList) {
                    paths.append(file.path);
                }
                json["path"] = paths;
            }
            
            torrents.append(json);
        }
        
        if (callback) {
            QMetaObject::invokeMethod(this, [callback, torrents]() {
                callback(torrents);
            }, Qt::QueuedConnection);
        }
    });
}

void SearchAPI::getTorrent(const QString& hash,
                           bool includeFiles,
                           Callback callback)
{
    if (!database_) {
        if (callback) callback(QJsonObject());
        return;
    }

    QtConcurrent::run([this, hash, includeFiles, callback]() {
        TorrentInfo torrent = database_->getTorrent(hash, includeFiles);
        
        QJsonObject result;
        if (torrent.isValid()) {
            result = torrentToJson(torrent);
            
            if (includeFiles && !torrent.filesList.isEmpty()) {
                QJsonArray files;
                for (const TorrentFile& file : torrent.filesList) {
                    QJsonObject fileObj;
                    fileObj["path"] = file.path;
                    fileObj["size"] = file.size;
                    files.append(fileObj);
                }
                result["filesList"] = files;
            }
        }
        
        if (callback) {
            QMetaObject::invokeMethod(this, [callback, result]() {
                callback(result);
            }, Qt::QueuedConnection);
        }
    });
}

void SearchAPI::getRecentTorrents(int limit, TorrentsCallback callback)
{
    if (!database_) {
        if (callback) callback(QJsonArray());
        return;
    }

    QtConcurrent::run([this, limit, callback]() {
        QVector<TorrentInfo> results = database_->getRecentTorrents(limit);
        
        QJsonArray torrents;
        for (const TorrentInfo& torrent : results) {
            torrents.append(torrentToJson(torrent));
        }
        
        if (callback) {
            QMetaObject::invokeMethod(this, [callback, torrents]() {
                callback(torrents);
            }, Qt::QueuedConnection);
        }
    });
}

void SearchAPI::getTopTorrents(const QString& type,
                                const QJsonObject& navigation,
                                TorrentsCallback callback)
{
    if (!database_) {
        if (callback) callback(QJsonArray());
        return;
    }

    int index = navigation.value("index").toInt(0);
    int limit = navigation.value("limit").toInt(20);
    QString time = navigation.value("time").toString();

    QtConcurrent::run([this, type, time, index, limit, callback]() {
        QVector<TorrentInfo> results = database_->getTopTorrents(type, time, index, limit);
        
        QJsonArray torrents;
        for (const TorrentInfo& torrent : results) {
            torrents.append(torrentToJson(torrent));
        }
        
        if (callback) {
            QMetaObject::invokeMethod(this, [callback, torrents]() {
                callback(torrents);
            }, Qt::QueuedConnection);
        }
    });
}

void SearchAPI::getStatistics(Callback callback)
{
    if (!database_) {
        if (callback) callback(QJsonObject());
        return;
    }

    TorrentDatabase::Statistics stats = database_->getStatistics();
    
    QJsonObject result;
    result["torrents"] = stats.totalTorrents;
    result["files"] = stats.totalFiles;
    result["size"] = stats.totalSize;
    
    if (callback) {
        callback(result);
    }
}

void SearchAPI::getPeers(Callback callback)
{
    QJsonObject result;
    
    if (p2p_) {
        result["size"] = p2p_->getPeerCount();
        result["connected"] = p2p_->isConnected();
        // TODO: Add more peer info
        result["torrents"] = 0;  // Sum of peer torrents
    } else {
        result["size"] = 0;
        result["connected"] = false;
        result["torrents"] = 0;
    }
    
    if (callback) {
        callback(result);
    }
}

void SearchAPI::checkTrackers(const QString& hash)
{
    if (!database_ || hash.length() != 40) {
        return;
    }

    qInfo() << "Checking trackers for" << hash.left(8);
    
    // TODO: Implement UDP tracker checking
    // This would contact trackers to get seeders/leechers/completed
    // For now, emit a placeholder update
    
    emit trackerUpdate(hash, 0, 0, 0);
}

void SearchAPI::vote(const QString& hash, bool isGood, Callback callback)
{
    if (!database_ || hash.length() != 40) {
        if (callback) {
            QJsonObject result;
            result["success"] = false;
            result["error"] = "Invalid hash";
            callback(result);
        }
        return;
    }

    // Get current torrent
    TorrentInfo torrent = database_->getTorrent(hash);
    if (!torrent.isValid()) {
        if (callback) {
            QJsonObject result;
            result["success"] = false;
            result["error"] = "Torrent not found";
            callback(result);
        }
        return;
    }

    // Update votes
    if (isGood) {
        torrent.good++;
    } else {
        torrent.bad++;
    }

    // Save to database
    database_->updateTorrent(torrent);

    // Emit update
    emit votesUpdated(hash, torrent.good, torrent.bad);

    // Return result
    if (callback) {
        QJsonObject result;
        result["success"] = true;
        result["hash"] = hash;
        result["good"] = torrent.good;
        result["bad"] = torrent.bad;
        callback(result);
    }

    // TODO: Store vote in P2P store for distribution
}

QJsonObject SearchAPI::torrentToJson(const TorrentInfo& torrent)
{
    QJsonObject obj;
    obj["hash"] = torrent.hash;
    obj["name"] = torrent.name;
    obj["size"] = torrent.size;
    obj["files"] = torrent.files;
    obj["piecelength"] = torrent.piecelength;
    obj["added"] = torrent.added.isValid() ? torrent.added.toMSecsSinceEpoch() : 0;
    obj["contentType"] = torrent.contentTypeString();
    obj["contentCategory"] = torrent.contentCategoryString();
    obj["seeders"] = torrent.seeders;
    obj["leechers"] = torrent.leechers;
    obj["completed"] = torrent.completed;
    obj["trackersChecked"] = torrent.trackersChecked.isValid() 
                              ? torrent.trackersChecked.toMSecsSinceEpoch() : 0;
    obj["good"] = torrent.good;
    obj["bad"] = torrent.bad;
    
    if (!torrent.info.isEmpty()) {
        obj["info"] = torrent.info;
    }
    
    return obj;
}

SearchOptions SearchAPI::jsonToSearchOptions(const QString& query, const QJsonObject& navigation)
{
    SearchOptions options;
    options.query = query;
    options.index = navigation.value("index").toInt(0);
    options.limit = navigation.value("limit").toInt(10);
    options.orderBy = navigation.value("orderBy").toString();
    options.orderDesc = navigation.value("orderDesc").toBool(true);
    options.safeSearch = navigation.value("safeSearch").toBool(false);
    options.contentType = navigation.value("type").toString();
    
    QJsonObject size = navigation.value("size").toObject();
    if (!size.isEmpty()) {
        options.sizeMin = size.value("min").toVariant().toLongLong();
        options.sizeMax = size.value("max").toVariant().toLongLong();
    }
    
    QJsonObject files = navigation.value("files").toObject();
    if (!files.isEmpty()) {
        options.filesMin = files.value("min").toInt();
        options.filesMax = files.value("max").toInt();
    }
    
    return options;
}

