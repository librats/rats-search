#ifndef P2PNETWORK_H
#define P2PNETWORK_H

#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QSet>
#include <QMutex>
#include <memory>
#include <functional>

namespace librats {
    class Node;
    class DhtDiscovery;
    class MdnsDiscovery;
    class PubSub;
    class MessageJson;
    class PortMappingService;
    class ReconnectionService;
    class StorageManager;
    class Bittorrent;
}

/**
 * @brief Information about a connected peer
 * 
 * Holds client statistics exchanged between rats-search clients.
 */
struct PeerInfo {
    QString clientVersion;       ///< Client software version
    qint64 torrentsCount = 0;    ///< Number of torrents in peer's database
    qint64 filesCount = 0;       ///< Number of files in peer's database
    qint64 totalSize = 0;        ///< Total size of torrents in bytes
    int peersConnected = 0;      ///< Number of peers connected to this peer
    qint64 connectedAt = 0;      ///< Timestamp when connected (ms since epoch)
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["clientVersion"] = clientVersion;
        obj["torrentsCount"] = torrentsCount;
        obj["filesCount"] = filesCount;
        obj["totalSize"] = totalSize;
        obj["peersConnected"] = peersConnected;
        obj["connectedAt"] = connectedAt;
        return obj;
    }
    
    static PeerInfo fromJson(const QJsonObject& obj) {
        PeerInfo info;
        info.clientVersion = obj["clientVersion"].toString();
        info.torrentsCount = obj["torrentsCount"].toVariant().toLongLong();
        info.filesCount = obj["filesCount"].toVariant().toLongLong();
        info.totalSize = obj["totalSize"].toVariant().toLongLong();
        info.peersConnected = obj["peersConnected"].toInt();
        info.connectedAt = obj["connectedAt"].toVariant().toLongLong();
        return info;
    }
};

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
    explicit P2PNetwork(int port, int dhtPort, const QString& dataDirectory, int maxPeers = 10, QObject *parent = nullptr);
    ~P2PNetwork();

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool start();
    void stop();
    bool isRunning() const;
    bool isConnected() const;

    /**
     * @brief Enable/disable automatic NAT port forwarding (UPnP + NAT-PMP).
     *
     * Applied to the librats client on start(); if the network is already
     * running the change takes effect immediately.
     */
    void setPortMappingEnabled(bool enabled);
    bool portMappingEnabled() const { return portMappingEnabled_; }
    
    // =========================================================================
    // Peer Info
    // =========================================================================
    int getPeerCount() const;
    QString getOurPeerId() const;
    size_t getDhtNodeCount() const;
    bool isDhtRunning() const;
    
    /**
     * @brief Get information about all connected peers with their stats
     * @return Map of peerId -> PeerInfo
     */
    QHash<QString, PeerInfo> getConnectedPeersInfo() const;
    
    /**
     * @brief Get information about a specific peer
     * @param peerId The peer ID to look up
     * @return PeerInfo if found, empty PeerInfo otherwise
     */
    PeerInfo getPeerInfo(const QString& peerId) const;
    
    /**
     * @brief Get total torrents count from all connected peers
     * @return Sum of torrentsCount from all peers' handshake info
     */
    qint64 getRemoteTorrentsCount() const;
    
    // =========================================================================
    // Our Client Info (to send to peers)
    // =========================================================================
    
    /**
     * @brief Set maximum number of P2P connections
     * Updates librats at runtime if the client is running
     */
    void setMaxPeers(int maxPeers);
    
    /**
     * @brief Set our client version to advertise to peers
     */
    void setClientVersion(const QString& version);
    
    /**
     * @brief Update our statistics to share with peers
     * Call this when database stats change significantly
     */
    void updateOurStats(qint64 torrents, qint64 files, qint64 totalSize);
    
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
     * @brief Set the directory for storing resume data files
     * @param path Directory path for resume data (should be app data directory)
     */
    void setResumeDataPath(const QString& path);
    
    /**
     * @brief Get the underlying librats Node (for advanced usage)
     */
    librats::Node* node() const { return node_.get(); }

    /**
     * @brief Get the BitTorrent subsystem (null until start(), or if features off)
     */
    librats::Bittorrent* bittorrent() const { return bittorrent_; }

    /**
     * @brief Get the distributed storage subsystem (null until start(), or if storage off)
     */
    librats::StorageManager* storage() const { return storage_; }
    
    // =========================================================================
    // Bootstrap Peers
    // =========================================================================

    /**
     * @brief Connect to a specific peer address
     * @param address Multiaddr-style address
     */
    bool connectToPeer(const QString& address);

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
    
    /**
     * @brief Emitted when a peer's info is received
     * @param peerId The peer ID
     * @param info The peer's information
     */
    void peerInfoReceived(const QString& peerId, const PeerInfo& info);
    
    // Generic message signal (for messages without registered handlers)
    void messageReceived(const QString& peerId, const QString& messageType, const QJsonObject& data);

private slots:
    void updatePeerCount();

private:
    void setupLibratsCallbacks();
    void setupGossipSub();
    void setupClientInfoHandler();
    void sendClientInfo(const QString& peerId);
    void handleClientInfo(const QString& peerId, const QJsonObject& data);
    QJsonObject buildOurInfo() const;

    /// Register a MessageJson dispatcher for a message type (idempotent).
    void registerDispatcher(const QString& messageType);
    /// Dispatch an inbound message/topic payload to handlers or the messageReceived signal.
    void dispatchMessage(const QString& peerId, const QString& messageType, const QJsonObject& data);

    // The librats Node owns these subsystems; the raw pointers are non-owning and
    // valid for the Node's lifetime (attached before start(), torn down on stop()).
    std::unique_ptr<librats::Node> node_;
    librats::DhtDiscovery*       dht_ = nullptr;
    librats::MdnsDiscovery*      mdns_ = nullptr;
    librats::PubSub*             pubsub_ = nullptr;
    librats::MessageJson*        messages_ = nullptr;
    librats::PortMappingService* portMapping_ = nullptr;
    librats::ReconnectionService* reconnect_ = nullptr;
    librats::StorageManager*     storage_ = nullptr;
    librats::Bittorrent*         bittorrent_ = nullptr;

    int port_;
    int dhtPort_;
    int maxPeers_;
    QString dataDirectory_;
    bool running_;
    bool bitTorrentEnabled_;
    bool portMappingEnabled_ = true;
    
    // Registered message handlers
    QHash<QString, MessageHandler> messageHandlers_;
    // Message types for which a MessageJson dispatcher has already been registered.
    QSet<QString> registeredDispatchers_;
    
    // Connected peer information
    mutable QMutex peerInfoMutex_;
    QHash<QString, PeerInfo> peerInfoMap_;
    
    // Our client info to share with peers
    QString clientVersion_;
    qint64 ourTorrentsCount_ = 0;
    qint64 ourFilesCount_ = 0;
    qint64 ourTotalSize_ = 0;
    
    QTimer *updateTimer_;
};

#endif // P2PNETWORK_H
