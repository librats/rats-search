#include "p2pnetwork.h"
#include "librats.h"
#include <QTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

// Helper: Convert nlohmann::json to QJsonObject
static QJsonObject nlohmannToQt(const nlohmann::json& j) {
    if (j.is_null() || !j.is_object()) {
        return QJsonObject();
    }
    QString jsonStr = QString::fromStdString(j.dump());
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    return doc.object();
}

// Helper: Convert QJsonObject to nlohmann::json
static nlohmann::json qtToNlohmann(const QJsonObject& obj) {
    QJsonDocument doc(obj);
    std::string jsonStr = doc.toJson(QJsonDocument::Compact).toStdString();
    return nlohmann::json::parse(jsonStr);
}

P2PNetwork::P2PNetwork(int port, int dhtPort, const QString& dataDirectory, int maxPeers, QObject *parent)
    : QObject(parent)
    , port_(port)
    , dhtPort_(dhtPort)
    , maxPeers_(maxPeers)
    , dataDirectory_(dataDirectory)
    , running_(false)
    , bitTorrentEnabled_(false)
{
    updateTimer_ = new QTimer(this);
    connect(updateTimer_, &QTimer::timeout, this, &P2PNetwork::updatePeerCount);
}

P2PNetwork::~P2PNetwork()
{
    stop();
}

bool P2PNetwork::start()
{
    if (running_) {
        return true;
    }
    
    try {
        qInfo() << "Starting P2P network on port" << port_;
        
        // Create librats client with configured max peers
        ratsClient_ = std::make_unique<librats::RatsClient>(port_, maxPeers_);

        // Set protocol name for rats-search
        ratsClient_->set_protocol_name("rats-search");
        ratsClient_->set_protocol_version("2.0.1");

        // Set data directory
        std::string dataDir = dataDirectory_.toStdString();
        ratsClient_->set_data_directory(dataDir);

        // Enable storage
        ratsClient_->get_storage_manager();
        
        // Load configuration
        ratsClient_->load_configuration();

        // Improtant to call initialize_encryption after loading configuration
        // Enable encryption for rats peers
        ratsClient_->initialize_encryption(true);
        
        // Setup librats callbacks
        setupLibratsCallbacks();
        
        // Start the client
        if (!ratsClient_->start()) {
            qWarning() << "Failed to start librats client";
            emit error("Failed to start P2P network");
            return false;
        }
        
        // Start DHT discovery on specified port
        if (ratsClient_->start_dht_discovery(dhtPort_)) {
            qInfo() << "DHT discovery started on port" << dhtPort_;
        } else {
            qWarning() << "Failed to start DHT discovery";
        }
        
        // Start mDNS discovery for local network
        if (ratsClient_->start_mdns_discovery("rats-search")) {
            qInfo() << "mDNS discovery started successfully";
        } else {
            qWarning() << "Failed to start mDNS discovery";
        }
        
        // Configure STUN for NAT traversal and public address discovery
        ratsClient_->add_stun_server("stun.l.google.com", 19302);
        ratsClient_->add_stun_server("stun1.l.google.com", 19302);
        
        auto publicAddr = ratsClient_->discover_public_address("stun.l.google.com", 19302, 5000);
        if (publicAddr && publicAddr->is_valid()) {
            qInfo() << "Public address discovered via STUN:" 
                    << QString::fromStdString(publicAddr->address) 
                    << "port:" << publicAddr->port;
        } else {
            qWarning() << "Could not discover public address via STUN";
        }
        
        // Setup GossipSub topics
        setupGossipSub();
        
        // Try to reconnect to known peers
        int reconnectAttempts = ratsClient_->load_and_reconnect_peers();
        qInfo() << "Attempted to reconnect to" << reconnectAttempts << "previous peers";
        
        running_ = true;
        updateTimer_->start(1000);  // Update every second
        
        emit started();
        emit statusChanged("Started");
        
        qInfo() << "P2P network started successfully";
        qInfo() << "Our peer ID:" << QString::fromStdString(ratsClient_->get_our_peer_id());
        
        return true;
        
    } catch (const std::exception& e) {
        qCritical() << "Exception starting P2P network:" << e.what();
        emit error(QString("Failed to start P2P network: %1").arg(e.what()));
        return false;
    }
}

void P2PNetwork::stop()
{
    if (!running_) {
        return;
    }
    
    qInfo() << "Stopping P2P network...";
    
    updateTimer_->stop();
    
    if (ratsClient_) {
        // Save configuration and peers
        ratsClient_->save_configuration();
        ratsClient_->save_historical_peers();
        
        // Stop DHT and mDNS
        ratsClient_->stop_dht_discovery();
        ratsClient_->stop_mdns_discovery();
        
        // Stop the client
        ratsClient_->stop();
        ratsClient_.reset();
    }
    
    running_ = false;
    emit stopped();
    emit statusChanged("Stopped");
    
    qInfo() << "P2P network stopped";
}

bool P2PNetwork::isRunning() const
{
    return running_ && ratsClient_ && ratsClient_->is_running();
}

bool P2PNetwork::isConnected() const
{
    return isRunning() && ratsClient_ && ratsClient_->get_peer_count() > 0;
}

int P2PNetwork::getPeerCount() const
{
    if (!ratsClient_) {
        return 0;
    }
    return ratsClient_->get_peer_count();
}

QString P2PNetwork::getOurPeerId() const
{
    if (!ratsClient_) {
        return QString();
    }
    return QString::fromStdString(ratsClient_->get_our_peer_id());
}

size_t P2PNetwork::getDhtNodeCount() const
{
    if (!ratsClient_) {
        return 0;
    }
    return ratsClient_->get_dht_routing_table_size();
}

bool P2PNetwork::isDhtRunning() const
{
    if (!ratsClient_) {
        return false;
    }
    return ratsClient_->is_dht_running();
}

QHash<QString, PeerInfo> P2PNetwork::getConnectedPeersInfo() const
{
    QMutexLocker locker(&peerInfoMutex_);
    return peerInfoMap_;
}

PeerInfo P2PNetwork::getPeerInfo(const QString& peerId) const
{
    QMutexLocker locker(&peerInfoMutex_);
    return peerInfoMap_.value(peerId);
}

qint64 P2PNetwork::getRemoteTorrentsCount() const
{
    QMutexLocker locker(&peerInfoMutex_);
    qint64 total = 0;
    for (auto it = peerInfoMap_.constBegin(); it != peerInfoMap_.constEnd(); ++it) {
        total += it.value().torrentsCount;
    }
    return total;
}

void P2PNetwork::setMaxPeers(int maxPeers)
{
    maxPeers_ = maxPeers;
    if (ratsClient_) {
        ratsClient_->set_max_peers(maxPeers);
        qInfo() << "P2P max peers updated to" << maxPeers;
    }
}

void P2PNetwork::setClientVersion(const QString& version)
{
    clientVersion_ = version;
}

void P2PNetwork::updateOurStats(qint64 torrents, qint64 files, qint64 totalSize)
{
    ourTorrentsCount_ = torrents;
    ourFilesCount_ = files;
    ourTotalSize_ = totalSize;
}


// =========================================================================
// Message Sending (Transport Layer)
// =========================================================================

bool P2PNetwork::sendMessage(const QString& peerId, const QString& messageType, const QJsonObject& data)
{
    if (!isRunning()) {
        qWarning() << "Cannot send message: P2P network not running";
        return false;
    }
    
    nlohmann::json jsonData = qtToNlohmann(data);
    ratsClient_->send(peerId.toStdString(), messageType.toStdString(), jsonData);
    return true;
}

int P2PNetwork::broadcastMessage(const QString& messageType, const QJsonObject& data)
{
    if (!isRunning()) {
        qWarning() << "Cannot broadcast: P2P network not running";
        return 0;
    }
    
    nlohmann::json jsonData = qtToNlohmann(data);
    ratsClient_->send(messageType.toStdString(), jsonData);
    return getPeerCount();  // Approximate count
}

bool P2PNetwork::publishToTopic(const QString& topic, const QJsonObject& data)
{
    if (!isRunning()) {
        return false;
    }
    
    nlohmann::json jsonData = qtToNlohmann(data);
    return ratsClient_->publish_json_to_topic(topic.toStdString(), jsonData);
}

void P2PNetwork::searchTorrents(const QString& query)
{
    if (!isRunning()) {
        qWarning() << "Cannot search: P2P network not running";
        return;
    }
    
    qInfo() << "Searching P2P network for:" << query;
    
    QJsonObject searchMsg;
    searchMsg["query"] = query;
    searchMsg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    
    // Send search request to all peers
    broadcastMessage("torrent_search", searchMsg);
    
    // Also publish to GossipSub topic for wider dissemination
    publishToTopic("rats-search", searchMsg);
}

void P2PNetwork::announceTorrent(const QString& infoHash, const QString& name)
{
    if (!isRunning()) {
        return;
    }
    
    qInfo() << "Announcing torrent:" << name;
    
    QJsonObject announceMsg;
    announceMsg["info_hash"] = infoHash;
    announceMsg["name"] = name;
    announceMsg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    
    // Broadcast to all peers
    broadcastMessage("torrent_announce", announceMsg);
    
    // Publish to GossipSub
    publishToTopic("rats-announcements", announceMsg);
}

// =========================================================================
// Message Handler Registration
// =========================================================================

void P2PNetwork::registerMessageHandler(const QString& messageType, MessageHandler handler)
{
    if (!ratsClient_) {
        // Store handler for later registration when client starts
        messageHandlers_[messageType] = handler;
        return;
    }
    
    messageHandlers_[messageType] = handler;
    
    // Register with librats
    ratsClient_->on(messageType.toStdString(), 
        [this, messageType](const std::string& peer_id, const nlohmann::json& data) {
            QString peerId = QString::fromStdString(peer_id);
            QJsonObject jsonData = nlohmannToQt(data);
            
            // Call registered handler
            auto it = messageHandlers_.find(messageType);
            if (it != messageHandlers_.end() && it.value()) {
                it.value()(peerId, jsonData);
            } else {
                // Emit signal for unhandled messages
                emit messageReceived(peerId, messageType, jsonData);
            }
        });
    
    qInfo() << "Registered P2P message handler for:" << messageType;
}

void P2PNetwork::unregisterMessageHandler(const QString& messageType)
{
    messageHandlers_.remove(messageType);
    
    if (ratsClient_) {
        ratsClient_->off(messageType.toStdString());
    }
}

// =========================================================================
// Private Implementation
// =========================================================================

void P2PNetwork::setupLibratsCallbacks()
{
    if (!ratsClient_) {
        return;
    }
    
    // Connection callback - send our client info
    ratsClient_->set_connection_callback([this](socket_t socket, const std::string& peer_id) {
        Q_UNUSED(socket);
        QString peerId = QString::fromStdString(peer_id);
        qInfo() << "Peer connected:" << peerId.left(8);
        
        emit peerConnected(peerId);
        emit peerCountChanged(ratsClient_->get_peer_count());
        
        // Send our client info to the new peer
        sendClientInfo(peerId);
    });
    
    // Disconnection callback - remove peer info
    ratsClient_->set_disconnect_callback([this](socket_t socket, const std::string& peer_id) {
        Q_UNUSED(socket);
        QString peerId = QString::fromStdString(peer_id);
        qInfo() << "Peer disconnected:" << peerId.left(8);
        
        {
            QMutexLocker locker(&peerInfoMutex_);
            peerInfoMap_.remove(peerId);
        }
        
        emit peerDisconnected(peerId);
        emit peerCountChanged(ratsClient_->get_peer_count());
    });
    
    // Setup client info handler
    setupClientInfoHandler();
    
    // Re-register any handlers that were added before start()
    for (auto it = messageHandlers_.begin(); it != messageHandlers_.end(); ++it) {
        const QString& messageType = it.key();
        ratsClient_->on(messageType.toStdString(),
            [this, messageType](const std::string& peer_id, const nlohmann::json& data) {
                QString peerId = QString::fromStdString(peer_id);
                QJsonObject jsonData = nlohmannToQt(data);
                
                auto handler = messageHandlers_.find(messageType);
                if (handler != messageHandlers_.end() && handler.value()) {
                    handler.value()(peerId, jsonData);
                } else {
                    emit messageReceived(peerId, messageType, jsonData);
                }
            });
    }
}

void P2PNetwork::setupGossipSub()
{
    if (!ratsClient_ || !ratsClient_->is_gossipsub_available()) {
        qWarning() << "GossipSub not available";
        return;
    }
    
    // Subscribe to rats-search topic
    if (ratsClient_->subscribe_to_topic("rats-search")) {
        qInfo() << "Subscribed to rats-search topic";
    }
    
    // Subscribe to rats-announcements topic
    if (ratsClient_->subscribe_to_topic("rats-announcements")) {
        qInfo() << "Subscribed to rats-announcements topic";
    }
    
    // GossipSub messages are forwarded via messageReceived signal
    // RatsAPI can register handlers for specific topics
}

void P2PNetwork::updatePeerCount()
{
    if (ratsClient_) {
        int count = ratsClient_->get_peer_count();
        static int lastCount = -1;
        if (count != lastCount) {
            emit peerCountChanged(count);
            lastCount = count;
        }
    }
}

// =========================================================================
// BitTorrent (optional)
// =========================================================================

bool P2PNetwork::isBitTorrentEnabled() const
{
    return bitTorrentEnabled_;
}

bool P2PNetwork::enableBitTorrent()
{
#ifdef RATS_SEARCH_FEATURES
    if (!ratsClient_) {
        qWarning() << "Cannot enable BitTorrent: RatsClient not started";
        return false;
    }
    
    if (bitTorrentEnabled_) {
        return true;
    }
    
    if (ratsClient_->enable_bittorrent(dhtPort_)) {
        bitTorrentEnabled_ = true;
        qInfo() << "BitTorrent enabled on port" << dhtPort_;
        return true;
    } else {
        qWarning() << "Failed to enable BitTorrent";
        return false;
    }
#else
    qWarning() << "BitTorrent features not compiled in";
    return false;
#endif
}

void P2PNetwork::disableBitTorrent()
{
#ifdef RATS_SEARCH_FEATURES
    if (ratsClient_ && bitTorrentEnabled_) {
        ratsClient_->disable_bittorrent();
        bitTorrentEnabled_ = false;
        qInfo() << "BitTorrent disabled";
    }
#endif
}

void P2PNetwork::setResumeDataPath(const QString& path)
{
#ifdef RATS_SEARCH_FEATURES
    if (ratsClient_ && bitTorrentEnabled_) {
        ratsClient_->set_resume_data_path(path.toStdString());
        qInfo() << "Resume data path set to:" << path;
    }
#else
    Q_UNUSED(path);
#endif
}

bool P2PNetwork::connectToPeer(const QString& address)
{
    if (!ratsClient_ || address.isEmpty()) {
        return false;
    }
    
    // Parse address - could be various formats:
    // - IP:port (e.g., "1.2.3.4:9000")
    // - hostname:port
    
    QString host;
    int port = 9000;
    
    // Parse host:port format
    QStringList parts = address.split(':');
    if (parts.size() >= 2) {
        host = parts[0];
        bool ok;
        int parsedPort = parts[1].toInt(&ok);
        if (ok && parsedPort > 0 && parsedPort < 65536) {
            port = parsedPort;
        }
    } else {
        host = address;
    }
    
    if (host.isEmpty()) {
        return false;
    }
    
    // Use librats connect_to_peer method
    std::string hostStr = host.toStdString();
    if (ratsClient_->connect_to_peer(hostStr, port)) {
        qInfo() << "Connected to bootstrap peer:" << address.left(30);
        return true;
    }
    
    return false;
}

// =========================================================================
// Client Info Exchange
// =========================================================================

void P2PNetwork::setupClientInfoHandler()
{
    if (!ratsClient_) {
        return;
    }
    
    ratsClient_->on("client_info",
        [this](const std::string& peer_id, const nlohmann::json& data) {
            QString peerId = QString::fromStdString(peer_id);
            QJsonObject jsonData = nlohmannToQt(data);
            handleClientInfo(peerId, jsonData);
        });
}

void P2PNetwork::sendClientInfo(const QString& peerId)
{
    if (!isRunning()) {
        return;
    }
    
    sendMessage(peerId, "client_info", buildOurInfo());
}

void P2PNetwork::handleClientInfo(const QString& peerId, const QJsonObject& data)
{
    PeerInfo info;
    info.clientVersion = data["clientVersion"].toString();
    info.torrentsCount = data["torrentsCount"].toVariant().toLongLong();
    info.filesCount = data["filesCount"].toVariant().toLongLong();
    info.totalSize = data["totalSize"].toVariant().toLongLong();
    info.peersConnected = data["peersConnected"].toInt();
    info.connectedAt = QDateTime::currentMSecsSinceEpoch();
    
    {
        QMutexLocker locker(&peerInfoMutex_);
        peerInfoMap_[peerId] = info;
    }
    
    qInfo() << "Peer" << peerId.left(8) << "info: v" << info.clientVersion
            << "torrents:" << info.torrentsCount << "files:" << info.filesCount
            << "totalSize:" << info.totalSize << "peersConnected:" << info.peersConnected;
    
    emit peerInfoReceived(peerId, info);
}

QJsonObject P2PNetwork::buildOurInfo() const
{
    QJsonObject info;
    info["clientVersion"] = clientVersion_.isEmpty() ? "2.0.0" : clientVersion_;
    info["torrentsCount"] = ourTorrentsCount_;
    info["filesCount"] = ourFilesCount_;
    info["totalSize"] = ourTotalSize_;
    info["peersConnected"] = getPeerCount();
    
    return info;
}
