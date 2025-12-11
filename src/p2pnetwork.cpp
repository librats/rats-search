#include "p2pnetwork.h"
#include "librats.h"
#include <QTimer>
#include <QDebug>

P2PNetwork::P2PNetwork(int port, int dhtPort, const QString& dataDirectory, QObject *parent)
    : QObject(parent)
    , port_(port)
    , dhtPort_(dhtPort)
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
        
        // Create librats client
        ratsClient_ = std::make_unique<librats::RatsClient>(port_);
        
        // Set protocol name for rats-search
        ratsClient_->set_protocol_name("rats-search");
        ratsClient_->set_protocol_version("2.0");
        
        // Set data directory
        std::string dataDir = dataDirectory_.toStdString();
        ratsClient_->set_data_directory(dataDir);
        
        // Load configuration
        ratsClient_->load_configuration();
        
        // Setup callbacks
        setupCallbacks();
        
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
        
        // Setup GossipSub topics
        setupGossipSub();
        
        // Try to reconnect to known peers
        int reconnectAttempts = ratsClient_->load_and_reconnect_peers();
        qInfo() << "Attempted to reconnect to" << reconnectAttempts << "previous peers";
        
        running_ = true;
        updateTimer_->start(1000);  // Update every second
        
        emit started();
        emit statusChanged("Connected");
        
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
    emit statusChanged("Disconnected");
    
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

void P2PNetwork::searchTorrents(const QString& query)
{
    if (!isRunning()) {
        qWarning() << "Cannot search: P2P network not running";
        return;
    }
    
    qInfo() << "Searching P2P network for:" << query;
    
    // Create search message
    nlohmann::json searchMsg;
    searchMsg["query"] = query.toStdString();
    searchMsg["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
    
    // Send search request to all peers
    ratsClient_->send("torrent_search", searchMsg);
    
    // Also publish to GossipSub topic for wider dissemination
    ratsClient_->publish_json_to_topic("rats-search", searchMsg);
}

void P2PNetwork::announceTorrent(const QString& infoHash, const QString& name)
{
    if (!isRunning()) {
        return;
    }
    
    qInfo() << "Announcing torrent:" << name;
    
    nlohmann::json announceMsg;
    announceMsg["info_hash"] = infoHash.toStdString();
    announceMsg["name"] = name.toStdString();
    announceMsg["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
    
    // Broadcast to all peers
    ratsClient_->send("torrent_announce", announceMsg);
    
    // Publish to GossipSub
    ratsClient_->publish_json_to_topic("rats-announcements", announceMsg);
}

void P2PNetwork::setupCallbacks()
{
    if (!ratsClient_) {
        return;
    }
    
    // Connection callback
    ratsClient_->set_connection_callback([this](socket_t socket, const std::string& peer_id) {
        Q_UNUSED(socket);
        qInfo() << "Peer connected:" << QString::fromStdString(peer_id).left(8);
        emit peerConnected(QString::fromStdString(peer_id));
        emit peerCountChanged(ratsClient_->get_peer_count());
    });
    
    // Disconnection callback
    ratsClient_->set_disconnect_callback([this](socket_t socket, const std::string& peer_id) {
        Q_UNUSED(socket);
        qInfo() << "Peer disconnected:" << QString::fromStdString(peer_id).left(8);
        emit peerDisconnected(QString::fromStdString(peer_id));
        emit peerCountChanged(ratsClient_->get_peer_count());
    });
    
    // Handle torrent search requests
    ratsClient_->on("torrent_search", [this](const std::string& peer_id, const nlohmann::json& data) {
        Q_UNUSED(peer_id);
        qDebug() << "Received torrent search request from peer";
        
        // TODO: Search local database and respond
        if (data.contains("query")) {
            QString query = QString::fromStdString(data["query"].get<std::string>());
            qDebug() << "Search query:" << query;
        }
    });
    
    // Handle torrent search responses
    ratsClient_->on("torrent_search_result", [this](const std::string& peer_id, const nlohmann::json& data) {
        Q_UNUSED(peer_id);
        
        if (data.contains("info_hash") && data.contains("name")) {
            QString infoHash = QString::fromStdString(data["info_hash"].get<std::string>());
            QString name = QString::fromStdString(data["name"].get<std::string>());
            qint64 size = data.value("size", 0);
            int seeders = data.value("seeders", 0);
            int leechers = data.value("leechers", 0);
            
            emit searchResultReceived(infoHash, name, size, seeders, leechers);
        }
    });
    
    // Handle torrent announcements
    ratsClient_->on("torrent_announce", [this](const std::string& peer_id, const nlohmann::json& data) {
        Q_UNUSED(peer_id);
        
        if (data.contains("info_hash") && data.contains("name")) {
            QString infoHash = QString::fromStdString(data["info_hash"].get<std::string>());
            QString name = QString::fromStdString(data["name"].get<std::string>());
            
            qDebug() << "Received torrent announcement:" << name;
            // TODO: Add to database
        }
    });
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
    
    // Handle messages on rats-search topic
    ratsClient_->on_topic_json_message("rats-search", 
        [this](const std::string& peer_id, const std::string& topic, const nlohmann::json& data) {
            Q_UNUSED(peer_id);
            Q_UNUSED(topic);
            
            if (data.contains("query")) {
                QString query = QString::fromStdString(data["query"].get<std::string>());
                qDebug() << "GossipSub search query:" << query;
            }
        });
    
    // Handle messages on rats-announcements topic
    ratsClient_->on_topic_json_message("rats-announcements",
        [this](const std::string& peer_id, const std::string& topic, const nlohmann::json& data) {
            Q_UNUSED(peer_id);
            Q_UNUSED(topic);
            
            if (data.contains("info_hash") && data.contains("name")) {
                QString infoHash = QString::fromStdString(data["info_hash"].get<std::string>());
                QString name = QString::fromStdString(data["name"].get<std::string>());
                qDebug() << "GossipSub torrent announcement:" << name;
            }
        });
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

