#include "ratsapi.h"
#include "configmanager.h"
#include "feedmanager.h"
#include "downloadmanager.h"
#include "../torrentdatabase.h"
#include "../p2pnetwork.h"
#include "../torrentclient.h"

#include <QDebug>
#include <QtConcurrent>
#include <QJsonDocument>

// ============================================================================
// Helper functions (declared first for use throughout)
// ============================================================================

static QJsonObject torrentInfoToJson(const TorrentInfo& torrent)
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

// ============================================================================
// Private implementation
// ============================================================================

class RatsAPI::Private {
public:
    TorrentDatabase* database = nullptr;
    P2PNetwork* p2p = nullptr;
    TorrentClient* torrentClient = nullptr;
    ConfigManager* config = nullptr;
    
    std::unique_ptr<DownloadManager> downloadManager;
    std::unique_ptr<FeedManager> feedManager;
    
    // Top torrents cache
    QHash<QString, QJsonArray> topCache;
    QDateTime topCacheExpiry;
    
    bool ready = false;
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

RatsAPI::RatsAPI(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    registerMethods();
}

RatsAPI::~RatsAPI() = default;

void RatsAPI::initialize(TorrentDatabase* database,
                         P2PNetwork* p2p,
                         TorrentClient* torrentClient,
                         ConfigManager* config)
{
    d->database = database;
    d->p2p = p2p;
    d->torrentClient = torrentClient;
    d->config = config;
    
    // Initialize download manager
    if (torrentClient && database) {
        d->downloadManager = std::make_unique<DownloadManager>(torrentClient, database, this);
        
        // Forward download signals
        connect(d->downloadManager.get(), &DownloadManager::downloadStarted,
                this, [this](const QString& hash) {
            emit downloadProgress(hash, {{"status", "started"}});
        });
        connect(d->downloadManager.get(), &DownloadManager::progressUpdated,
                this, &RatsAPI::downloadProgress);
        connect(d->downloadManager.get(), &DownloadManager::filesReady,
                this, &RatsAPI::filesReady);
        connect(d->downloadManager.get(), &DownloadManager::downloadCompleted,
                this, [this](const QString& hash) {
            emit downloadCompleted(hash, false);
        });
        connect(d->downloadManager.get(), &DownloadManager::downloadCancelled,
                this, [this](const QString& hash) {
            emit downloadCompleted(hash, true);
        });
    }
    
    // Initialize feed manager
    if (database) {
        d->feedManager = std::make_unique<FeedManager>(database, this);
        d->feedManager->load();
        
        connect(d->feedManager.get(), &FeedManager::feedUpdated,
                this, [this]() {
            emit feedUpdated(d->feedManager->toJsonArray());
        });
    }
    
    // Forward config changes
    if (config) {
        connect(config, &ConfigManager::configChanged,
                this, [this](const QStringList& /*keys*/) {
            emit configChanged(d->config->toJson());
        });
    }
    
    d->ready = true;
    qInfo() << "RatsAPI initialized";
}

bool RatsAPI::isReady() const
{
    return d->ready;
}

// ============================================================================
// Method Registration (for generic call routing)
// ============================================================================

void RatsAPI::registerMethods()
{
    // Search methods
    methods_["search.torrents"] = [this](const QJsonObject& params, ApiCallback cb) {
        searchTorrents(params["text"].toString(), params, cb);
    };
    methods_["search.files"] = [this](const QJsonObject& params, ApiCallback cb) {
        searchFiles(params["text"].toString(), params, cb);
    };
    methods_["search.torrent"] = [this](const QJsonObject& params, ApiCallback cb) {
        getTorrent(params["hash"].toString(), 
                   params["files"].toBool(false),
                   params["peer"].toString(), cb);
    };
    methods_["search.recent"] = [this](const QJsonObject& params, ApiCallback cb) {
        getRecentTorrents(params["limit"].toInt(10), cb);
    };
    methods_["search.top"] = [this](const QJsonObject& params, ApiCallback cb) {
        getTopTorrents(params["type"].toString(), params, cb);
    };
    
    // Download methods
    methods_["downloads.add"] = [this](const QJsonObject& params, ApiCallback cb) {
        downloadAdd(params["hash"].toString(), params["savePath"].toString(), cb);
    };
    methods_["downloads.cancel"] = [this](const QJsonObject& params, ApiCallback cb) {
        downloadCancel(params["hash"].toString(), cb);
    };
    methods_["downloads.update"] = [this](const QJsonObject& params, ApiCallback cb) {
        downloadUpdate(params["hash"].toString(), params, cb);
    };
    methods_["downloads.selectFiles"] = [this](const QJsonObject& params, ApiCallback cb) {
        downloadSelectFiles(params["hash"].toString(), params["files"].toArray(), cb);
    };
    methods_["downloads.list"] = [this](const QJsonObject& /*params*/, ApiCallback cb) {
        getDownloads(cb);
    };
    
    // Statistics methods
    methods_["stats.database"] = [this](const QJsonObject& /*params*/, ApiCallback cb) {
        getStatistics(cb);
    };
    methods_["stats.peers"] = [this](const QJsonObject& /*params*/, ApiCallback cb) {
        getPeers(cb);
    };
    methods_["stats.p2pStatus"] = [this](const QJsonObject& /*params*/, ApiCallback cb) {
        getP2PStatus(cb);
    };
    
    // Config methods
    methods_["config.get"] = [this](const QJsonObject& /*params*/, ApiCallback cb) {
        getConfig(cb);
    };
    methods_["config.set"] = [this](const QJsonObject& params, ApiCallback cb) {
        setConfig(params, cb);
    };
    
    // Torrent operations
    methods_["torrent.vote"] = [this](const QJsonObject& params, ApiCallback cb) {
        vote(params["hash"].toString(), params["isGood"].toBool(true), cb);
    };
    methods_["torrent.checkTrackers"] = [this](const QJsonObject& params, ApiCallback cb) {
        checkTrackers(params["hash"].toString(), cb);
    };
    methods_["torrent.remove"] = [this](const QJsonObject& params, ApiCallback cb) {
        removeTorrents(params["checkOnly"].toBool(false), cb);
    };
    
    // Feed
    methods_["feed.get"] = [this](const QJsonObject& params, ApiCallback cb) {
        getFeed(params["index"].toInt(0), params["limit"].toInt(20), cb);
    };
}

void RatsAPI::call(const QString& method,
                   const QJsonObject& params,
                   ApiCallback callback,
                   const QString& requestId)
{
    auto it = methods_.find(method);
    if (it == methods_.end()) {
        ApiResponse resp = ApiResponse::fail("Unknown method: " + method);
        resp.requestId = requestId;
        if (callback) callback(resp);
        return;
    }
    
    // Wrap callback to add requestId
    ApiCallback wrappedCb = [callback, requestId](const ApiResponse& resp) {
        ApiResponse r = resp;
        r.requestId = requestId;
        if (callback) callback(r);
    };
    
    (*it)(params, wrappedCb);
}

QStringList RatsAPI::availableMethods() const
{
    return methods_.keys();
}

// ============================================================================
// Search API Implementation
// ============================================================================

void RatsAPI::searchTorrents(const QString& text,
                              const QJsonObject& options,
                              ApiCallback callback)
{
    if (!d->database) {
        if (callback) callback(ApiResponse::fail("Database not initialized"));
        return;
    }
    
    if (text.length() <= 2) {
        if (callback) callback(ApiResponse::fail("Query too short"));
        return;
    }
    
    SearchOptions opts;
    opts.query = text;
    opts.index = options["index"].toInt(0);
    opts.limit = options["limit"].toInt(10);
    opts.orderBy = options["orderBy"].toString();
    opts.orderDesc = options["orderDesc"].toBool(true);
    opts.safeSearch = options["safeSearch"].toBool(false);
    opts.contentType = options["type"].toString();
    
    QJsonObject size = options["size"].toObject();
    if (!size.isEmpty()) {
        opts.sizeMin = size["min"].toVariant().toLongLong();
        opts.sizeMax = size["max"].toVariant().toLongLong();
    }
    
    QJsonObject files = options["files"].toObject();
    if (!files.isEmpty()) {
        opts.filesMin = files["min"].toInt();
        opts.filesMax = files["max"].toInt();
    }
    
    // Run in background
    (void)QtConcurrent::run([this, opts, callback]() {
        QVector<TorrentInfo> results = d->database->searchTorrents(opts);
        
        QJsonArray torrents;
        for (const TorrentInfo& t : results) {
            torrents.append(torrentInfoToJson(t));
        }
        
        if (callback) {
            QMetaObject::invokeMethod(this, [callback, torrents]() {
                callback(ApiResponse::ok(torrents));
            }, Qt::QueuedConnection);
        }
    });
    
    // Also search P2P network if available
    if (d->p2p && d->p2p->isConnected()) {
        d->p2p->searchTorrents(text);
    }
}

void RatsAPI::searchFiles(const QString& text,
                           const QJsonObject& options,
                           ApiCallback callback)
{
    if (!d->database) {
        if (callback) callback(ApiResponse::fail("Database not initialized"));
        return;
    }
    
    if (text.length() <= 2) {
        if (callback) callback(ApiResponse::fail("Query too short"));
        return;
    }
    
    SearchOptions opts;
    opts.query = text;
    opts.index = options["index"].toInt(0);
    opts.limit = options["limit"].toInt(10);
    opts.orderBy = options["orderBy"].toString();
    opts.orderDesc = options["orderDesc"].toBool(true);
    opts.safeSearch = options["safeSearch"].toBool(false);
    
    (void)QtConcurrent::run([this, opts, callback]() {
        QVector<TorrentInfo> results = d->database->searchFiles(opts);
        
        QJsonArray torrents;
        for (const TorrentInfo& t : results) {
            QJsonObject obj = torrentInfoToJson(t);
            
            // Add file paths
            if (!t.filesList.isEmpty()) {
                QJsonArray paths;
                for (const TorrentFile& f : t.filesList) {
                    paths.append(f.path);
                }
                obj["path"] = paths;
            }
            torrents.append(obj);
        }
        
        if (callback) {
            QMetaObject::invokeMethod(this, [callback, torrents]() {
                callback(ApiResponse::ok(torrents));
            }, Qt::QueuedConnection);
        }
    });
}

void RatsAPI::getTorrent(const QString& hash,
                          bool includeFiles,
                          const QString& remotePeer,
                          ApiCallback callback)
{
    if (hash.length() != 40) {
        if (callback) callback(ApiResponse::fail("Invalid hash"));
        return;
    }
    
    if (!d->database) {
        if (callback) callback(ApiResponse::fail("Database not initialized"));
        return;
    }
    
    // TODO: Handle remote peer request via P2P
    Q_UNUSED(remotePeer);
    
    (void)QtConcurrent::run([this, hash, includeFiles, callback]() {
        TorrentInfo torrent = d->database->getTorrent(hash, includeFiles);
        
        if (!torrent.isValid()) {
            if (callback) {
                QMetaObject::invokeMethod(this, [callback]() {
                    callback(ApiResponse::fail("Torrent not found"));
                }, Qt::QueuedConnection);
            }
            return;
        }
        
        QJsonObject result = torrentInfoToJson(torrent);
        
        if (includeFiles && !torrent.filesList.isEmpty()) {
            QJsonArray files;
            for (const TorrentFile& f : torrent.filesList) {
                QJsonObject fileObj;
                fileObj["path"] = f.path;
                fileObj["size"] = f.size;
                files.append(fileObj);
            }
            result["filesList"] = files;
        }
        
        // Merge with download info if downloading
        if (d->downloadManager && d->downloadManager->isDownloading(hash)) {
            DownloadInfo dl = d->downloadManager->getDownload(hash);
            result["download"] = dl.toProgressJson();
        }
        
        if (callback) {
            QMetaObject::invokeMethod(this, [callback, result]() {
                callback(ApiResponse::ok(result));
            }, Qt::QueuedConnection);
        }
    });
}

void RatsAPI::getRecentTorrents(int limit, ApiCallback callback)
{
    if (!d->database) {
        if (callback) callback(ApiResponse::fail("Database not initialized"));
        return;
    }
    
    (void)QtConcurrent::run([this, limit, callback]() {
        QVector<TorrentInfo> results = d->database->getRecentTorrents(limit);
        
        QJsonArray torrents;
        for (const TorrentInfo& t : results) {
            torrents.append(torrentInfoToJson(t));
        }
        
        if (callback) {
            QMetaObject::invokeMethod(this, [callback, torrents]() {
                callback(ApiResponse::ok(torrents));
            }, Qt::QueuedConnection);
        }
    });
}

void RatsAPI::getTopTorrents(const QString& type,
                              const QJsonObject& options,
                              ApiCallback callback)
{
    if (!d->database) {
        if (callback) callback(ApiResponse::fail("Database not initialized"));
        return;
    }
    
    int index = options["index"].toInt(0);
    int limit = options["limit"].toInt(20);
    QString time = options["time"].toString();
    
    // Check cache
    QString cacheKey = QString("%1_%2_%3_%4").arg(type, time, QString::number(index), QString::number(limit));
    if (d->topCacheExpiry.isValid() && d->topCacheExpiry > QDateTime::currentDateTime()) {
        auto it = d->topCache.find(cacheKey);
        if (it != d->topCache.end()) {
            if (callback) callback(ApiResponse::ok(*it));
            return;
        }
    }
    
    (void)QtConcurrent::run([this, type, time, index, limit, cacheKey, callback]() {
        QVector<TorrentInfo> results = d->database->getTopTorrents(type, time, index, limit);
        
        QJsonArray torrents;
        for (const TorrentInfo& t : results) {
            torrents.append(torrentInfoToJson(t));
        }
        
        // Update cache
        d->topCache[cacheKey] = torrents;
        d->topCacheExpiry = QDateTime::currentDateTime().addSecs(86400);  // 24h cache
        
        if (callback) {
            QMetaObject::invokeMethod(this, [callback, torrents]() {
                callback(ApiResponse::ok(torrents));
            }, Qt::QueuedConnection);
        }
    });
}

// ============================================================================
// Download API Implementation
// ============================================================================

void RatsAPI::downloadAdd(const QString& hash,
                           const QString& savePath,
                           ApiCallback callback)
{
    if (hash.length() != 40) {
        if (callback) callback(ApiResponse::fail("Invalid hash"));
        return;
    }
    
    if (!d->downloadManager) {
        if (callback) callback(ApiResponse::fail("Download manager not initialized"));
        return;
    }
    
    bool ok = d->downloadManager->add(hash, savePath);
    if (callback) {
        callback(ok ? ApiResponse::ok() : ApiResponse::fail("Failed to start download"));
    }
}

void RatsAPI::downloadCancel(const QString& hash, ApiCallback callback)
{
    if (!d->downloadManager) {
        if (callback) callback(ApiResponse::fail("Download manager not initialized"));
        return;
    }
    
    bool ok = d->downloadManager->cancel(hash);
    if (callback) {
        callback(ok ? ApiResponse::ok() : ApiResponse::fail("Download not found"));
    }
}

void RatsAPI::downloadUpdate(const QString& hash,
                              const QJsonObject& options,
                              ApiCallback callback)
{
    if (!d->downloadManager) {
        if (callback) callback(ApiResponse::fail("Download manager not initialized"));
        return;
    }
    
    bool ok = true;
    
    if (options.contains("pause")) {
        QString pauseVal = options["pause"].toString();
        if (pauseVal == "switch") {
            ok = d->downloadManager->togglePause(hash);
        } else {
            if (options["pause"].toBool()) {
                ok = d->downloadManager->pause(hash);
            } else {
                ok = d->downloadManager->resume(hash);
            }
        }
    }
    
    if (options.contains("removeOnDone")) {
        QString rodVal = options["removeOnDone"].toString();
        if (rodVal == "switch") {
            ok = d->downloadManager->toggleRemoveOnDone(hash) && ok;
        } else {
            ok = d->downloadManager->setRemoveOnDone(hash, options["removeOnDone"].toBool()) && ok;
        }
    }
    
    if (callback) {
        if (ok) {
            DownloadInfo info = d->downloadManager->getDownload(hash);
            QJsonObject result;
            result["paused"] = info.paused;
            result["removeOnDone"] = info.removeOnDone;
            callback(ApiResponse::ok(result));
        } else {
            callback(ApiResponse::fail("Download not found"));
        }
    }
}

void RatsAPI::downloadSelectFiles(const QString& hash,
                                   const QJsonArray& files,
                                   ApiCallback callback)
{
    if (!d->downloadManager) {
        if (callback) callback(ApiResponse::fail("Download manager not initialized"));
        return;
    }
    
    bool ok = d->downloadManager->selectFiles(hash, files);
    if (callback) {
        callback(ok ? ApiResponse::ok() : ApiResponse::fail("Failed to select files"));
    }
}

void RatsAPI::getDownloads(ApiCallback callback)
{
    if (!d->downloadManager) {
        if (callback) callback(ApiResponse::ok(QJsonArray()));
        return;
    }
    
    if (callback) {
        callback(ApiResponse::ok(d->downloadManager->toJsonArray()));
    }
}

// ============================================================================
// Statistics API Implementation
// ============================================================================

void RatsAPI::getStatistics(ApiCallback callback)
{
    if (!d->database) {
        if (callback) callback(ApiResponse::fail("Database not initialized"));
        return;
    }
    
    TorrentDatabase::Statistics stats = d->database->getStatistics();
    
    QJsonObject result;
    result["torrents"] = stats.totalTorrents;
    result["files"] = stats.totalFiles;
    result["size"] = stats.totalSize;
    
    if (callback) callback(ApiResponse::ok(result));
}

void RatsAPI::getPeers(ApiCallback callback)
{
    QJsonObject result;
    
    if (d->p2p) {
        result["size"] = d->p2p->getPeerCount();
        result["connected"] = d->p2p->isConnected();
        result["dhtNodes"] = static_cast<int>(d->p2p->getDhtNodeCount());
    } else {
        result["size"] = 0;
        result["connected"] = false;
        result["dhtNodes"] = 0;
    }
    
    if (callback) callback(ApiResponse::ok(result));
}

void RatsAPI::getP2PStatus(ApiCallback callback)
{
    QJsonObject result;
    
    if (d->p2p) {
        result["running"] = d->p2p->isRunning();
        result["connected"] = d->p2p->isConnected();
        result["peerId"] = d->p2p->getOurPeerId();
        result["peerCount"] = d->p2p->getPeerCount();
        result["dhtRunning"] = d->p2p->isDhtRunning();
        result["dhtNodes"] = static_cast<int>(d->p2p->getDhtNodeCount());
        result["bitTorrentEnabled"] = d->p2p->isBitTorrentEnabled();
    } else {
        result["running"] = false;
        result["connected"] = false;
        result["peerCount"] = 0;
    }
    
    if (callback) callback(ApiResponse::ok(result));
}

// ============================================================================
// Config API Implementation
// ============================================================================

void RatsAPI::getConfig(ApiCallback callback)
{
    if (!d->config) {
        if (callback) callback(ApiResponse::fail("Config not initialized"));
        return;
    }
    
    if (callback) callback(ApiResponse::ok(d->config->toJson()));
}

void RatsAPI::setConfig(const QJsonObject& options, ApiCallback callback)
{
    if (!d->config) {
        if (callback) callback(ApiResponse::fail("Config not initialized"));
        return;
    }
    
    QStringList changed = d->config->fromJson(options);
    
    QJsonObject result;
    result["changed"] = QJsonArray::fromStringList(changed);
    
    if (callback) callback(ApiResponse::ok(result));
}

// ============================================================================
// Torrent Operations Implementation
// ============================================================================

void RatsAPI::vote(const QString& hash, bool isGood, ApiCallback callback)
{
    if (hash.length() != 40) {
        if (callback) callback(ApiResponse::fail("Invalid hash"));
        return;
    }
    
    if (!d->database) {
        if (callback) callback(ApiResponse::fail("Database not initialized"));
        return;
    }
    
    TorrentInfo torrent = d->database->getTorrent(hash);
    if (!torrent.isValid()) {
        if (callback) callback(ApiResponse::fail("Torrent not found"));
        return;
    }
    
    if (isGood) {
        torrent.good++;
    } else {
        torrent.bad++;
    }
    
    d->database->updateTorrent(torrent);
    
    // Update feed
    if (d->feedManager && isGood) {
        d->feedManager->addByHash(hash);
    }
    
    emit votesUpdated(hash, torrent.good, torrent.bad);
    
    QJsonObject result;
    result["hash"] = hash;
    result["good"] = torrent.good;
    result["bad"] = torrent.bad;
    
    if (callback) callback(ApiResponse::ok(result));
}

void RatsAPI::checkTrackers(const QString& hash, ApiCallback callback)
{
    if (hash.length() != 40) {
        if (callback) callback(ApiResponse::fail("Invalid hash"));
        return;
    }
    
    qInfo() << "Checking trackers for" << hash.left(8);
    
    // TODO: Implement UDP tracker checking
    // For now, just acknowledge the request
    
    QJsonObject result;
    result["hash"] = hash;
    result["status"] = "checking";
    
    if (callback) callback(ApiResponse::ok(result));
}

void RatsAPI::removeTorrents(bool checkOnly, ApiCallback callback)
{
    if (!d->database) {
        if (callback) callback(ApiResponse::fail("Database not initialized"));
        return;
    }
    
    // TODO: Implement torrent cleanup based on filters
    
    QJsonObject result;
    result["removed"] = 0;
    result["checkOnly"] = checkOnly;
    
    if (callback) callback(ApiResponse::ok(result));
}

// ============================================================================
// Feed API Implementation
// ============================================================================

void RatsAPI::getFeed(int index, int limit, ApiCallback callback)
{
    if (!d->feedManager) {
        if (callback) callback(ApiResponse::ok(QJsonArray()));
        return;
    }
    
    QJsonArray feed = d->feedManager->toJsonArray(index, limit);
    if (callback) callback(ApiResponse::ok(feed));
}
