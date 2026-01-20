#include "ratsapi.h"
#include "configmanager.h"
#include "feedmanager.h"
#include "downloadmanager.h"
#include "p2pstoremanager.h"
#include "trackerchecker.h"
#include "../torrentdatabase.h"
#include "../p2pnetwork.h"
#include "../torrentclient.h"

// librats for torrent parsing and DHT metadata lookup
#ifdef RATS_SEARCH_FEATURES
#include "../librats/src/bittorrent.h"
#include "../librats/src/librats.h"
#endif

#include <QDebug>
#include <QtConcurrent>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>

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
    std::unique_ptr<P2PStoreManager> p2pStore;
    std::unique_ptr<TrackerChecker> trackerChecker;
    
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

RatsAPI::~RatsAPI()
{
    // Save download session before destruction
    if (d->downloadManager) {
        QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QString sessionPath = dataPath + "/downloads_session.json";
        d->downloadManager->saveSession(sessionPath);
    }
}

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
        
        // Restore previous download session
        QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QString sessionPath = dataPath + "/downloads_session.json";
        int restored = d->downloadManager->restoreSession(sessionPath);
        if (restored > 0) {
            qInfo() << "Restored" << restored << "downloads from previous session";
        }
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
    
    // Initialize P2P store manager for distributed storage (voting, etc.)
    if (p2p) {
        d->p2pStore = std::make_unique<P2PStoreManager>(p2p, this);
        
        // Forward vote signals from remote peers
        connect(d->p2pStore.get(), &P2PStoreManager::voteStored,
                this, [this](const QString& hash, bool isGood, const QString& peerId) {
            Q_UNUSED(isGood);
            Q_UNUSED(peerId);
            // Update vote counts when remote votes arrive
            VoteCounts votes = d->p2pStore->getVotes(hash);
            emit votesUpdated(hash, votes.good, votes.bad);
        });
        
        connect(d->p2pStore.get(), &P2PStoreManager::syncCompleted,
                this, [this](bool success, const QString& error) {
            if (success) {
                qInfo() << "P2P store sync completed successfully";
            } else {
                qWarning() << "P2P store sync failed:" << error;
            }
        });
        
        qInfo() << "P2PStoreManager initialized";
    }
    
    // Initialize tracker checker
    if (config && config->trackersEnabled()) {
        d->trackerChecker = std::make_unique<TrackerChecker>(this);
        quint16 trackerPort = static_cast<quint16>(config->udpTrackersPort());
        if (d->trackerChecker->initialize(trackerPort)) {
            d->trackerChecker->setTimeout(config->udpTrackersTimeout());
            qInfo() << "TrackerChecker initialized on port" << d->trackerChecker->localPort();
        } else {
            qWarning() << "Failed to initialize TrackerChecker";
            d->trackerChecker.reset();
        }
    }
    
    // Forward config changes
    if (config) {
        connect(config, &ConfigManager::configChanged,
                this, [this](const QStringList& /*keys*/) {
            emit configChanged(d->config->toJson());
        });
    }
    
    // Setup P2P message handlers for remote API calls
    setupP2PHandlers();
    
    d->ready = true;
    qInfo() << "RatsAPI initialized";
}

// ============================================================================
// P2P API Setup (like legacy api.js)
// ============================================================================

void RatsAPI::setupP2PHandlers()
{
    if (!d->p2p) {
        qWarning() << "Cannot setup P2P handlers: P2P network not available";
        return;
    }
    
    qInfo() << "Setting up P2P API handlers...";
    
    // Handler for torrent search requests from remote peers
    // Legacy: p2p.on('searchTorrent', ...)
    d->p2p->registerMessageHandler("torrent_search", 
        [this](const QString& peerId, const QJsonObject& data) {
            handleP2PSearchRequest(peerId, data);
        });
    
    // Also register legacy name for compatibility
    d->p2p->registerMessageHandler("searchTorrent",
        [this](const QString& peerId, const QJsonObject& data) {
            handleP2PSearchRequest(peerId, data);
        });
    
    // Handler for file search requests
    // Legacy: p2p.on('searchFiles', ...)
    d->p2p->registerMessageHandler("searchFiles",
        [this](const QString& peerId, const QJsonObject& data) {
            handleP2PSearchFilesRequest(peerId, data);
        });
    
    // Handler for top torrents requests
    // Legacy: p2p.on('topTorrents', ...)
    d->p2p->registerMessageHandler("topTorrents",
        [this](const QString& peerId, const QJsonObject& data) {
            handleP2PTopTorrentsRequest(peerId, data);
        });
    
    // Handler for single torrent requests
    // Legacy: p2p.on('torrent', ...)
    d->p2p->registerMessageHandler("torrent",
        [this](const QString& peerId, const QJsonObject& data) {
            handleP2PTorrentRequest(peerId, data);
        });
    
    // Handler for feed requests
    // Legacy: p2p.on('feed', ...)
    d->p2p->registerMessageHandler("feed",
        [this](const QString& peerId, const QJsonObject& data) {
            handleP2PFeedRequest(peerId, data);
        });
    
    // Handler for search results from other peers (incoming results)
    d->p2p->registerMessageHandler("torrent_search_result",
        [this](const QString& peerId, const QJsonObject& data) {
            handleP2PSearchResult(peerId, data);
        });
    
    // Handler for torrent announcements
    d->p2p->registerMessageHandler("torrent_announce",
        [this](const QString& peerId, const QJsonObject& data) {
            handleP2PTorrentAnnounce(peerId, data);
        });
    
    // Handler for feed responses (P2P feed sync)
    d->p2p->registerMessageHandler("feed_response",
        [this](const QString& peerId, const QJsonObject& data) {
            handleP2PFeedResponse(peerId, data);
        });
    
    // Handler for randomTorrents (P2P replication)
    d->p2p->registerMessageHandler("randomTorrents",
        [this](const QString& peerId, const QJsonObject& data) {
            handleP2PRandomTorrentsRequest(peerId, data);
        });
    
    // Handler for randomTorrents_response (P2P replication - incoming torrents)
    d->p2p->registerMessageHandler("randomTorrents_response",
        [this](const QString& peerId, const QJsonObject& data) {
            QJsonArray torrents = data["torrents"].toArray();
            qInfo() << "Received" << torrents.size() << "random torrents from" << peerId.left(8);
            
            for (const QJsonValue& val : torrents) {
                if (val.isObject()) {
                    insertTorrentFromFeed(val.toObject());
                }
            }
        });
    
    // Connect to peer connected signal to request feed sync
    connect(d->p2p, &P2PNetwork::peerConnected,
            this, &RatsAPI::handleP2PPeerConnected);
    
    qInfo() << "P2P API handlers registered";
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
    methods_["torrent.getVotes"] = [this](const QJsonObject& params, ApiCallback cb) {
        getVotes(params["hash"].toString(), cb);
    };
    methods_["torrent.checkTrackers"] = [this](const QJsonObject& params, ApiCallback cb) {
        checkTrackers(params["hash"].toString(), cb);
    };
    methods_["torrent.remove"] = [this](const QJsonObject& params, ApiCallback cb) {
        removeTorrents(params["checkOnly"].toBool(false), cb);
    };
    methods_["torrent.drop"] = [this](const QJsonObject& params, ApiCallback cb) {
        // Accept base64-encoded torrent data
        QByteArray data = QByteArray::fromBase64(params["data"].toString().toLatin1());
        dropTorrents(data, cb);
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
            // Torrent not in database - try DHT metadata lookup (like legacy api.js:346-373)
#ifdef RATS_SEARCH_FEATURES
            if (d->p2p && d->p2p->isBitTorrentEnabled()) {
                auto* client = d->p2p->getRatsClient();
                if (client && client->is_bittorrent_enabled()) {
                    qInfo() << "Torrent" << hash.left(8) << "not in DB, trying DHT metadata lookup...";
                    
                    // Use get_torrent_metadata to fetch via DHT/BEP9
                    client->get_torrent_metadata(hash.toStdString(),
                        [this, hash, callback](const librats::TorrentInfo& libratsTorrent, bool success, const std::string& error) {
                            if (success && libratsTorrent.is_valid()) {
                                qInfo() << "DHT metadata lookup succeeded for" << hash.left(8);
                                
                                // Create TorrentInfo for database
                                TorrentInfo torrent;
                                torrent.hash = hash;
                                torrent.name = QString::fromStdString(libratsTorrent.get_name());
                                torrent.size = static_cast<qint64>(libratsTorrent.get_total_length());
                                torrent.files = static_cast<int>(libratsTorrent.get_files().size());
                                torrent.piecelength = static_cast<int>(libratsTorrent.get_piece_length());
                                torrent.added = QDateTime::currentDateTime();
                                
                                // Build file list
                                const auto& files = libratsTorrent.get_files();
                                for (const auto& f : files) {
                                    TorrentFile tf;
                                    tf.path = QString::fromStdString(f.path);
                                    tf.size = static_cast<qint64>(f.length);
                                    torrent.filesList.append(tf);
                                }
                                
                                // Detect content type
                                TorrentDatabase::detectContentType(torrent);
                                
                                // Insert into database for future lookups
                                if (d->database) {
                                    d->database->insertTorrent(torrent);
                                }
                                
                                QJsonObject result = torrentInfoToJson(torrent);
                                result["fromDHT"] = true;
                                
                                // Add files to result
                                if (!torrent.filesList.isEmpty()) {
                                    QJsonArray filesArr;
                                    for (const TorrentFile& f : torrent.filesList) {
                                        QJsonObject fileObj;
                                        fileObj["path"] = f.path;
                                        fileObj["size"] = f.size;
                                        filesArr.append(fileObj);
                                    }
                                    result["filesList"] = filesArr;
                                }
                                
                                QMetaObject::invokeMethod(this, [this, callback, result, hash, torrent]() {
                                    emit torrentIndexed(hash, torrent.name);
                                    if (callback) callback(ApiResponse::ok(result));
                                }, Qt::QueuedConnection);
                            } else {
                                qInfo() << "DHT metadata lookup failed for" << hash.left(8) 
                                        << ":" << QString::fromStdString(error);
                                QMetaObject::invokeMethod(this, [callback]() {
                                    if (callback) callback(ApiResponse::fail("Torrent not found"));
                                }, Qt::QueuedConnection);
                            }
                        });
                    return;  // Async operation in progress
                }
            }
#endif
            // No DHT available or not enabled
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
    
    // Check if already voted (via P2P store)
    if (d->p2pStore && d->p2pStore->isAvailable() && d->p2pStore->hasVoted(hash)) {
        // Already voted - return current vote counts from P2P store
        VoteCounts votes = d->p2pStore->getVotes(hash);
        
        QJsonObject result;
        result["hash"] = hash;
        result["good"] = votes.good;
        result["bad"] = votes.bad;
        result["selfVoted"] = true;
        result["alreadyVoted"] = true;
        
        if (callback) callback(ApiResponse::ok(result));
        return;
    }
    
    // Store vote in P2P distributed store (this syncs to all peers)
    bool storedInP2P = false;
    if (d->p2pStore && d->p2pStore->isAvailable()) {
        // Include torrent data for replication (like legacy _temp field)
        QJsonObject torrentData = torrentInfoToJson(torrent);
        storedInP2P = d->p2pStore->storeVote(hash, isGood, torrentData);
        
        if (storedInP2P) {
            qInfo() << "Vote stored in P2P network for" << hash.left(8);
        }
    }
    
    // Update local database counts as well (for fast local access)
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
    
    // Get aggregated votes (combines P2P store with local)
    int goodCount = torrent.good;
    int badCount = torrent.bad;
    
    if (d->p2pStore && d->p2pStore->isAvailable()) {
        VoteCounts votes = d->p2pStore->getVotes(hash);
        goodCount = votes.good;
        badCount = votes.bad;
    }
    
    emit votesUpdated(hash, goodCount, badCount);
    
    QJsonObject result;
    result["hash"] = hash;
    result["good"] = goodCount;
    result["bad"] = badCount;
    result["selfVoted"] = true;
    result["distributed"] = storedInP2P;
    
    if (callback) callback(ApiResponse::ok(result));
}

void RatsAPI::getVotes(const QString& hash, ApiCallback callback)
{
    if (hash.length() != 40) {
        if (callback) callback(ApiResponse::fail("Invalid hash"));
        return;
    }
    
    QJsonObject result;
    result["hash"] = hash;
    
    // Get votes from P2P store if available (aggregates all peer votes)
    if (d->p2pStore && d->p2pStore->isAvailable()) {
        VoteCounts votes = d->p2pStore->getVotes(hash);
        result["good"] = votes.good;
        result["bad"] = votes.bad;
        result["selfVoted"] = votes.selfVoted;
        result["source"] = "distributed";
    } else if (d->database) {
        // Fall back to local database
        TorrentInfo torrent = d->database->getTorrent(hash);
        if (torrent.isValid()) {
            result["good"] = torrent.good;
            result["bad"] = torrent.bad;
            result["selfVoted"] = false;  // Can't determine from local DB
            result["source"] = "local";
        } else {
            result["good"] = 0;
            result["bad"] = 0;
            result["selfVoted"] = false;
            result["source"] = "none";
        }
    } else {
        result["good"] = 0;
        result["bad"] = 0;
        result["selfVoted"] = false;
        result["source"] = "unavailable";
    }
    
    if (callback) callback(ApiResponse::ok(result));
}

P2PStoreManager* RatsAPI::p2pStore() const
{
    return d->p2pStore.get();
}

void RatsAPI::checkTrackers(const QString& hash, ApiCallback callback)
{
    if (hash.length() != 40) {
        if (callback) callback(ApiResponse::fail("Invalid hash"));
        return;
    }
    
    if (!d->trackerChecker) {
        if (callback) {
            QJsonObject result;
            result["hash"] = hash;
            result["status"] = "disabled";
            result["error"] = "Tracker checking is disabled";
            callback(ApiResponse::ok(result));
        }
        return;
    }
    
    qInfo() << "Checking trackers for" << hash.left(8);
    
    // Scrape from multiple trackers and get best result
    d->trackerChecker->scrapeMultiple(hash, [this, hash, callback](const TrackerResult& result) {
        QJsonObject response;
        response["hash"] = hash;
        
        if (result.success) {
            response["status"] = "success";
            response["seeders"] = result.seeders;
            response["leechers"] = result.leechers;
            response["completed"] = result.completed;
            response["tracker"] = result.tracker;
            
            // Update database with new tracker info
            if (d->database) {
                d->database->updateTrackerInfo(hash, result.seeders, result.leechers, result.completed);
            }
            
            qInfo() << "Tracker check for" << hash.left(8) << "- seeders:" << result.seeders 
                    << "leechers:" << result.leechers;
        } else {
            response["status"] = "failed";
            response["error"] = result.error.isEmpty() ? "No tracker responded" : result.error;
        }
        
        if (callback) callback(ApiResponse::ok(response));
    });
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

void RatsAPI::dropTorrents(const QByteArray& torrentData, ApiCallback callback)
{
#ifdef RATS_SEARCH_FEATURES
    if (torrentData.isEmpty()) {
        if (callback) callback(ApiResponse::fail("Empty torrent data"));
        return;
    }
    
    if (!d->database) {
        if (callback) callback(ApiResponse::fail("Database not initialized"));
        return;
    }
    
    // Parse torrent data using librats TorrentInfo
    std::vector<uint8_t> data(torrentData.begin(), torrentData.end());
    
    librats::TorrentInfo libratsTorrent;
    if (!libratsTorrent.load_from_data(data)) {
        if (callback) callback(ApiResponse::fail("Failed to parse torrent file"));
        return;
    }
    
    if (!libratsTorrent.is_valid()) {
        if (callback) callback(ApiResponse::fail("Invalid torrent file"));
        return;
    }
    
    // Convert info hash to hex string
    QString hash = QString::fromStdString(
        librats::info_hash_to_hex(libratsTorrent.get_info_hash()));
    
    // Check if torrent already exists
    TorrentInfo existing = d->database->getTorrent(hash);
    if (existing.isValid()) {
        // Return existing torrent info
        QJsonObject result = torrentInfoToJson(existing);
        result["alreadyExists"] = true;
        if (callback) callback(ApiResponse::ok(result));
        return;
    }
    
    // Create TorrentInfo for database
    TorrentInfo torrent;
    torrent.hash = hash;
    torrent.name = QString::fromStdString(libratsTorrent.get_name());
    torrent.size = static_cast<qint64>(libratsTorrent.get_total_length());
    torrent.files = static_cast<int>(libratsTorrent.get_files().size());
    torrent.piecelength = static_cast<int>(libratsTorrent.get_piece_length());
    torrent.added = QDateTime::currentDateTime();
    
    // Build file list
    const auto& files = libratsTorrent.get_files();
    for (const auto& f : files) {
        TorrentFile tf;
        tf.path = QString::fromStdString(f.path);
        tf.size = static_cast<qint64>(f.length);
        torrent.filesList.append(tf);
    }
    
    // Detect content type from files
    TorrentDatabase::detectContentType(torrent);
    
    // Check filters before inserting
    QString rejectionReason = getTorrentRejectionReason(torrent);
    if (!rejectionReason.isEmpty()) {
        if (callback) callback(ApiResponse::fail("Torrent rejected: " + rejectionReason));
        return;
    }
    
    // Insert into database (this also handles files via addFilesToDatabase)
    d->database->insertTorrent(torrent);
    
    qInfo() << "Imported torrent:" << torrent.name.left(50) << "hash:" << hash.left(8);
    
    emit torrentIndexed(hash, torrent.name);
    
    QJsonObject result = torrentInfoToJson(torrent);
    result["imported"] = true;
    
    if (callback) callback(ApiResponse::ok(result));
#else
    Q_UNUSED(torrentData);
    if (callback) callback(ApiResponse::fail("BitTorrent features not enabled"));
#endif
}

bool RatsAPI::checkTorrentFilters(const TorrentInfo& torrent) const
{
    return getTorrentRejectionReason(torrent).isEmpty();
}

QString RatsAPI::getTorrentRejectionReason(const TorrentInfo& torrent) const
{
    if (!d->config) {
        return QString();  // No config = no filters = pass
    }
    
    // Check max files filter
    int maxFiles = d->config->filtersMaxFiles();
    if (maxFiles > 0 && torrent.files > maxFiles) {
        return QString("Too many files: %1 > %2").arg(torrent.files).arg(maxFiles);
    }
    
    // Check size filters
    qint64 sizeMin = d->config->filtersSizeMin();
    qint64 sizeMax = d->config->filtersSizeMax();
    
    if (sizeMin > 0 && torrent.size < sizeMin) {
        return QString("Size too small: %1 < %2").arg(torrent.size).arg(sizeMin);
    }
    
    if (sizeMax > 0 && torrent.size > sizeMax) {
        return QString("Size too large: %1 > %2").arg(torrent.size).arg(sizeMax);
    }
    
    // Check adult filter
    if (d->config->filtersAdultFilter()) {
        // Check for adult content indicators
        QString nameLower = torrent.name.toLower();
        static QStringList adultKeywords = {"xxx", "porn", "sex", "adult", "18+", "nsfw"};
        
        for (const QString& keyword : adultKeywords) {
            if (nameLower.contains(keyword)) {
                return QString("Adult content detected: %1").arg(keyword);
            }
        }
        
        // Also check content category
        if (torrent.contentCategoryId == static_cast<int>(ContentCategory::XXX)) {
            return "Adult content category";
        }
    }
    
    // Check naming regex filter
    QString regexPattern = d->config->filtersNamingRegExp();
    if (!regexPattern.isEmpty()) {
        QRegularExpression regex(regexPattern, QRegularExpression::CaseInsensitiveOption);
        
        if (regex.isValid()) {
            bool matches = regex.match(torrent.name).hasMatch();
            bool isNegative = d->config->filtersNamingRegExpNegative();
            
            if (isNegative) {
                // Negative filter: reject if matches
                if (matches) {
                    return QString("Name matches blocked pattern: %1").arg(regexPattern);
                }
            } else {
                // Positive filter: reject if doesn't match
                if (!matches) {
                    return QString("Name doesn't match required pattern: %1").arg(regexPattern);
                }
            }
        }
    }
    
    // Check content type filter
    QString contentTypeFilter = d->config->filtersContentType();
    if (!contentTypeFilter.isEmpty() && contentTypeFilter != "all") {
        // Parse comma-separated content types
        QStringList allowedTypes = contentTypeFilter.split(',', Qt::SkipEmptyParts);
        
        if (!allowedTypes.isEmpty()) {
            bool typeAllowed = false;
            for (const QString& type : allowedTypes) {
                if (torrent.contentTypeString().compare(type.trimmed(), Qt::CaseInsensitive) == 0) {
                    typeAllowed = true;
                    break;
                }
            }
            
            if (!typeAllowed) {
                return QString("Content type not allowed: %1").arg(torrent.contentTypeString());
            }
        }
    }
    
    return QString();  // Passes all filters
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

// ============================================================================
// P2P Message Handlers Implementation
// These handle incoming requests from other peers and send responses back
// Similar to legacy api.js: p2p.on('searchTorrent', ...)
// ============================================================================

QJsonObject RatsAPI::torrentToP2PJson(const TorrentInfo& torrent)
{
    QJsonObject obj;
    obj["hash"] = torrent.hash;
    obj["info_hash"] = torrent.hash;  // Legacy compatibility
    obj["name"] = torrent.name;
    obj["size"] = torrent.size;
    obj["files"] = torrent.files;
    obj["seeders"] = torrent.seeders;
    obj["leechers"] = torrent.leechers;
    obj["completed"] = torrent.completed;
    obj["contentType"] = torrent.contentTypeString();
    obj["contentCategory"] = torrent.contentCategoryString();
    obj["added"] = torrent.added.isValid() ? torrent.added.toMSecsSinceEpoch() : 0;
    obj["good"] = torrent.good;
    obj["bad"] = torrent.bad;
    return obj;
}

void RatsAPI::handleP2PSearchRequest(const QString& peerId, const QJsonObject& data)
{
    if (!d->database || !d->p2p) {
        return;
    }
    
    // Parse query from different possible formats
    QString query = data["text"].toString();
    if (query.isEmpty()) {
        query = data["query"].toString();
    }
    
    if (query.length() <= 2) {
        qDebug() << "P2P search query too short, ignoring";
        return;
    }
    
    qInfo() << "Processing P2P search request for:" << query << "from peer" << peerId.left(8);
    
    // Build search options from navigation object (legacy format)
    SearchOptions opts;
    opts.query = query;
    opts.limit = 10;
    opts.index = 0;
    opts.orderDesc = true;
    
    // Parse navigation object (legacy format)
    if (data.contains("navigation")) {
        QJsonObject nav = data["navigation"].toObject();
        opts.limit = nav["limit"].toInt(10);
        opts.index = nav["index"].toInt(0);
        opts.orderBy = nav["orderBy"].toString();
        opts.orderDesc = nav["orderDesc"].toBool(true);
        opts.safeSearch = nav["safeSearch"].toBool(false);
        opts.contentType = nav["type"].toString();
    } else {
        // New format - params at top level
        opts.limit = data["limit"].toInt(10);
        opts.index = data["index"].toInt(0);
        opts.orderBy = data["orderBy"].toString();
        opts.orderDesc = data["orderDesc"].toBool(true);
        opts.safeSearch = data["safeSearch"].toBool(false);
    }
    
    // Execute search
    QVector<TorrentInfo> results = d->database->searchTorrents(opts);
    
    qInfo() << "Found" << results.size() << "results for P2P search from" << peerId.left(8);
    
    // Send each result back to the requester
    for (const TorrentInfo& torrent : results) {
        QJsonObject result = torrentToP2PJson(torrent);
        d->p2p->sendMessage(peerId, "torrent_search_result", result);
    }
}

void RatsAPI::handleP2PSearchFilesRequest(const QString& peerId, const QJsonObject& data)
{
    if (!d->database || !d->p2p) {
        return;
    }
    
    QString query = data["text"].toString();
    if (query.length() <= 2) {
        return;
    }
    
    qInfo() << "Processing P2P searchFiles request for:" << query << "from" << peerId.left(8);
    
    SearchOptions opts;
    opts.query = query;
    opts.limit = data["limit"].toInt(10);
    opts.index = data["index"].toInt(0);
    
    if (data.contains("navigation")) {
        QJsonObject nav = data["navigation"].toObject();
        opts.limit = nav["limit"].toInt(10);
        opts.index = nav["index"].toInt(0);
    }
    
    QVector<TorrentInfo> results = d->database->searchFiles(opts);
    
    // Send results
    for (const TorrentInfo& torrent : results) {
        QJsonObject result = torrentToP2PJson(torrent);
        
        // Add file paths if available
        if (!torrent.filesList.isEmpty()) {
            QJsonArray paths;
            for (const TorrentFile& f : torrent.filesList) {
                paths.append(f.path);
            }
            result["path"] = paths;
        }
        
        d->p2p->sendMessage(peerId, "searchFiles_result", result);
    }
}

void RatsAPI::handleP2PTopTorrentsRequest(const QString& peerId, const QJsonObject& data)
{
    if (!d->database || !d->p2p) {
        return;
    }
    
    QString type = data["type"].toString();
    QString time;
    int index = 0;
    int limit = 20;
    
    if (data.contains("navigation")) {
        QJsonObject nav = data["navigation"].toObject();
        time = nav["time"].toString();
        index = nav["index"].toInt(0);
        limit = nav["limit"].toInt(20);
    } else {
        time = data["time"].toString();
        index = data["index"].toInt(0);
        limit = data["limit"].toInt(20);
    }
    
    qInfo() << "Processing P2P topTorrents request from" << peerId.left(8);
    
    QVector<TorrentInfo> results = d->database->getTopTorrents(type, time, index, limit);
    
    // Build response array
    QJsonArray torrentsArray;
    for (const TorrentInfo& torrent : results) {
        torrentsArray.append(torrentToP2PJson(torrent));
    }
    
    QJsonObject response;
    response["torrents"] = torrentsArray;
    response["type"] = type;
    response["time"] = time;
    
    d->p2p->sendMessage(peerId, "topTorrents_response", response);
    
    // Also emit for UI (like legacy remoteTopTorrents)
    emit remoteSearchResults("top_" + type, torrentsArray);
}

void RatsAPI::handleP2PTorrentRequest(const QString& peerId, const QJsonObject& data)
{
    if (!d->database || !d->p2p) {
        return;
    }
    
    QString hash = data["hash"].toString();
    if (hash.length() != 40) {
        return;
    }
    
    bool includeFiles = false;
    if (data.contains("options")) {
        includeFiles = data["options"].toObject()["files"].toBool(false);
    } else {
        includeFiles = data["files"].toBool(false);
    }
    
    qInfo() << "Processing P2P torrent request for" << hash.left(8) << "from" << peerId.left(8);
    
    TorrentInfo torrent = d->database->getTorrent(hash, includeFiles);
    
    if (!torrent.isValid()) {
        // Torrent not found, don't respond
        return;
    }
    
    QJsonObject response = torrentToP2PJson(torrent);
    
    if (includeFiles && !torrent.filesList.isEmpty()) {
        QJsonArray filesArray;
        for (const TorrentFile& f : torrent.filesList) {
            QJsonObject fileObj;
            fileObj["path"] = f.path;
            fileObj["size"] = f.size;
            filesArray.append(fileObj);
        }
        response["filesList"] = filesArray;
    }
    
    d->p2p->sendMessage(peerId, "torrent_response", response);
}

void RatsAPI::handleP2PFeedRequest(const QString& peerId, const QJsonObject& data)
{
    Q_UNUSED(data);
    
    if (!d->p2p) {
        return;
    }
    
    qInfo() << "Processing P2P feed request from" << peerId.left(8);
    
    QJsonObject response;
    
    if (d->feedManager) {
        response["feed"] = d->feedManager->toJsonArray();
        response["feedDate"] = d->feedManager->feedDate();
        response["size"] = d->feedManager->size();
    } else {
        response["feed"] = QJsonArray();
        response["feedDate"] = 0;
        response["size"] = 0;
    }
    
    d->p2p->sendMessage(peerId, "feed_response", response);
}

void RatsAPI::handleP2PFeedResponse(const QString& peerId, const QJsonObject& data)
{
    if (!d->feedManager) {
        return;
    }
    
    int remoteSize = data["size"].toInt(data["feed"].toArray().size());
    qint64 remoteFeedDate = data["feedDate"].toVariant().toLongLong();
    QJsonArray remoteFeed = data["feed"].toArray();
    
    int localSize = d->feedManager->size();
    qint64 localFeedDate = d->feedManager->feedDate();
    
    qInfo() << "Received feed response from" << peerId.left(8) 
            << "- Remote:" << remoteSize << "items, date:" << remoteFeedDate
            << "- Local:" << localSize << "items, date:" << localFeedDate;
    
    // Determine if we should replace our feed with remote feed
    // (Like legacy: if remote is bigger/newer, replace)
    bool shouldReplace = false;
    
    if (remoteSize > localSize) {
        shouldReplace = true;
        qInfo() << "Replacing local feed: remote has more items";
    } else if (remoteSize == localSize && remoteFeedDate > localFeedDate) {
        shouldReplace = true;
        qInfo() << "Replacing local feed: remote is newer";
    }
    
    if (shouldReplace) {
        d->feedManager->fromJsonArray(remoteFeed, remoteFeedDate);
        
        // Replicate torrents from feed to our database
        for (const QJsonValue& val : remoteFeed) {
            if (val.isObject()) {
                QJsonObject torrentObj = val.toObject();
                insertTorrentFromFeed(torrentObj);
            }
        }
        
        qInfo() << "Feed replaced with" << remoteFeed.size() << "items from peer" << peerId.left(8);
        emit feedUpdated(d->feedManager->toJsonArray());
    }
}

void RatsAPI::insertTorrentFromFeed(const QJsonObject& torrentData)
{
    if (!d->database) {
        return;
    }
    
    QString hash = torrentData["hash"].toString();
    if (hash.length() != 40) {
        return;
    }
    
    // Check if we already have this torrent
    TorrentInfo existing = d->database->getTorrent(hash);
    if (existing.isValid()) {
        // Update votes if remote has more
        int remoteGood = torrentData["good"].toInt();
        int remoteBad = torrentData["bad"].toInt();
        
        if (remoteGood > existing.good || remoteBad > existing.bad) {
            existing.good = qMax(existing.good, remoteGood);
            existing.bad = qMax(existing.bad, remoteBad);
            d->database->updateTorrent(existing);
        }
        return;
    }
    
    // Create new torrent entry from feed data
    TorrentInfo torrent;
    torrent.hash = hash;
    torrent.name = torrentData["name"].toString();
    torrent.size = torrentData["size"].toVariant().toLongLong();
    torrent.files = torrentData["files"].toInt();
    torrent.seeders = torrentData["seeders"].toInt();
    torrent.good = torrentData["good"].toInt();
    torrent.bad = torrentData["bad"].toInt();
    
    QString contentType = torrentData["contentType"].toString();
    torrent.setContentTypeFromString(contentType);
    
    QString contentCategory = torrentData["contentCategory"].toString();
    torrent.setContentCategoryFromString(contentCategory);
    
    torrent.added = QDateTime::currentDateTime();
    
    // Check filters before inserting
    QString rejectionReason = getTorrentRejectionReason(torrent);
    if (!rejectionReason.isEmpty()) {
        qDebug() << "Rejected torrent from feed:" << torrent.name.left(30) << "-" << rejectionReason;
        return;
    }
    
    // Insert into database
    d->database->insertTorrent(torrent);
    
    qDebug() << "Inserted torrent from feed:" << torrent.name.left(30);
}

void RatsAPI::handleP2PSearchResult(const QString& peerId, const QJsonObject& data)
{
    // Received search result from another peer
    QString hash = data["info_hash"].toString();
    if (hash.isEmpty()) {
        hash = data["hash"].toString();
    }
    
    if (hash.isEmpty()) {
        return;
    }
    
    qDebug() << "Received P2P search result from" << peerId.left(8) << ":" << data["name"].toString();
    
    // Mark as remote result
    QJsonObject result = data;
    result["remote"] = true;
    result["peer"] = peerId;
    
    QJsonArray results;
    results.append(result);
    
    // Emit for UI handling
    emit remoteSearchResults(QString(), results);
}

void RatsAPI::handleP2PTorrentAnnounce(const QString& peerId, const QJsonObject& data)
{
    QString hash = data["info_hash"].toString();
    QString name = data["name"].toString();
    
    if (hash.isEmpty() || name.isEmpty()) {
        return;
    }
    
    qDebug() << "Received torrent announcement from" << peerId.left(8) << ":" << name;
    
    // Optionally insert into database for replication
    insertTorrentFromFeed(data);
    
    emit torrentIndexed(hash, name);
}

void RatsAPI::handleP2PPeerConnected(const QString& peerId)
{
    // Request feed from newly connected peer (for P2P feed sync)
    if (d->p2p && d->feedManager) {
        qInfo() << "Requesting feed from new peer" << peerId.left(8);
        
        QJsonObject request;
        request["localSize"] = d->feedManager->size();
        request["localFeedDate"] = d->feedManager->feedDate();
        
        d->p2p->sendMessage(peerId, "feed", request);
    }
    
    // Also request random torrents for replication (if enabled in config)
    if (d->p2p && d->config && d->config->p2pReplication()) {
        qInfo() << "Requesting random torrents from new peer" << peerId.left(8);
        
        QJsonObject request;
        request["limit"] = 5;
        
        d->p2p->sendMessage(peerId, "randomTorrents", request);
    }
}

void RatsAPI::handleP2PRandomTorrentsRequest(const QString& peerId, const QJsonObject& data)
{
    // Handle randomTorrents request - like legacy api.js
    if (!d->database || !d->p2p) {
        return;
    }
    
    // Check if replication server is enabled
    if (!d->config || !d->config->p2pReplicationServer()) {
        qDebug() << "P2P replication server disabled, ignoring randomTorrents request";
        return;
    }
    
    int limit = data["limit"].toInt(5);
    // Limit based on server load (like legacy)
    limit = qBound(1, limit, 10);
    
    qInfo() << "Processing P2P randomTorrents request from" << peerId.left(8) << "limit:" << limit;
    
    QVector<TorrentInfo> torrents = d->database->getRandomTorrents(limit);
    
    QJsonArray response;
    for (const TorrentInfo& torrent : torrents) {
        QJsonObject obj = torrentToP2PJson(torrent);
        
        // Include files list for replication
        if (!torrent.filesList.isEmpty()) {
            QJsonArray filesArray;
            for (const TorrentFile& file : torrent.filesList) {
                QJsonObject fileObj;
                fileObj["path"] = file.path;
                fileObj["size"] = file.size;
                filesArray.append(fileObj);
            }
            obj["filesList"] = filesArray;
        }
        
        response.append(obj);
    }
    
    d->p2p->sendMessage(peerId, "randomTorrents_response", QJsonObject{{"torrents", response}});
}
