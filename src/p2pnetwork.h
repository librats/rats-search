#ifndef P2PNETWORK_H
#define P2PNETWORK_H

#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>
#include <memory>

namespace librats {
    class RatsClient;
}

/**
 * @brief P2PNetwork - Central owner of librats RatsClient for Qt integration
 * 
 * This class is the SINGLE owner of RatsClient. All other components
 * that need access to RatsClient should get it through this class.
 * 
 * Handles P2P networking using librats library, including:
 * - Peer discovery (DHT, mDNS)
 * - Torrent search in P2P network
 * - Torrent data exchange
 * - GossipSub pub-sub messaging
 * - BitTorrent functionality (when enabled)
 */
class P2PNetwork : public QObject
{
    Q_OBJECT

public:
    explicit P2PNetwork(int port, int dhtPort, const QString& dataDirectory, QObject *parent = nullptr);
    ~P2PNetwork();

    bool start();
    void stop();
    bool isRunning() const;
    bool isConnected() const;
    
    int getPeerCount() const;
    QString getOurPeerId() const;
    
    /**
     * @brief Get the RatsClient instance
     * @note This is the only RatsClient instance - do not create others!
     * @return Pointer to RatsClient, or nullptr if not started
     */
    librats::RatsClient* getRatsClient() const { return ratsClient_.get(); }
    
    /**
     * @brief Get DHT routing table size
     */
    size_t getDhtNodeCount() const;
    
    /**
     * @brief Check if DHT is running
     */
    bool isDhtRunning() const;
    
    /**
     * @brief Check if BitTorrent is enabled
     */
    bool isBitTorrentEnabled() const;
    
    /**
     * @brief Enable BitTorrent functionality
     */
    bool enableBitTorrent();
    
    /**
     * @brief Disable BitTorrent functionality
     */
    void disableBitTorrent();
    
    /**
     * @brief Enable spider mode for DHT
     * Spider mode enables aggressive node discovery and collects announce_peer requests
     * @param intervalMs Interval between spider_walk calls in milliseconds (default: 100ms)
     */
    void enableSpiderMode(int intervalMs = 100);
    
    /**
     * @brief Disable spider mode
     */
    void disableSpiderMode();
    
    /**
     * @brief Check if spider mode is enabled
     */
    bool isSpiderModeEnabled() const;
    
    // Search torrents in P2P network
    void searchTorrents(const QString& query);
    
    // Announce torrent
    void announceTorrent(const QString& infoHash, const QString& name);

signals:
    void started();
    void stopped();
    void statusChanged(const QString& status);
    void peerCountChanged(int count);
    void peerConnected(const QString& peerId);
    void peerDisconnected(const QString& peerId);
    
    // Search results from other peers
    void searchResultReceived(const QString& infoHash, const QString& name, 
                             qint64 size, int seeders, int leechers);
    
    void error(const QString& errorMessage);
    
    // Spider mode signals
    void spiderAnnounce(const QString& infoHash, const QString& peerAddress);

private slots:
    void updatePeerCount();
    void onSpiderWalkTimer();

private:
    void setupCallbacks();
    void setupGossipSub();
    
    std::unique_ptr<librats::RatsClient> ratsClient_;
    int port_;
    int dhtPort_;
    QString dataDirectory_;
    bool running_;
    bool bitTorrentEnabled_;
    bool spiderModeEnabled_;
    
    QTimer *updateTimer_;
    QTimer *spiderWalkTimer_;
};

#endif // P2PNETWORK_H

