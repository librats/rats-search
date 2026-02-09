#ifndef RATSAPI_H
#define RATSAPI_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <functional>
#include <memory>

// Include TorrentInfo for InsertResult struct (needs complete type)
#include "../torrentdatabase.h"

// Forward declarations
class P2PNetwork;
class TorrentClient;
class ConfigManager;
class FeedManager;
class P2PStoreManager;
class TrackerInfoScraper;

// librats forward declarations (only when RATS_SEARCH_FEATURES enabled)
#ifdef RATS_SEARCH_FEATURES
namespace librats {
    class TorrentInfo;
}
#endif

/**
 * @brief ApiResponse - Standard response wrapper for all API calls
 * 
 * Provides consistent response format across all API methods:
 * - success: whether the operation succeeded
 * - data: the response payload (object or array)
 * - error: error message if failed
 */
struct ApiResponse {
    bool success = true;
    QJsonValue data;  // Can be object or array
    QString error;
    QString requestId;  // For tracking async requests
    
    static ApiResponse ok(const QJsonValue& data = QJsonValue()) {
        ApiResponse r;
        r.success = true;
        r.data = data;
        return r;
    }
    
    static ApiResponse fail(const QString& error) {
        ApiResponse r;
        r.success = false;
        r.error = error;
        return r;
    }
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["success"] = success;
        if (!data.isNull() && !data.isUndefined()) {
            obj["data"] = data;
        }
        if (!error.isEmpty()) {
            obj["error"] = error;
        }
        if (!requestId.isEmpty()) {
            obj["requestId"] = requestId;
        }
        return obj;
    }
};

/**
 * @brief Callback types for async operations
 */
using ApiCallback = std::function<void(const ApiResponse&)>;

/**
 * @brief RatsAPI - Main API facade for rats-search
 * 
 * This is the central API class that provides:
 * - Unified interface for all operations
 * - Easy integration with REST server, WebSocket, CLI
 * - Consistent error handling
 * - Request/response tracking
 * 
 * Design principles:
 * 1. All methods use async callbacks for consistency
 * 2. Standard ApiResponse format for all responses
 * 3. JSON-based input/output for easy serialization
 * 4. Modular sub-APIs for different domains
 * 
 * Usage:
 *   api->search()->torrents("ubuntu", {{"limit", 10}}, [](const ApiResponse& r) {
 *       if (r.success) processResults(r.data.toArray());
 *   });
 */
class RatsAPI : public QObject
{
    Q_OBJECT

public:
    explicit RatsAPI(QObject *parent = nullptr);
    ~RatsAPI();
    
    /**
     * @brief Initialize the API with required dependencies
     * 
     * This method:
     * - Sets up all required dependencies
     * - Registers P2P message handlers for remote API calls
     * - Initializes download manager, feed manager, etc.
     */
    void initialize(TorrentDatabase* database,
                   P2PNetwork* p2p,
                   TorrentClient* torrentClient,
                   ConfigManager* config);
    
    /**
     * @brief Check if API is ready
     */
    bool isReady() const;
    
    // =========================================================================
    // P2P API (Remote peer requests - like legacy api.js)
    // =========================================================================
    
    /**
     * @brief Setup all P2P message handlers
     * 
     * This registers handlers for remote API calls from other peers:
     * - torrent_search / searchTorrent
     * - searchFiles
     * - topTorrents
     * - torrent (get single torrent)
     * - feed
     * - vote
     * 
     * Called automatically during initialize(), but can be called again
     * if P2P network is restarted.
     */
    void setupP2PHandlers();
    
    // =========================================================================
    // Generic API call (for REST/WebSocket routing)
    // =========================================================================
    
    /**
     * @brief Call an API method by name
     * 
     * This is the main entry point for external interfaces (REST, WebSocket).
     * Routes the call to appropriate handler based on method name.
     * 
     * @param method API method name (e.g., "search.torrents", "config.get")
     * @param params Method parameters as JSON object
     * @param callback Response callback
     * @param requestId Optional request ID for tracking
     * 
     * Example:
     *   api->call("search.torrents", {{"text", "ubuntu"}}, callback);
     *   api->call("downloads.add", {{"hash", "abc123"}}, callback);
     */
    void call(const QString& method,
              const QJsonObject& params,
              ApiCallback callback,
              const QString& requestId = QString());
    
    /**
     * @brief Get list of available API methods
     */
    QStringList availableMethods() const;
    
    // =========================================================================
    // Search API
    // =========================================================================
    
    /**
     * @brief Search torrents
     * @param text Search query
     * @param options {index, limit, orderBy, orderDesc, safeSearch, type, size, files}
     */
    void searchTorrents(const QString& text,
                        const QJsonObject& options,
                        ApiCallback callback);
    
    /**
     * @brief Search files within torrents
     */
    void searchFiles(const QString& text,
                     const QJsonObject& options,
                     ApiCallback callback);
    
    /**
     * @brief Get torrent by hash
     * @param hash 40-char hex hash
     * @param includeFiles Whether to include file list
     * @param remotePeer Optional peer address for remote fetch
     */
    void getTorrent(const QString& hash,
                    bool includeFiles,
                    const QString& remotePeer,
                    ApiCallback callback);
    
    /**
     * @brief Get recent torrents
     */
    void getRecentTorrents(int limit, ApiCallback callback);
    
    /**
     * @brief Get top torrents by seeders
     * @param type Content type filter
     * @param options {index, limit, time}
     */
    void getTopTorrents(const QString& type,
                        const QJsonObject& options,
                        ApiCallback callback);
    
    // =========================================================================
    // Download API
    // =========================================================================
    
    /**
     * @brief Start downloading a torrent
     * @param hash Torrent hash
     * @param savePath Optional save path
     */
    void downloadAdd(const QString& hash,
                     const QString& savePath,
                     ApiCallback callback);
    
    /**
     * @brief Cancel a download
     */
    void downloadCancel(const QString& hash, ApiCallback callback);
    
    /**
     * @brief Update download settings (pause/resume/removeOnDone)
     * @param options {pause, removeOnDone}
     */
    void downloadUpdate(const QString& hash,
                        const QJsonObject& options,
                        ApiCallback callback);
    
    /**
     * @brief Select files for download
     * @param files Array of file indices or map of {index: selected}
     */
    void downloadSelectFiles(const QString& hash,
                             const QJsonArray& files,
                             ApiCallback callback);
    
    /**
     * @brief Get list of all downloads with progress
     */
    void getDownloads(ApiCallback callback);
    
    // =========================================================================
    // Statistics API
    // =========================================================================
    
    /**
     * @brief Get database statistics
     * Returns: {torrents, files, size}
     */
    void getStatistics(ApiCallback callback);
    
    /**
     * @brief Get P2P peer information
     * Returns: {size, connected, torrents}
     */
    void getPeers(ApiCallback callback);
    
    /**
     * @brief Get P2P connection status
     * Returns: {connected, peerId, dhtNodes, ...}
     */
    void getP2PStatus(ApiCallback callback);
    
    // =========================================================================
    // Config API
    // =========================================================================
    
    /**
     * @brief Get current configuration
     */
    void getConfig(ApiCallback callback);
    
    /**
     * @brief Update configuration
     * @param options Key-value pairs to update
     */
    void setConfig(const QJsonObject& options, ApiCallback callback);
    
    // =========================================================================
    // Torrent Operations API
    // =========================================================================
    
    /**
     * @brief Vote on a torrent
     * 
     * Stores the vote both locally and in the distributed P2P store.
     * 
     * @param hash Torrent hash
     * @param isGood true for upvote, false for downvote
     */
    void vote(const QString& hash, bool isGood, ApiCallback callback);
    
    /**
     * @brief Get votes for a torrent
     * 
     * Aggregates votes from all peers via the distributed P2P store.
     * 
     * @param hash Torrent hash
     */
    void getVotes(const QString& hash, ApiCallback callback);
    
    /**
     * @brief Get the P2P store manager
     */
    P2PStoreManager* p2pStore() const;
    
    /**
     * @brief Get the torrent client for downloads
     */
    TorrentClient* getTorrentClient() const;
    
    /**
     * @brief Check/update tracker info for a torrent
     */
    void checkTrackers(const QString& hash, ApiCallback callback);
    
    /**
     * @brief Scrape tracker websites for torrent details
     * 
     * Queries tracker websites (RuTracker, Nyaa) for additional info about
     * a torrent: description, poster image, content category, tracker links.
     * Results are stored in the torrent's `info` JSON field.
     * 
     * @param hash Torrent info hash
     * @param callback Called with updated info. data contains the merged info JSON.
     */
    void scrapeTrackerInfo(const QString& hash, ApiCallback callback);
    
    /**
     * @brief Get the tracker info scraper
     */
    TrackerInfoScraper* trackerInfoScraper() const;
    
    /**
     * @brief Remove torrents matching filters
     * @param checkOnly If true, only count matching torrents
     */
    void removeTorrents(bool checkOnly, ApiCallback callback);
    
    /**
     * @brief Add a torrent file to the search index
     * 
     * Parses the .torrent file and adds it to the database for searching.
     * Does NOT start downloading or seeding.
     * 
     * @param filePath Path to the .torrent file
     */
    void addTorrentFile(const QString& filePath, ApiCallback callback);
    
    /**
     * @brief Callback for torrent creation progress
     */
    using TorrentCreationProgressCallback = std::function<void(int currentPiece, int totalPieces)>;
    
    /**
     * @brief Create a torrent from a file or directory
     * 
     * Creates a .torrent, indexes it in the database, and optionally starts seeding.
     * 
     * @param path Path to file or directory
     * @param startSeeding If true, start seeding immediately
     * @param trackers List of tracker URLs (optional)
     * @param comment Torrent comment (optional)
     * @param progressCallback Callback for progress updates (optional)
     */
    void createTorrent(const QString& path,
                       bool startSeeding,
                       const QStringList& trackers,
                       const QString& comment,
                       TorrentCreationProgressCallback progressCallback,
                       ApiCallback callback);
    
    /**
     * @brief Check if a torrent passes the configured filters
     * @param torrent Torrent info to check
     * @return true if torrent passes all filters, false if should be rejected
     */
    bool checkTorrentFilters(const TorrentInfo& torrent) const;
    
    /**
     * @brief Get reason why torrent was rejected
     * @param torrent Torrent info to check
     * @return Empty string if passes, rejection reason otherwise
     */
    QString getTorrentRejectionReason(const TorrentInfo& torrent) const;
    
    // =========================================================================
    // Feed API
    // =========================================================================
    
    /**
     * @brief Get feed (voted/popular torrents)
     * @param index Offset
     * @param limit Number of items
     */
    void getFeed(int index, int limit, ApiCallback callback);
    
    // =========================================================================
    // P2P Replication API (like legacy api.js:247-272)
    // =========================================================================
    
    /**
     * @brief Start continuous P2P replication
     * 
     * Starts a timer that periodically requests random torrents from peers.
     * The interval adapts based on how many torrents are received.
     */
    void startReplication();
    
    /**
     * @brief Stop continuous P2P replication
     */
    void stopReplication();
    
    /**
     * @brief Check if replication is currently active
     */
    bool isReplicationActive() const;
    
    /**
     * @brief Get total replicated torrents count
     */
    qint64 replicationStats() const;
    
signals:
    // =========================================================================
    // Events (for push notifications to clients)
    // =========================================================================
    
    /**
     * @brief Emitted when remote search results arrive (torrent name search)
     */
    void remoteSearchResults(const QString& searchId, const QJsonArray& torrents);
    
    /**
     * @brief Emitted when remote file search results arrive
     * These are results from searchFiles, containing highlighted matching paths
     */
    void remoteFileSearchResults(const QString& searchId, const QJsonArray& torrents);
    
    /**
     * @brief Emitted when remote top torrents arrive
     */
    void remoteTopTorrents(const QJsonArray& torrents, const QString& type, const QString& time);
    
    /**
     * @brief Emitted when download progress updates
     */
    void downloadProgress(const QString& hash, const QJsonObject& progress);
    
    /**
     * @brief Emitted when download completes
     */
    void downloadCompleted(const QString& hash, bool cancelled);
    
    /**
     * @brief Emitted when files are ready for selection
     */
    void filesReady(const QString& hash, const QJsonArray& files);
    
    /**
     * @brief Emitted when config changes
     */
    void configChanged(const QJsonObject& config);
    
    /**
     * @brief Emitted when tracker info is scraped for a torrent
     * @param hash Torrent info hash
     * @param info Merged tracker info JSON (poster, description, trackers, etc.)
     */
    void trackerInfoUpdated(const QString& hash, const QJsonObject& info);
    
    /**
     * @brief Emitted when votes are updated
     */
    void votesUpdated(const QString& hash, int good, int bad);
    
    /**
     * @brief Emitted when feed is updated
     */
    void feedUpdated(const QJsonArray& feed);
    
    /**
     * @brief Emitted when a torrent is indexed
     */
    void torrentIndexed(const QString& hash, const QString& name);
    
    /**
     * @brief Emitted during cleanup
     */
    void cleanupProgress(int current, int total, const QString& phase);
    
    /**
     * @brief Emitted when P2P replication starts
     */
    void replicationStarted();
    
    /**
     * @brief Emitted when P2P replication stops
     */
    void replicationStopped();
    
    /**
     * @brief Emitted when torrents are replicated from peers
     * @param replicated Number of torrents replicated in this batch
     * @param total Total number of replicated torrents since start
     */
    void replicationProgress(int replicated, qint64 total);
    
    /**
     * @brief Emitted when a torrent is received from a remote peer
     * 
     * This is emitted in response to a getTorrent request to a remote peer.
     * Contains the full torrent data including file list.
     * 
     * @param hash Torrent hash
     * @param torrentData Full torrent JSON including filesList
     */
    void remoteTorrentReceived(const QString& hash, const QJsonObject& torrentData);

private:
    class Private;
    std::unique_ptr<Private> d;
    
    // Method routing table
    void registerMethods();
    using MethodHandler = std::function<void(const QJsonObject&, ApiCallback)>;
    QHash<QString, MethodHandler> methods_;
    
    // =========================================================================
    // P2P Message Handlers (remote peer requests)
    // These handle incoming requests from other peers and send responses
    // =========================================================================
    
    void handleP2PSearchRequest(const QString& peerId, const QJsonObject& data);
    void handleP2PSearchFilesRequest(const QString& peerId, const QJsonObject& data);
    void handleP2PTopTorrentsRequest(const QString& peerId, const QJsonObject& data);
    void handleP2PTorrentRequest(const QString& peerId, const QJsonObject& data);
    void handleP2PTorrentResponse(const QString& peerId, const QJsonObject& data);
    void handleP2PFeedRequest(const QString& peerId, const QJsonObject& data);
    void handleP2PSearchResult(const QString& peerId, const QJsonObject& data);
    void handleP2PTorrentAnnounce(const QString& peerId, const QJsonObject& data);
    void handleP2PFeedResponse(const QString& peerId, const QJsonObject& data);
    void handleP2PRandomTorrentsRequest(const QString& peerId, const QJsonObject& data);
    void handleP2PPeerConnected(const QString& peerId);
    
    // Helper to convert TorrentInfo to JSON for P2P responses
    static QJsonObject torrentToP2PJson(const TorrentInfo& torrent);
    
    // =========================================================================
    // Torrent Processing Helpers (centralized insertion logic)
    // =========================================================================
    
    /**
     * @brief Result of processAndInsertTorrent operation
     */
    struct InsertResult {
        bool success = false;
        bool alreadyExists = false;
        QString error;
        TorrentInfo torrent;
    };
    
    /**
     * @brief Process and insert a torrent into the database
     * 
     * This is the central method for all torrent insertions. It:
     * 1. Detects content type if not set
     * 2. Checks filters
     * 3. Inserts into database
     * 4. Emits torrentIndexed signal
     * 
     * @param torrent TorrentInfo to insert (will be modified with detected content type)
     * @param detectContentType Whether to auto-detect content type
     * @param emitSignal Whether to emit torrentIndexed signal
     * @return InsertResult with success status and error message if failed
     */
    InsertResult processAndInsertTorrent(TorrentInfo& torrent, 
                                          bool detectContentType = true,
                                          bool emitSignal = true);
    
#ifdef RATS_SEARCH_FEATURES
    /**
     * @brief Create TorrentInfo from librats::TorrentInfo
     * 
     * Converts librats torrent metadata to our TorrentInfo structure.
     * Used for DHT metadata lookups and .torrent file parsing.
     * 
     * @param hash 40-char hex info hash
     * @param libratsTorrent librats torrent info
     * @return Populated TorrentInfo
     */
    static TorrentInfo createTorrentFromLibrats(const QString& hash, 
                                                 const librats::TorrentInfo& libratsTorrent);
#endif
    
    /**
     * @brief Create TorrentInfo from JSON object
     * 
     * Converts a JSON torrent representation to TorrentInfo.
     * Used for P2P replication and feed sync.
     * 
     * @param data JSON object with torrent data
     * @return Populated TorrentInfo
     */
    static TorrentInfo createTorrentFromJson(const QJsonObject& data);
    
    // Helper to insert a torrent from feed replication (uses processAndInsertTorrent)
    // Returns true if torrent was inserted, false if already exists or rejected
    bool insertTorrentFromFeed(const QJsonObject& torrentData);
    
    // =========================================================================
    // Replication Timer (private)
    // =========================================================================
    
    /**
     * @brief Perform one cycle of replication
     * Called periodically by the replication timer
     */
    void performReplicationCycle();
    
    /**
     * @brief Called when a torrent is received via replication
     */
    void onReplicationTorrentReceived();
};

#endif // RATSAPI_H

