#include "ratsapi.h"
#include "configmanager.h"
#include "feedmanager.h"
#include "p2pstoremanager.h"
#include "trackerwrapper.h"
#include "../torrentdatabase.h"
#include "../sphinxql.h"
#include "../p2pnetwork.h"
#include "../torrentclient.h"

// librats for torrent parsing and DHT metadata lookup
#ifdef RATS_SEARCH_FEATURES
#include "../librats/src/bittorrent.h"
#include "../librats/src/librats.h"
#include "../librats/src/bt_create_torrent.h"
#endif

#include <QDebug>
#include <QtConcurrent>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

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
    
    std::unique_ptr<FeedManager> feedManager;
    std::unique_ptr<P2PStoreManager> p2pStore;
    std::unique_ptr<TrackerWrapper> trackerWrapper;
    
    // Top torrents cache (protected by mutex for thread-safety)
    mutable QMutex topCacheMutex;
    QHash<QString, QJsonArray> topCache;
    QDateTime topCacheExpiry;
    
    // P2P Replication (like legacy api.js:247-272)
    QTimer* replicationTimer = nullptr;
    int replicationInterval = 5000;  // Start with 5 seconds
    int replicationTorrentsReceived = 0;  // Track received torrents per cycle
    qint64 totalReplicatedTorrents = 0;
    
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
    if (d->torrentClient) {
        QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QString sessionPath = dataPath + "/downloads_session.json";
        d->torrentClient->saveSession(sessionPath);
    }
}

// ============================================================================
// Torrent Processing Helpers Implementation
// ============================================================================

#ifdef RATS_SEARCH_FEATURES
TorrentInfo RatsAPI::createTorrentFromLibrats(const QString& hash, 
                                               const librats::TorrentInfo& libratsTorrent)
{
    TorrentInfo torrent;
    torrent.hash = hash.toLower();
    torrent.name = QString::fromStdString(libratsTorrent.name());
    torrent.size = static_cast<qint64>(libratsTorrent.total_size());
    torrent.files = static_cast<int>(libratsTorrent.files().files().size());
    torrent.piecelength = static_cast<int>(libratsTorrent.piece_length());
    torrent.added = QDateTime::currentDateTime();
    
    // Build file list
    const auto& files = libratsTorrent.files().files();
    for (const auto& f : files) {
        TorrentFile tf;
        tf.path = QString::fromStdString(f.path);
        tf.size = static_cast<qint64>(f.size);
        torrent.filesList.append(tf);
    }
    
    return torrent;
}
#endif

TorrentInfo RatsAPI::createTorrentFromJson(const QJsonObject& data)
{
    TorrentInfo torrent;
    
    torrent.hash = data["hash"].toString().toLower();
    torrent.name = data["name"].toString();
    torrent.size = data["size"].toVariant().toLongLong();
    torrent.files = data["files"].toInt();
    torrent.piecelength = data["piecelength"].toInt();
    torrent.seeders = data["seeders"].toInt();
    torrent.leechers = data["leechers"].toInt();
    torrent.completed = data["completed"].toInt();
    torrent.good = data["good"].toInt();
    torrent.bad = data["bad"].toInt();
    
    // Parse added timestamp
    if (data.contains("added")) {
        qint64 addedMs = data["added"].toVariant().toLongLong();
        if (addedMs > 0) {
            torrent.added = QDateTime::fromMSecsSinceEpoch(addedMs);
        }
    }
    if (!torrent.added.isValid()) {
        torrent.added = QDateTime::currentDateTime();
    }
    
    // Parse content type/category
    QString contentType = data["contentType"].toString();
    if (!contentType.isEmpty()) {
        torrent.setContentTypeFromString(contentType);
    }
    
    QString contentCategory = data["contentCategory"].toString();
    if (!contentCategory.isEmpty()) {
        torrent.setContentCategoryFromString(contentCategory);
    }
    
    // Parse filesList (critical for P2P replication)
    if (data.contains("filesList")) {
        QJsonArray filesArray = data["filesList"].toArray();
        for (const QJsonValue& fileVal : filesArray) {
            QJsonObject fileObj = fileVal.toObject();
            TorrentFile tf;
            tf.path = fileObj["path"].toString();
            tf.size = fileObj["size"].toVariant().toLongLong();
            torrent.filesList.append(tf);
        }
        
        // Update files count if not set
        if (torrent.files == 0 && !torrent.filesList.isEmpty()) {
            torrent.files = torrent.filesList.size();
        }
    }
    
    return torrent;
}

RatsAPI::InsertResult RatsAPI::processAndInsertTorrent(TorrentInfo& torrent,
                                                        bool detectContentType,
                                                        bool emitSignal)
{
    InsertResult result;
    
    if (!d->database) {
        result.error = "Database not initialized";
        return result;
    }
    
    if (torrent.hash.length() != 40) {
        result.error = "Invalid hash";
        return result;
    }
    
    if (torrent.name.isEmpty()) {
        result.error = "Empty torrent name";
        return result;
    }
    
    qDebug() << "Processing torrent:" << torrent.hash.left(16) << torrent.name.left(40);
    
    // Check if torrent already exists
    TorrentInfo existing = d->database->getTorrent(torrent.hash);
    if (existing.isValid()) {
        result.success = true;
        result.alreadyExists = true;
        result.torrent = existing;
        
        // Update votes if remote has more
        if (torrent.good > existing.good || torrent.bad > existing.bad) {
            existing.good = qMax(existing.good, torrent.good);
            existing.bad = qMax(existing.bad, torrent.bad);
            d->database->updateTorrent(existing);
            result.torrent = existing;
        }
        
        return result;
    }
    
    // Detect content type from files if needed
    if (detectContentType && torrent.contentTypeId == 0) {
        TorrentDatabase::detectContentType(torrent);
    }
    
    // Check filters before inserting
    QString rejectionReason = getTorrentRejectionReason(torrent);
    if (!rejectionReason.isEmpty()) {
        qInfo() << "Torrent rejected:" << torrent.hash.left(16) << "-" << rejectionReason;
        result.error = "Rejected: " + rejectionReason;
        return result;
    }
    
    // Insert into database (this also handles files via addFilesToDatabase)
    d->database->insertTorrent(torrent);
    
    result.success = true;
    result.torrent = torrent;
    
    qInfo() << "Torrent indexed:" << torrent.hash.left(16) << torrent.name.left(50) 
            << "size:" << (torrent.size / (1024*1024)) << "MB files:" << torrent.files;
    
    // Emit signal for UI updates
    if (emitSignal) {
        emit torrentIndexed(torrent.hash, torrent.name);
    }
    
    return result;
}

// ============================================================================
// Initialization
// ============================================================================

void RatsAPI::initialize(TorrentDatabase* database,
                         P2PNetwork* p2p,
                         TorrentClient* torrentClient,
                         ConfigManager* config)
{
    d->database = database;
    d->p2p = p2p;
    d->torrentClient = torrentClient;
    d->config = config;
    
    // Connect torrent client signals and set up database
    if (torrentClient) {
        // Set database for metadata lookup
        torrentClient->setDatabase(database);
        
        // Forward download signals
        connect(torrentClient, &TorrentClient::downloadStarted,
                this, [this](const QString& hash) {
            emit downloadProgress(hash, {{"status", "started"}});
        });
        connect(torrentClient, &TorrentClient::progressUpdated,
                this, &RatsAPI::downloadProgress);
        connect(torrentClient, &TorrentClient::filesReadyJson,
                this, &RatsAPI::filesReady);
        connect(torrentClient, &TorrentClient::downloadCompleted,
                this, [this](const QString& hash) {
            emit downloadCompleted(hash, false);
        });
        connect(torrentClient, &TorrentClient::torrentRemoved,
                this, [this](const QString& hash) {
            emit downloadCompleted(hash, true);
        });
        
        // Restore previous download session
        QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QString sessionPath = dataPath + "/downloads_session.json";
        int restored = torrentClient->loadSession(sessionPath);
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
    
    // Initialize tracker wrapper (uses librats for HTTP/UDP tracker support)
    if (config && config->trackersEnabled()) {
        d->trackerWrapper = std::make_unique<TrackerWrapper>(this);
        d->trackerWrapper->setTimeout(config->udpTrackersTimeout());
        qInfo() << "TrackerWrapper initialized (using librats tracker implementation)";
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
    
    // Initialize P2P replication timer (like legacy api.js:247-272)
    if (p2p && config) {
        d->replicationTimer = new QTimer(this);
        connect(d->replicationTimer, &QTimer::timeout, this, &RatsAPI::performReplicationCycle);
        
        // Connect to replication config changes
        connect(config, &ConfigManager::p2pReplicationChanged, this, [this](bool enabled) {
            if (enabled) {
                startReplication();
            } else {
                stopReplication();
            }
        });
        
        // Start replication if enabled
        if (config->p2pReplication()) {
            startReplication();
        }
    }
    
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
    
    // Handler for file search results from other peers
    d->p2p->registerMessageHandler("searchFiles_result",
        [this](const QString& peerId, const QJsonObject& data) {
            // Convert incoming file search result to array and emit signal
            QString searchQuery = data["text"].toString();
            QJsonArray torrents;
            
            QJsonObject torrentData = data;
            torrentData["isFileMatch"] = true;
            torrentData["remote"] = true;
            torrentData["peer"] = peerId;  // Track source peer for remote fetch
            
            // Convert 'path' array to 'matchingPaths' for consistency
            if (data.contains("path")) {
                torrentData["matchingPaths"] = data["path"];
            }
            
            torrents.append(torrentData);
            emit remoteFileSearchResults(searchQuery, torrents);
            
            qInfo() << "Received file search result from" << peerId.left(8) << "for query:" << searchQuery;
        });
    
    // Handler for top torrents response (incoming results from remote peers)
    d->p2p->registerMessageHandler("topTorrents_response",
        [this](const QString& peerId, const QJsonObject& data) {
            QString type = data["type"].toString();
            QString time = data["time"].toString();
            QJsonArray torrents = data["torrents"].toArray();
            
            // Add source peer to each torrent for remote fetch
            QJsonArray torrentsWithPeer;
            for (const QJsonValue& val : torrents) {
                QJsonObject obj = val.toObject();
                obj["peer"] = peerId;
                obj["remote"] = true;
                torrentsWithPeer.append(obj);
            }
            
            qInfo() << "Received" << torrentsWithPeer.size() << "top torrents from" << peerId.left(8);
            emit remoteTopTorrents(torrentsWithPeer, type, time);
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
    
    // Handler for torrent_response (response to our torrent request from remote peer)
    // This saves received torrent metadata to our database for replication
    d->p2p->registerMessageHandler("torrent_response",
        [this](const QString& peerId, const QJsonObject& data) {
            handleP2PTorrentResponse(peerId, data);
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
            qDebug() << "Received" << torrents.size() << "random torrents from" << peerId.left(8);
            
            int inserted = 0;
            for (const QJsonValue& val : torrents) {
                if (val.isObject()) {
                    QJsonObject torrentObj = val.toObject();
                    
                    // insertTorrentFromFeed returns true if torrent was actually inserted
                    if (insertTorrentFromFeed(torrentObj)) {
                        inserted++;
                        
                        // Track for replication statistics
                        onReplicationTorrentReceived();
                    }
                }
            }
            
            if (inserted > 0) {
                qInfo() << "P2P: Replicated" << inserted << "torrents from peer" << peerId.left(8);
                
                // Emit signal to update UI (statusbar, etc.)
                emit replicationProgress(inserted, d->totalReplicatedTorrents);
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
    methods_["torrent.addFile"] = [this](const QJsonObject& params, ApiCallback cb) {
        QString filePath = params["path"].toString();
        addTorrentFile(filePath, cb);
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

// Helper: Parse magnet link to extract info hash (like legacy magnetParse.js)
static QString parseMagnetLink(const QString& text)
{
    // If it's already a 40-char hex hash, return it
    static QRegularExpression hashRegex("^[0-9a-fA-F]{40}$");
    if (hashRegex.match(text).hasMatch()) {
        return text.toLower();
    }
    
    // Parse magnet link: magnet:?xt=urn:btih:HASH...
    static QRegularExpression magnetRegex("(?:magnet:\\?.*?)?xt=urn:btih:([0-9a-fA-F]{40})", 
                                          QRegularExpression::CaseInsensitiveOption);
    auto match = magnetRegex.match(text);
    if (match.hasMatch()) {
        return match.captured(1).toLower();
    }
    
    // Also try base32 (32 uppercase letters/numbers)
    // Note: Would need to decode base32 to hex - for now just return empty
    
    return QString();
}

// Helper: Check if text looks like a SHA1 info hash
static bool isSHA1Hash(const QString& text)
{
    if (text.length() != 40) {
        return false;
    }
    static QRegularExpression hexRegex("^[0-9a-fA-F]{40}$");
    return hexRegex.match(text).hasMatch();
}

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
    
    // Parse magnet link if present (like legacy api.js:295)
    QString query = parseMagnetLink(text);
    if (query.isEmpty()) {
        query = text;
    }
    
    qInfo() << "API: searchTorrents query:" << query.left(40) << "limit:" << options["limit"].toInt(10);
    
    SearchOptions opts;
    opts.query = query;
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
    
    // Check if query is SHA1 hash - do direct lookup instead of text search
    bool isHash = isSHA1Hash(query);
    
    // Run in background
    (void)QtConcurrent::run([this, opts, isHash, callback]() {
        QVector<TorrentInfo> results = d->database->searchTorrents(opts);
        
        QJsonArray torrents;
        for (const TorrentInfo& t : results) {
            torrents.append(torrentInfoToJson(t));
        }
        
        // If searching by hash and no results, try DHT metadata lookup (like legacy api.js:346-373)
        if (isHash && results.isEmpty()) {
#ifdef RATS_SEARCH_FEATURES
            if (d->p2p && d->p2p->isBitTorrentEnabled()) {
                auto* client = d->p2p->getRatsClient();
                if (client && client->is_bittorrent_enabled()) {
                    QString hash = opts.query.toLower();
                    qInfo() << "Search: Hash" << hash.left(8) << "not in DB, trying DHT lookup...";
                    
                    client->get_torrent_metadata(hash.toStdString(),
                        [this, hash, callback](const librats::TorrentInfo& libratsTorrent, bool success, const std::string& error) {
                            if (success && libratsTorrent.is_valid()) {
                                qInfo() << "DHT search lookup succeeded for" << hash.left(8);
                                
                                // Use helper to create and insert torrent
                                TorrentInfo torrent = createTorrentFromLibrats(hash, libratsTorrent);
                                InsertResult insertResult = const_cast<RatsAPI*>(this)->processAndInsertTorrent(torrent);
                                
                                QJsonObject result = torrentInfoToJson(insertResult.torrent);
                                result["fromDHT"] = true;
                                
                                QJsonArray results;
                                results.append(result);
                                
                                QMetaObject::invokeMethod(this, [callback, results]() {
                                    if (callback) callback(ApiResponse::ok(results));
                                }, Qt::QueuedConnection);
                            } else {
                                qInfo() << "DHT search lookup failed for" << hash.left(8)
                                        << ":" << QString::fromStdString(error);
                                // Return empty results
                                QMetaObject::invokeMethod(this, [callback]() {
                                    if (callback) callback(ApiResponse::ok(QJsonArray()));
                                }, Qt::QueuedConnection);
                            }
                        });
                    return;  // Async operation in progress
                }
            }
#endif
        }
        
        if (callback) {
            QMetaObject::invokeMethod(this, [callback, torrents]() {
                callback(ApiResponse::ok(torrents));
            }, Qt::QueuedConnection);
        }
    });
    
    // Also search P2P network if available (for text search, not hash lookup)
    if (!isHash && d->p2p && d->p2p->isConnected()) {
        d->p2p->searchTorrents(query);
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
            
            // Mark as file match result
            obj["isFileMatch"] = t.isFileMatch;
            
            // Add matching paths (highlighted snippets)
            if (!t.matchingPaths.isEmpty()) {
                QJsonArray paths;
                for (const QString& path : t.matchingPaths) {
                    paths.append(path);
                }
                obj["matchingPaths"] = paths;
            }
            
            // Also add file paths for legacy compatibility
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
        qWarning() << "API: getTorrent invalid hash length:" << hash.length();
        if (callback) callback(ApiResponse::fail("Invalid hash"));
        return;
    }
    
    if (!d->database) {
        if (callback) callback(ApiResponse::fail("Database not initialized"));
        return;
    }
    
    // Handle remote peer request via P2P (like legacy api.js:128-161)
    if (!remotePeer.isEmpty() && d->p2p) {
        qInfo() << "API: getTorrent" << hash.left(16) << "from remote peer" << remotePeer.left(8);
        
        QJsonObject request;
        request["hash"] = hash;
        
        QJsonObject options;
        options["files"] = includeFiles;
        request["options"] = options;
        
        // Send request to specific peer
        d->p2p->sendMessage(remotePeer, "torrent", request);

        return;
    }
    
    (void)QtConcurrent::run([this, hash, includeFiles, callback]() {
        TorrentInfo torrent = d->database->getTorrent(hash, includeFiles);
        
        if (!torrent.isValid()) {
            // Torrent not in database - try DHT metadata lookup (like legacy api.js:346-373)
#ifdef RATS_SEARCH_FEATURES
            if (d->p2p && d->p2p->isBitTorrentEnabled()) {
                auto* client = d->p2p->getRatsClient();
                if (client && client->is_bittorrent_enabled()) {
                    qInfo() << "DHT: Looking up metadata for" << hash.left(16);
                    
                    // Use get_torrent_metadata to fetch via DHT/BEP9
                    client->get_torrent_metadata(hash.toStdString(),
                        [this, hash, callback](const librats::TorrentInfo& libratsTorrent, bool success, const std::string& error) {
                            if (success && libratsTorrent.is_valid()) {
                                qInfo() << "DHT: Metadata received for" << hash.left(16) 
                                        << "name:" << QString::fromStdString(libratsTorrent.name()).left(40);
                                
                                // Use helper to create and insert torrent
                                TorrentInfo torrent = createTorrentFromLibrats(hash, libratsTorrent);
                                InsertResult insertResult = const_cast<RatsAPI*>(this)->processAndInsertTorrent(torrent);
                                
                                QJsonObject result = torrentInfoToJson(insertResult.torrent);
                                result["fromDHT"] = true;
                                
                                // Add files to result
                                if (!insertResult.torrent.filesList.isEmpty()) {
                                    QJsonArray filesArr;
                                    for (const TorrentFile& f : insertResult.torrent.filesList) {
                                        QJsonObject fileObj;
                                        fileObj["path"] = f.path;
                                        fileObj["size"] = f.size;
                                        filesArr.append(fileObj);
                                    }
                                    result["filesList"] = filesArr;
                                }
                                
                                QMetaObject::invokeMethod(this, [callback, result]() {
                                    if (callback) callback(ApiResponse::ok(result));
                                }, Qt::QueuedConnection);
                            } else {
                                qInfo() << "DHT: Metadata lookup failed for" << hash.left(16) 
                                        << "-" << QString::fromStdString(error);
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
            qDebug() << "Torrent not found in DB and DHT unavailable:" << hash.left(16);
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
        if (d->torrentClient && d->torrentClient->isDownloading(hash)) {
            ActiveTorrent dl = d->torrentClient->getTorrent(hash);
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
    
    // Check cache (protected by mutex for thread-safety)
    QString cacheKey = QString("%1_%2_%3_%4").arg(type, time, QString::number(index), QString::number(limit));
    {
        QMutexLocker locker(&d->topCacheMutex);
        if (d->topCacheExpiry.isValid() && d->topCacheExpiry > QDateTime::currentDateTime()) {
            auto it = d->topCache.find(cacheKey);
            if (it != d->topCache.end()) {
                if (callback) callback(ApiResponse::ok(*it));
                return;
            }
        }
    }
    
    (void)QtConcurrent::run([this, type, time, index, limit, cacheKey, callback]() {
        QVector<TorrentInfo> results = d->database->getTopTorrents(type, time, index, limit);
        
        QJsonArray torrents;
        for (const TorrentInfo& t : results) {
            torrents.append(torrentInfoToJson(t));
        }
        
        // Update cache (protected by mutex for thread-safety)
        {
            QMutexLocker locker(&d->topCacheMutex);
            d->topCache[cacheKey] = torrents;
            d->topCacheExpiry = QDateTime::currentDateTime().addSecs(86400);  // 24h cache
        }
        
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
    qInfo() << "API: downloadAdd hash:" << hash.left(16) << "path:" << savePath;
    
    if (hash.length() != 40) {
        if (callback) callback(ApiResponse::fail("Invalid hash"));
        return;
    }
    
    if (!d->torrentClient) {
        if (callback) callback(ApiResponse::fail("Torrent client not initialized"));
        return;
    }
    
    // Get torrent info from database for name/size
    QString name = hash;
    qint64 size = 0;
    if (d->database) {
        TorrentInfo torrent = d->database->getTorrent(hash);
        if (torrent.isValid()) {
            name = torrent.name;
            size = torrent.size;
        }
    }
    
    bool ok = d->torrentClient->downloadWithInfo(hash, name, size, savePath);
    if (callback) {
        callback(ok ? ApiResponse::ok() : ApiResponse::fail("Failed to start download"));
    }
}

void RatsAPI::downloadCancel(const QString& hash, ApiCallback callback)
{
    qInfo() << "API: downloadCancel hash:" << hash.left(16);
    
    if (!d->torrentClient) {
        if (callback) callback(ApiResponse::fail("Torrent client not initialized"));
        return;
    }
    
    bool found = d->torrentClient->isDownloading(hash);
    if (found) {
        d->torrentClient->stopTorrent(hash);
        qInfo() << "Download cancelled:" << hash.left(16);
    }
    if (callback) {
        callback(found ? ApiResponse::ok() : ApiResponse::fail("Download not found"));
    }
}

void RatsAPI::downloadUpdate(const QString& hash,
                              const QJsonObject& options,
                              ApiCallback callback)
{
    if (!d->torrentClient) {
        if (callback) callback(ApiResponse::fail("Torrent client not initialized"));
        return;
    }
    
    if (!d->torrentClient->isDownloading(hash)) {
        if (callback) callback(ApiResponse::fail("Download not found"));
        return;
    }
    
    bool ok = true;
    
    if (options.contains("pause")) {
        QString pauseVal = options["pause"].toString();
        if (pauseVal == "switch") {
            ok = d->torrentClient->togglePause(hash);
        } else {
            if (options["pause"].toBool()) {
                ok = d->torrentClient->pauseTorrent(hash);
            } else {
                ok = d->torrentClient->resumeTorrent(hash);
            }
        }
    }
    
    if (options.contains("removeOnDone")) {
        QString rodVal = options["removeOnDone"].toString();
        if (rodVal == "switch") {
            // Toggle removeOnDone
            ActiveTorrent torrent = d->torrentClient->getTorrent(hash);
            d->torrentClient->setRemoveOnDone(hash, !torrent.removeOnDone);
        } else {
            d->torrentClient->setRemoveOnDone(hash, options["removeOnDone"].toBool());
        }
    }
    
    if (callback) {
        ActiveTorrent torrent = d->torrentClient->getTorrent(hash);
        QJsonObject result;
        result["paused"] = torrent.paused;
        result["removeOnDone"] = torrent.removeOnDone;
        callback(ApiResponse::ok(result));
    }
}

void RatsAPI::downloadSelectFiles(const QString& hash,
                                   const QJsonArray& files,
                                   ApiCallback callback)
{
    if (!d->torrentClient) {
        if (callback) callback(ApiResponse::fail("Torrent client not initialized"));
        return;
    }
    
    bool ok = d->torrentClient->selectFilesJson(hash, files);
    if (callback) {
        callback(ok ? ApiResponse::ok() : ApiResponse::fail("Failed to select files"));
    }
}

void RatsAPI::getDownloads(ApiCallback callback)
{
    if (!d->torrentClient) {
        if (callback) callback(ApiResponse::ok(QJsonArray()));
        return;
    }
    
    if (callback) {
        callback(ApiResponse::ok(d->torrentClient->toJsonArray()));
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
    
    if (!changed.isEmpty()) {
        qInfo() << "API: config changed keys:" << changed.join(", ");
    }
    
    QJsonObject result;
    result["changed"] = QJsonArray::fromStringList(changed);
    
    if (callback) callback(ApiResponse::ok(result));
}

// ============================================================================
// Torrent Operations Implementation
// ============================================================================

void RatsAPI::vote(const QString& hash, bool isGood, ApiCallback callback)
{
    qInfo() << "API: vote hash:" << hash.left(16) << "isGood:" << isGood;
    
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

        qInfo() << "Already voted - returning current vote counts from P2P store" << votes.toJson();
        qInfo() << "Good:" << votes.good << "Bad:" << votes.bad << "SelfVoted:" << votes.selfVoted;

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
        qInfo() << "Storing vote in P2P network for" << hash.left(8) << "with torrent data" << torrentData;
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
    qInfo() << "Updating local database counts for" << hash.left(8) << "with good:" << torrent.good << "bad:" << torrent.bad;
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
    qInfo() << "Returning result for" << hash.left(8) << "with good:" << goodCount << "bad:" << badCount << "selfVoted:" << true << "distributed:" << storedInP2P;
    
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

TorrentClient* RatsAPI::getTorrentClient() const
{
    return d->torrentClient;
}

void RatsAPI::checkTrackers(const QString& hash, ApiCallback callback)
{
    if (hash.length() != 40) {
        if (callback) callback(ApiResponse::fail("Invalid hash"));
        return;
    }
    
    if (!d->trackerWrapper) {
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
    
    // Scrape from multiple trackers and get best result (using librats via TrackerWrapper)
    d->trackerWrapper->scrapeMultiple(hash, [this, hash, callback](const TrackerResult& result) {
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
                    << "leechers:" << result.leechers << "completed:" << result.completed;
        } else {
            response["status"] = "failed";
            response["error"] = result.error.isEmpty() ? "No tracker responded" : result.error;
        }
        
        if (callback) callback(ApiResponse::ok(response));
    });
}

void RatsAPI::removeTorrents(bool checkOnly, ApiCallback callback)
{
    qInfo() << "API: removeTorrents checkOnly:" << checkOnly;
    
    if (!d->database) {
        if (callback) callback(ApiResponse::fail("Database not initialized"));
        return;
    }
    
    // Run cleanup in background thread (like legacy api.js:923-961)
    (void)QtConcurrent::run([this, checkOnly, callback]() {
        QVector<QString> toRemove;
        int checked = 0;
        
        // Query torrents in batches to avoid memory issues
        const int batchSize = 1000;
        qint64 lastId = 0;
        
        while (true) {
            QString querySql = QString("SELECT * FROM torrents WHERE id > %1 ORDER BY id ASC LIMIT %2")
                               .arg(lastId).arg(batchSize);
            
            SphinxQL::Results rows = d->database->sphinxQL()->query(querySql);
            
            if (rows.isEmpty()) {
                break;  // No more torrents
            }
            
            for (const QVariantMap& row : rows) {
                // Convert to TorrentInfo
                // Note: Manticore returns all field names in lowercase!
                TorrentInfo torrent;
                torrent.id = row["id"].toLongLong();
                torrent.hash = row["hash"].toString();
                torrent.name = row["name"].toString();
                torrent.size = row["size"].toLongLong();
                torrent.files = row["files"].toInt();
                torrent.piecelength = row["piecelength"].toInt();
                torrent.contentTypeId = row["contenttype"].toInt();      // lowercase!
                torrent.contentCategoryId = row["contentcategory"].toInt(); // lowercase!
                
                lastId = torrent.id;
                checked++;
                
                // Check if torrent passes filters
                QString rejectionReason = getTorrentRejectionReason(torrent);
                if (!rejectionReason.isEmpty()) {
                    toRemove.append(torrent.hash);
                    qInfo() << "Cleanup: Marking torrent for removal:" << torrent.hash.left(8) 
                            << torrent.name.left(40) << "-" << rejectionReason;
                    
                    // Emit progress periodically
                    if (toRemove.size() % 100 == 0) {
                        QMetaObject::invokeMethod(this, [this, checked, count = toRemove.size()]() {
                            emit cleanupProgress(checked, count, "check");
                        }, Qt::QueuedConnection);
                    }
                }
            }
            
            if (rows.size() < batchSize) {
                break;  // Last batch
            }
        }
        
        qInfo() << "Cleanup: Found" << toRemove.size() << "torrents to remove out of" << checked << "checked";
        
        int removed = 0;
        
        // Actually remove if not checkOnly
        if (!checkOnly && !toRemove.isEmpty()) {
            for (const QString& hash : toRemove) {
                if (d->database->removeTorrent(hash)) {
                    removed++;
                    
                    // Emit progress periodically
                    if (removed % 100 == 0) {
                        QMetaObject::invokeMethod(this, [this, removed, total = toRemove.size()]() {
                            emit cleanupProgress(removed, total, "clean");
                        }, Qt::QueuedConnection);
                    }
                }
            }
            
            qInfo() << "Cleanup: Removed" << removed << "torrents";
        }
        
        QJsonObject result;
        result["checked"] = checked;
        result["found"] = toRemove.size();
        result["removed"] = removed;
        result["checkOnly"] = checkOnly;
        
        QMetaObject::invokeMethod(this, [callback, result]() {
            if (callback) callback(ApiResponse::ok(result));
        }, Qt::QueuedConnection);
    });
}

void RatsAPI::addTorrentFile(const QString& filePath, ApiCallback callback)
{
#ifdef RATS_SEARCH_FEATURES
    qInfo() << "API: addTorrentFile" << QFileInfo(filePath).fileName();
    
    if (filePath.isEmpty()) {
        if (callback) callback(ApiResponse::fail("Empty file path"));
        return;
    }
    
    // Read the torrent file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (callback) callback(ApiResponse::fail("Failed to open torrent file: " + file.errorString()));
        return;
    }
    
    QByteArray torrentData = file.readAll();
    file.close();
    
    if (torrentData.isEmpty()) {
        if (callback) callback(ApiResponse::fail("Empty torrent file"));
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
        librats::info_hash_to_hex(libratsTorrent.info_hash()));
    
    // Use helper to create TorrentInfo from librats
    TorrentInfo torrent = createTorrentFromLibrats(hash, libratsTorrent);
    
    // Use centralized insert logic
    InsertResult insertResult = processAndInsertTorrent(torrent);
    
    if (!insertResult.success && !insertResult.alreadyExists) {
        if (callback) callback(ApiResponse::fail(insertResult.error));
        return;
    }
    
    qInfo() << "Imported torrent:" << insertResult.torrent.name.left(50) << "hash:" << hash.left(8);
    
    QJsonObject result = torrentInfoToJson(insertResult.torrent);
    result["alreadyExists"] = insertResult.alreadyExists;
    result["imported"] = !insertResult.alreadyExists;
    
    if (callback) callback(ApiResponse::ok(result));
#else
    Q_UNUSED(filePath);
    if (callback) callback(ApiResponse::fail("BitTorrent features not enabled"));
#endif
}

void RatsAPI::createTorrent(const QString& path,
                            bool startSeeding,
                            const QStringList& trackers,
                            const QString& comment,
                            TorrentCreationProgressCallback progressCallback,
                            ApiCallback callback)
{
#ifdef RATS_SEARCH_FEATURES
    qInfo() << "API: createTorrent" << path << "seeding:" << startSeeding;
    
    if (path.isEmpty()) {
        if (callback) callback(ApiResponse::fail("Empty path"));
        return;
    }
    
    if (!d->database) {
        if (callback) callback(ApiResponse::fail("Database not initialized"));
        return;
    }
    
    // Check if path exists
    QFileInfo pathInfo(path);
    if (!pathInfo.exists()) {
        if (callback) callback(ApiResponse::fail("Path does not exist: " + path));
        return;
    }
    
    // Prepare torrents directory in app data folder
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString torrentsDir = dataPath + "/torrents";
    QDir dir(torrentsDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QString hash;
    QString torrentFilePath;  // Path where .torrent file is saved
    
    if (startSeeding && d->torrentClient && d->torrentClient->isReady()) {
        // Create and start seeding using TorrentClient
        TorrentClient::CreationProgressCallback tcCallback = nullptr;
        if (progressCallback) {
            tcCallback = [progressCallback](int current, int total) {
                progressCallback(current, total);
            };
        }
        
        hash = d->torrentClient->createAndSeedTorrent(path, trackers, comment, tcCallback);
        
        if (hash.isEmpty()) {
            if (callback) callback(ApiResponse::fail("Failed to create torrent"));
            return;
        }
        
        // Also save .torrent file to torrents directory
        // Get the torrent name for the filename
        ActiveTorrent active = d->torrentClient->getTorrent(hash);
        QString torrentName = active.name.isEmpty() ? hash : active.name;
        
        // Sanitize filename (remove invalid characters)
        torrentName.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");
        if (torrentName.length() > 200) {
            torrentName = torrentName.left(200);
        }
        
        torrentFilePath = torrentsDir + "/" + torrentName + ".torrent";
        
        // Use TorrentClient to create the .torrent file
        bool fileSaved = d->torrentClient->createTorrentFile(
            path, torrentFilePath, trackers, comment, nullptr);
        
        if (!fileSaved) {
            qWarning() << "Failed to save .torrent file to:" << torrentFilePath;
            torrentFilePath.clear();  // Clear path since save failed
        } else {
            qInfo() << "Saved .torrent file to:" << torrentFilePath;
        }
    } else {
        // Just create the torrent info without seeding
        // Use librats directly
        librats::TorrentCreatorConfig config;
        config.comment = comment.toStdString();
        config.created_by = "Rats Search";
        
        librats::TorrentCreator creator(path.toStdString(), config);
        
        if (creator.num_files() == 0) {
            if (callback) callback(ApiResponse::fail("No files found at path"));
            return;
        }
        
        for (const QString& tracker : trackers) {
            creator.add_tracker(tracker.toStdString());
        }
        
        // Convert progress callback
        librats::PieceHashProgressCallback hashCallback = nullptr;
        if (progressCallback) {
            hashCallback = [progressCallback](uint32_t current, uint32_t total) {
                progressCallback(static_cast<int>(current), static_cast<int>(total));
            };
        }
        
        librats::TorrentCreateError error;
        if (!creator.set_piece_hashes(hashCallback, &error)) {
            if (callback) callback(ApiResponse::fail("Failed to compute piece hashes: " + QString::fromStdString(error.message)));
            return;
        }
        
        auto torrentInfoOpt = creator.generate_torrent_info(&error);
        if (!torrentInfoOpt) {
            if (callback) callback(ApiResponse::fail("Failed to generate torrent: " + QString::fromStdString(error.message)));
            return;
        }
        
        hash = QString::fromStdString(torrentInfoOpt->info_hash_hex()).toLower();
        
        // Save .torrent file to torrents directory
        QString torrentName = QString::fromStdString(torrentInfoOpt->name());
        if (torrentName.isEmpty()) {
            torrentName = hash;
        }
        
        // Sanitize filename
        torrentName.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");
        if (torrentName.length() > 200) {
            torrentName = torrentName.left(200);
        }
        
        torrentFilePath = torrentsDir + "/" + torrentName + ".torrent";
        
        if (!creator.save_to_file(torrentFilePath.toStdString(), &error)) {
            qWarning() << "Failed to save .torrent file:" << QString::fromStdString(error.message);
            torrentFilePath.clear();
        } else {
            qInfo() << "Saved .torrent file to:" << torrentFilePath;
        }
    }
    
    // Add to database for searching
    // Build TorrentInfo based on source (TorrentClient if seeding, or librats)
    TorrentInfo torrent;
    torrent.hash = hash;
    torrent.added = QDateTime::currentDateTime();
    
    if (startSeeding && d->torrentClient) {
        ActiveTorrent active = d->torrentClient->getTorrent(hash);
        torrent.name = active.name;
        torrent.size = active.totalSize;
        torrent.files = active.files.size();
        
        // Build file list
        for (const TorrentFileInfo& f : active.files) {
            TorrentFile tf;
            tf.path = f.path;
            tf.size = f.size;
            torrent.filesList.append(tf);
        }
    } else {
        // Recreate TorrentInfo from librats (we don't have it from above due to scope)
        librats::TorrentCreatorConfig config;
        librats::TorrentCreator creator(path.toStdString(), config);
        
        librats::TorrentCreateError error;
        creator.set_piece_hashes(nullptr, &error);
        auto torrentInfoOpt = creator.generate_torrent_info(&error);
        
        if (torrentInfoOpt) {
            torrent.name = QString::fromStdString(torrentInfoOpt->name());
            torrent.size = static_cast<qint64>(torrentInfoOpt->total_size());
            torrent.files = static_cast<int>(torrentInfoOpt->num_files());
            torrent.piecelength = static_cast<int>(torrentInfoOpt->piece_length());
            
            const auto& files = torrentInfoOpt->files().files();
            for (const auto& f : files) {
                TorrentFile tf;
                tf.path = QString::fromStdString(f.path);
                tf.size = static_cast<qint64>(f.size);
                torrent.filesList.append(tf);
            }
        }
    }
    
    // Use centralized insert logic (handles content type detection, filters, DB insert, signal)
    InsertResult insertResult = processAndInsertTorrent(torrent);
    
    if (!insertResult.success && !insertResult.alreadyExists) {
        if (callback) callback(ApiResponse::fail(insertResult.error));
        return;
    }
    
    qInfo() << "Torrent created:" << insertResult.torrent.name.left(50) << "hash:" << hash.left(16) 
            << "size:" << (insertResult.torrent.size / (1024*1024)) << "MB"
            << (startSeeding ? "seeding" : "indexed");
    
    QJsonObject result = torrentInfoToJson(insertResult.torrent);
    result["alreadyExists"] = insertResult.alreadyExists;
    result["created"] = !insertResult.alreadyExists;
    result["seeding"] = startSeeding;
    
    // Include path to saved .torrent file if it was created
    if (!torrentFilePath.isEmpty()) {
        result["torrentFile"] = torrentFilePath;
    }
    
    if (callback) callback(ApiResponse::ok(result));
#else
    Q_UNUSED(path);
    Q_UNUSED(startSeeding);
    Q_UNUSED(trackers);
    Q_UNUSED(comment);
    Q_UNUSED(progressCallback);
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
        qInfo() << "P2P search query too short, ignoring";
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
        // Get feed items and ensure files are included for proper replication
        QJsonArray feedArray = d->feedManager->toJsonArray();
        
        // Enrich feed items with files if missing (for old feed items without files)
        // Use batch query to avoid N+1 queries problem
        if (d->database) {
            // First pass: collect hashes that need files lookup
            QStringList hashesNeedingFiles;
            QHash<QString, int> hashToIndex;
            
            for (int i = 0; i < feedArray.size(); ++i) {
                QJsonObject item = feedArray[i].toObject();
                
                // Check if this item needs files
                if (!item.contains("filesList") || item["filesList"].toArray().isEmpty()) {
                    QString hash = item["hash"].toString();
                    if (hash.length() == 40) {
                        hashesNeedingFiles.append(hash);
                        hashToIndex[hash] = i;
                    }
                }
            }
            
            // Single batch query to get all missing files
            if (!hashesNeedingFiles.isEmpty()) {
                QHash<QString, QVector<TorrentFile>> filesMap = 
                    d->database->getFilesForTorrents(hashesNeedingFiles);
                
                // Update feed items with the retrieved files
                for (auto it = filesMap.begin(); it != filesMap.end(); ++it) {
                    const QString& hash = it.key();
                    const QVector<TorrentFile>& files = it.value();
                    
                    int idx = hashToIndex.value(hash, -1);
                    if (idx >= 0 && !files.isEmpty()) {
                        QJsonObject item = feedArray[idx].toObject();
                        QJsonArray filesArray;
                        for (const TorrentFile& f : files) {
                            QJsonObject fileObj;
                            fileObj["path"] = f.path;
                            fileObj["size"] = f.size;
                            filesArray.append(fileObj);
                        }
                        item["filesList"] = filesArray;
                        feedArray[idx] = item;
                    }
                }
            }
        }
        
        response["feed"] = feedArray;
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

bool RatsAPI::insertTorrentFromFeed(const QJsonObject& torrentData)
{
    // Use helper to create TorrentInfo from JSON
    TorrentInfo torrent = createTorrentFromJson(torrentData);
    
    if (torrent.hash.length() != 40) {
        return false;
    }
    
    // Use centralized insert logic
    // detectContentType=true only if not already set in JSON
    // emitSignal=false for replication (avoid UI spam)
    InsertResult result = processAndInsertTorrent(torrent, true, false);
    
    if (result.success && !result.alreadyExists) {
        qInfo() << "Inserted torrent from feed:" << result.torrent.name.left(30) 
                << "with" << result.torrent.filesList.size() << "files";
        return true;
    } else if (!result.success) {
        qInfo() << "Rejected torrent from feed:" << torrent.name.left(30) << "-" << result.error;
    }
    
    return false;
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

void RatsAPI::handleP2PTorrentResponse(const QString& peerId, const QJsonObject& data)
{
    // Received torrent data from a remote peer (response to our 'torrent' request)
    // This should be saved to our database for replication
    
    QString hash = data["hash"].toString();
    if (hash.isEmpty()) {
        hash = data["info_hash"].toString();
    }
    
    if (hash.length() != 40) {
        qWarning() << "Invalid torrent_response from" << peerId.left(8) << "- invalid hash";
        return;
    }
    
    QString name = data["name"].toString();
    if (name.isEmpty()) {
        qWarning() << "Invalid torrent_response from" << peerId.left(8) << "- empty name";
        return;
    }
    
    qInfo() << "Received torrent_response from" << peerId.left(8) << ":" << name.left(50) 
            << "hash:" << hash.left(16);
    
    // First, emit signal for UI to receive the data (even if torrent already exists)
    // This is important for getTorrent requests that need the response
    QJsonObject torrentData = data;
    torrentData["peer"] = peerId;
    torrentData["remote"] = true;
    emit remoteTorrentReceived(hash, torrentData);
    
    // Use the standard insertion method to save the torrent
    // This handles content type detection, filtering, and proper database insertion
    if (insertTorrentFromFeed(data)) {
        qInfo() << "P2P: Saved torrent from peer" << peerId.left(8) << ":" << name.left(40);
        
        // Track replication statistics
        onReplicationTorrentReceived();
        
        // Emit signals for UI updates
        emit torrentIndexed(hash, name);
        emit replicationProgress(1, d->totalReplicatedTorrents);
    } else {
        qDebug() << "P2P: Torrent from peer" << peerId.left(8) 
                 << "already exists or was rejected:" << hash.left(16);
    }
}

void RatsAPI::handleP2PPeerConnected(const QString& peerId)
{
    qInfo() << "P2P: Peer connected:" << peerId.left(16);
    
    // Request feed from newly connected peer (for P2P feed sync)
    if (d->p2p && d->feedManager) {
        qDebug() << "Requesting feed from new peer" << peerId.left(8);
        
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
        qInfo() << "P2P replication server disabled, ignoring randomTorrents request";
        return;
    }
    
    int limit = data["limit"].toInt(5);
    // Limit based on server load (like legacy)
    limit = qBound(1, limit, 10);
    
    qInfo() << "Processing P2P randomTorrents request from" << peerId.left(8) << "limit:" << limit;
    
    // Include files for replication (like legacy api.js that queries files table)
    QVector<TorrentInfo> torrents = d->database->getRandomTorrents(limit, true);
    
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

// ============================================================================
// P2P Replication (like legacy api.js:247-272)
// ============================================================================

void RatsAPI::startReplication()
{
    if (!d->replicationTimer || !d->config) {
        return;
    }
    
    if (!d->config->p2pReplication()) {
        qInfo() << "P2P: Replication disabled in config";
        return;
    }
    
    if (d->replicationTimer->isActive()) {
        qDebug() << "Replication timer already running";
        return;
    }
    
    d->replicationInterval = 5000;  // Reset to 5 seconds
    d->replicationTorrentsReceived = 0;
    d->replicationTimer->start(d->replicationInterval);
    
    qInfo() << "P2P: Replication started, interval:" << d->replicationInterval << "ms";
    emit replicationStarted();
}

void RatsAPI::stopReplication()
{
    if (!d->replicationTimer) {
        return;
    }
    
    if (d->replicationTimer->isActive()) {
        d->replicationTimer->stop();
        qInfo() << "P2P: Replication stopped, total replicated:" << d->totalReplicatedTorrents;
        emit replicationStopped();
    }
}

bool RatsAPI::isReplicationActive() const
{
    return d->replicationTimer && d->replicationTimer->isActive();
}

qint64 RatsAPI::replicationStats() const
{
    return d->totalReplicatedTorrents;
}

void RatsAPI::performReplicationCycle()
{
    if (!d->p2p || !d->database || !d->config) {
        return;
    }
    
    // Check if still enabled
    if (!d->config->p2pReplication()) {
        stopReplication();
        return;
    }
    
    int peerCount = d->p2p->getPeerCount();
    if (peerCount == 0) {
        qDebug() << "Replication: No peers connected, skipping cycle";
        return;
    }
    
    qDebug() << "Replication cycle: requesting from" << peerCount << "peers";
    
    // Reset counter for this cycle
    d->replicationTorrentsReceived = 0;
    
    // Broadcast randomTorrents request to all connected peers
    QJsonObject request;
    request["limit"] = 5;  // Request up to 5 torrents per peer
    request["version"] = "2.0";  // Protocol version for compatibility check
    
    d->p2p->broadcastMessage("randomTorrents", request);
    
    // Adaptive interval calculation (like legacy getReplicationTorrents)
    // If we received many torrents in the last cycle, slow down
    // If we received few, keep the faster interval
    QTimer::singleShot(3000, this, [this]() {
        int received = d->replicationTorrentsReceived;
        
        if (received > 8) {
            // Many torrents received, slow down (like legacy: gotTorrents * 600)
            d->replicationInterval = qMin(60000, received * 600);  // Max 60 seconds
        } else {
            // Few or no torrents, use faster interval
            d->replicationInterval = 10000;  // 10 seconds
        }
        
        // Update timer interval for next cycle
        if (d->replicationTimer && d->replicationTimer->isActive()) {
            d->replicationTimer->setInterval(d->replicationInterval);
        }
        
        if (received > 0) {
            d->totalReplicatedTorrents += received;
            qInfo() << "P2P: Replicated" << received << "torrents, total:" 
                    << d->totalReplicatedTorrents << "next interval:" << d->replicationInterval << "ms";
        }
    });
}

void RatsAPI::onReplicationTorrentReceived()
{
    // Called when a torrent is successfully inserted from replication
    d->replicationTorrentsReceived++;
}
