#ifndef P2PNETWORK_H
#define P2PNETWORK_H

#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <functional>

namespace librats {
    class RatsClient;
}

/**
 * @brief P2PNetwork - Transport layer for P2P communication
 * 
 * This class is purely a transport layer - it handles:
 * - Managing librats RatsClient lifecycle
 * - Peer discovery (DHT, mDNS)
 * - Sending/receiving messages
 * - GossipSub pub-sub messaging
 * 
 * All business logic (search handling, response generation) belongs in RatsAPI.
 * P2PNetwork emits signals when messages arrive, and RatsAPI handles them.
 */
class P2PNetwork : public QObject
{
    Q_OBJECT

public:
    explicit P2PNetwork(int port, int dhtPort, const QString& dataDirectory, QObject *parent = nullptr);
    ~P2PNetwork();

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool start();
    void stop();
    bool isRunning() const;
    bool isConnected() const;
    
    // =========================================================================
    // Peer Info
    // =========================================================================
    int getPeerCount() const;
    QString getOurPeerId() const;
    size_t getDhtNodeCount() const;
    bool isDhtRunning() const;
    
    // =========================================================================
    // Message Sending (Transport Layer)
    // =========================================================================
    
    /**
     * @brief Send a message to a specific peer
     */
    bool sendMessage(const QString& peerId, const QString& messageType, const QJsonObject& data);
    
    /**
     * @brief Broadcast a message to all connected peers
     */
    int broadcastMessage(const QString& messageType, const QJsonObject& data);
    
    /**
     * @brief Publish message to GossipSub topic
     */
    bool publishToTopic(const QString& topic, const QJsonObject& data);
    
    /**
     * @brief Send search request to all peers and publish to GossipSub
     * Convenience method for torrent search
     */
    void searchTorrents(const QString& query);
    
    /**
     * @brief Announce a torrent to the network
     */
    void announceTorrent(const QString& infoHash, const QString& name);
    
    // =========================================================================
    // Message Handler Registration
    // =========================================================================
    
    using MessageHandler = std::function<void(const QString& peerId, const QJsonObject& data)>;
    
    /**
     * @brief Register a handler for a specific message type
     * This allows RatsAPI to register handlers without P2PNetwork knowing the business logic
     */
    void registerMessageHandler(const QString& messageType, MessageHandler handler);
    
    /**
     * @brief Unregister handler for a message type
     */
    void unregisterMessageHandler(const QString& messageType);
    
    // =========================================================================
    // BitTorrent (optional feature)
    // =========================================================================
    bool isBitTorrentEnabled() const;
    bool enableBitTorrent();
    void disableBitTorrent();
    
    /**
     * @brief Get the RatsClient instance (for advanced usage)
     */
    librats::RatsClient* getRatsClient() const { return ratsClient_.get(); }

signals:
    // Lifecycle signals
    void started();
    void stopped();
    void statusChanged(const QString& status);
    void error(const QString& errorMessage);
    
    // Peer signals
    void peerCountChanged(int count);
    void peerConnected(const QString& peerId);
    void peerDisconnected(const QString& peerId);
    
    // Generic message signal (for messages without registered handlers)
    void messageReceived(const QString& peerId, const QString& messageType, const QJsonObject& data);

private slots:
    void updatePeerCount();

private:
    void setupLibratsCallbacks();
    void setupGossipSub();
    
    std::unique_ptr<librats::RatsClient> ratsClient_;
    int port_;
    int dhtPort_;
    QString dataDirectory_;
    bool running_;
    bool bitTorrentEnabled_;
    
    // Registered message handlers
    QHash<QString, MessageHandler> messageHandlers_;
    
    QTimer *updateTimer_;
};

#endif // P2PNETWORK_H
