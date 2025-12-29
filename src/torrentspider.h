#ifndef TORRENTSPIDER_H
#define TORRENTSPIDER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <memory>
#include <atomic>
#include <queue>
#include <mutex>
#include <set>

// Forward declarations
class TorrentDatabase;
class P2PNetwork;

namespace librats {
    class RatsClient;
}

/**
 * @brief TorrentSpider - DHT spider for discovering torrents
 * 
 * Uses librats DHT spider mode (via P2PNetwork's RatsClient) to:
 * 1. Walk the DHT network discovering nodes
 * 2. Capture announce_peer messages from other clients
 * 3. Fetch torrent metadata for discovered info hashes
 * 4. Store torrent information in the database
 * 
 * NOTE: This class does NOT own a RatsClient. It uses the one from P2PNetwork.
 */
class TorrentSpider : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param database Pointer to torrent database
     * @param p2pNetwork Pointer to P2P network (provides RatsClient)
     * @param parent Parent QObject
     */
    explicit TorrentSpider(TorrentDatabase *database, P2PNetwork *p2pNetwork, QObject *parent = nullptr);
    ~TorrentSpider();

    /**
     * @brief Start the spider
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the spider
     */
    void stop();

    /**
     * @brief Check if spider is running
     */
    bool isRunning() const;

    /**
     * @brief Get number of torrents indexed
     */
    int getIndexedCount() const;

    /**
     * @brief Get number of pending metadata fetches
     */
    int getPendingCount() const;

    /**
     * @brief Set spider walk interval (ms)
     */
    void setWalkInterval(int intervalMs);

    /**
     * @brief Get current walk interval
     */
    int getWalkInterval() const;

    /**
     * @brief Set ignore interval (ms) - rate limiting
     */
    void setIgnoreInterval(int intervalMs);

    /**
     * @brief Enable/disable metadata fetching
     */
    void setMetadataFetchEnabled(bool enabled);

    /**
     * @brief Get DHT routing table size
     */
    size_t getDhtNodeCount() const;

    /**
     * @brief Get spider pool size (separate from routing table)
     */
    size_t getSpiderPoolSize() const;

    /**
     * @brief Get number of visited nodes in spider mode
     */
    size_t getSpiderVisitedCount() const;

signals:
    void started();
    void stopped();
    void statusChanged(const QString& status);
    void torrentDiscovered(const QString& infoHash);
    void torrentIndexed(const QString& infoHash, const QString& name);
    void indexedCountChanged(int count);
    void error(const QString& message);

private slots:
    void onSpiderWalk();
    void onIgnoreToggle();
    void processMetadataQueue();

private:
    /**
     * @brief Get RatsClient from P2PNetwork
     */
    librats::RatsClient* getRatsClient() const;

    /**
     * @brief Handle announce_peer callback from DHT
     */
    void onAnnounce(const std::array<uint8_t, 20>& infoHash, 
                    const std::string& ip, uint16_t port);

    /**
     * @brief Fetch metadata for an info hash
     * @param infoHash The info hash to fetch metadata for
     * @param peerIp IP address of peer to connect to (optional, for fast path)
     * @param peerPort Port of peer to connect to (optional, for fast path)
     * 
     * If peerIp and peerPort are provided, connects directly to that peer (fast path).
     * Otherwise, uses DHT to discover peers (slow path).
     */
    void fetchMetadata(const QString& infoHash, const QString& peerIp = QString(), uint16_t peerPort = 0);

    /**
     * @brief Handle metadata retrieval result
     */
    void onMetadataReceived(const QString& infoHash, 
                           const QString& name,
                           qint64 size,
                           int files,
                           int pieceLength,
                           const QVector<QPair<QString, qint64>>& filesList);

    TorrentDatabase* database_;
    P2PNetwork* p2pNetwork_;  // Not owned - just a reference
    
    std::atomic<bool> running_;
    std::atomic<int> indexedCount_;
    std::atomic<int> pendingCount_;
    
    // Timers
    QTimer* walkTimer_;
    QTimer* ignoreTimer_;
    QTimer* metadataQueueTimer_;
    
    int walkIntervalMs_;
    int ignoreIntervalMs_;
    bool metadataFetchEnabled_;
    
    // Queue for metadata fetching
    std::queue<QString> metadataQueue_;
    std::mutex queueMutex_;
    static const int MAX_CONCURRENT_METADATA_FETCHES = 10;
    std::atomic<int> activeFetches_;
    
    // Set of recently seen hashes to avoid duplicates
    std::set<QString> recentHashes_;
    std::mutex recentHashesMutex_;
    static const size_t MAX_RECENT_HASHES = 10000;
};

#endif // TORRENTSPIDER_H
