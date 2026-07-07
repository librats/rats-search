#include "p2pnetwork.h"

// librats' EventBus exposes a method named emit(), which collides with Qt's
// `emit` keyword macro. Neutralise the macro across all librats includes, then
// restore it so rats-search's own `emit signal` statements keep working.
#pragma push_macro("emit")
#pragma push_macro("slots")
#pragma push_macro("signals")
#undef emit
#undef slots
#undef signals
#include "node/node.h"
#include "peer/peer.h"
#include "peer/peer_id.h"
#include "core/address.h"
#include "core/bytes.h"
#include "util/json.h"
#include "subsystems/dht_discovery.h"
#include "subsystems/mdns_discovery.h"
#include "subsystems/pubsub.h"
#include "subsystems/message_json.h"
#include "subsystems/port_mapping_service.h"
#include "subsystems/reconnection.h"
#include "dht/dht.h"
#ifdef RATS_SEARCH_FEATURES
#include "subsystems/bittorrent.h"
#endif
#ifdef RATS_STORAGE
#include "storage/storage.h"
#endif
#pragma pop_macro("signals")
#pragma pop_macro("slots")
#pragma pop_macro("emit")

#include <QTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QMutexLocker>

// Helper: Convert librats::Json to QJsonObject (via compact text round-trip).
static QJsonObject libratsJsonToQt(const librats::Json& j)
{
    if (!j.is_object()) {
        return QJsonObject();
    }
    const std::string s = j.dump();
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(s));
    return doc.object();
}

// Helper: Convert QJsonObject to librats::Json (via compact text round-trip).
static librats::Json qtToLibratsJson(const QJsonObject& obj)
{
    const QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    librats::Json j = librats::Json::parse(std::string(bytes.constData(),
                                                       static_cast<size_t>(bytes.size())),
                                           nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        return librats::Json::object();
}
    return j;
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
        
        // --- Build the node configuration ---------------------------------
        librats::NodeConfig config;
        config.listen_port = static_cast<uint16_t>(port_);
        config.max_peers   = maxPeers_ > 0 ? static_cast<size_t>(maxPeers_) : 0;
        // Protocol identity is bound into the Noise handshake AND namespaces DHT
        // discovery. Keep it version-less so peers across patch releases meet.
        config.protocol    = "rats-search/2";
        config.data_dir    = dataDirectory_.toStdString();
        config.security    = librats::NodeConfig::Security::Noise;

        node_ = std::make_unique<librats::Node>(std::move(config));

        // Peer connect/disconnect callbacks MUST be registered before start().
        setupLibratsCallbacks();

        // --- Attach subsystems (all BEFORE node_->start()) ----------------
        
        // DHT discovery (shared by the BitTorrent subsystem and the spider).
        {
            librats::DhtDiscovery::Config dhtCfg;
            dhtCfg.dht_port = static_cast<uint16_t>(dhtPort_);
            dhtCfg.data_dir = dataDirectory_.toStdString();
            dht_ = node_->add_subsystem(std::make_unique<librats::DhtDiscovery>(std::move(dhtCfg)));
        }

        // Local-network discovery.
        mdns_ = node_->add_subsystem(std::make_unique<librats::MdnsDiscovery>());

        // Pub/sub (GossipSub) for search/announcement dissemination.
        pubsub_ = node_->add_subsystem(std::make_unique<librats::PubSub>());

        // Typed JSON messaging (the old RatsClient on()/send() surface).
        messages_ = node_->add_subsystem(std::make_unique<librats::MessageJson>());
        
        // Automatic NAT port forwarding (UPnP + NAT-PMP), gated by preference.
        if (portMappingEnabled_) {
            librats::PortMappingConfig pmCfg;
            pmCfg.enabled       = true;
            pmCfg.enable_upnp   = true;
            pmCfg.enable_natpmp = true;
            portMapping_ = node_->add_subsystem(std::make_unique<librats::PortMappingService>(pmCfg));
        }
        
        // Remember + re-dial known peers across restarts.
        {
            librats::ReconnectionService::Config rc;
            if (!dataDirectory_.isEmpty()) {
                rc.store_path = (dataDirectory_ + "/peers.json").toStdString();
        }
            reconnect_ = node_->add_subsystem(std::make_unique<librats::ReconnectionService>(rc));
        }
        
#ifdef RATS_STORAGE
        // Distributed key/value store (used by the voting system).
        {
            librats::StorageConfig sc;
            sc.data_directory = (dataDirectory_ + "/storage").toStdString();
            storage_ = node_->add_subsystem(std::make_unique<librats::StorageManager>(sc));
        }
#endif
        
#ifdef RATS_SEARCH_FEATURES
        // BitTorrent (downloads + DHT spider). Attached after DhtDiscovery so it
        // borrows the same Kademlia swarm. Always available.
        {
            librats::Bittorrent::Config btCfg;
            btCfg.client.download_path = dataDirectory_.toStdString();
            btCfg.client.listen_port   = static_cast<uint16_t>(dhtPort_);
            btCfg.use_node_dht         = true;
            bittorrent_ = node_->add_subsystem(std::make_unique<librats::Bittorrent>(std::move(btCfg)));
        }
#endif

        // --- Bring the node (and all subsystems) up -----------------------
        if (!node_->start()) {
            qWarning() << "Failed to start librats node";
            emit error("Failed to start P2P network");
            node_.reset();
            dht_ = nullptr; mdns_ = nullptr; pubsub_ = nullptr; messages_ = nullptr;
            portMapping_ = nullptr; reconnect_ = nullptr; storage_ = nullptr; bittorrent_ = nullptr;
            return false;
        }

        bitTorrentEnabled_ = (bittorrent_ != nullptr);

        // Post-start wiring: topic subscriptions + message dispatchers.
        setupGossipSub();
        setupClientInfoHandler();
        for (auto it = messageHandlers_.begin(); it != messageHandlers_.end(); ++it) {
            registerDispatcher(it.key());
        }
        
        if (dht_ && dht_->is_running()) {
            qInfo() << "DHT discovery started on port" << dhtPort_;
        } else {
            qWarning() << "DHT discovery not running";
        }
        
        running_ = true;
        updateTimer_->start(1000);  // Update every second
        
        emit started();
        emit statusChanged("Started");
        
        qInfo() << "P2P network started successfully";
        qInfo() << "Our peer ID:" << getOurPeerId();
        
        return true;
        
    } catch (const std::exception& e) {
        qCritical() << "Exception starting P2P network:" << e.what();
        emit error(QString("Failed to start P2P network: %1").arg(e.what()));
        return false;
    }
}

void P2PNetwork::setPortMappingEnabled(bool enabled)
{
    // Subsystems are attached before start(), so this only takes effect on the
    // next P2P (re)start. We just record the preference here.
    portMappingEnabled_ = enabled;
    }

void P2PNetwork::stop()
{
    if (!running_) {
        return;
    }
    
    qInfo() << "Stopping P2P network...";
    
    updateTimer_->stop();
    
    if (node_) {
        // ReconnectionService persists the peer book; identity persists via data_dir.
        node_->stop();
        node_.reset();
    }
        
    dht_ = nullptr; mdns_ = nullptr; pubsub_ = nullptr; messages_ = nullptr;
    portMapping_ = nullptr; reconnect_ = nullptr; storage_ = nullptr; bittorrent_ = nullptr;
    registeredDispatchers_.clear();
    bitTorrentEnabled_ = false;
        
    running_ = false;
    emit stopped();
    emit statusChanged("Stopped");
    
    qInfo() << "P2P network stopped";
}

bool P2PNetwork::isRunning() const
{
    return running_ && node_ != nullptr;
}

bool P2PNetwork::isConnected() const
{
    return isRunning() && getPeerCount() > 0;
}

int P2PNetwork::getPeerCount() const
{
    if (!node_) {
        return 0;
    }
    return static_cast<int>(node_->peer_count());
}

QString P2PNetwork::getOurPeerId() const
{
    if (!node_) {
        return QString();
    }
    return QString::fromStdString(node_->local_id().to_hex());
}

size_t P2PNetwork::getDhtNodeCount() const
{
    if (!dht_) {
        return 0;
    }
    librats::DhtClient* client = dht_->dht_client();
    return client ? client->get_routing_table_size() : 0;
}

bool P2PNetwork::isDhtRunning() const
{
    return dht_ && dht_->is_running();
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
    if (node_) {
        node_->set_max_peers(maxPeers > 0 ? static_cast<size_t>(maxPeers) : 0);
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
    if (!isRunning() || !messages_) {
        qWarning() << "Cannot send message: P2P network not running";
        return false;
    }
    
    auto id = librats::PeerId::from_hex(peerId.toStdString());
    if (!id) {
        qWarning() << "Cannot send message: invalid peer id" << peerId.left(8);
        return false;
    }

    librats::Json jsonData = qtToLibratsJson(data);
    messages_->send(*id, messageType.toStdString(), jsonData);
    return true;
}

int P2PNetwork::broadcastMessage(const QString& messageType, const QJsonObject& data)
{
    if (!isRunning() || !messages_) {
        qWarning() << "Cannot broadcast: P2P network not running";
        return 0;
    }
    
    librats::Json jsonData = qtToLibratsJson(data);
    messages_->send(messageType.toStdString(), jsonData);
    return getPeerCount();  // Approximate count
}

bool P2PNetwork::publishToTopic(const QString& topic, const QJsonObject& data)
{
    if (!isRunning() || !pubsub_) {
        return false;
    }
    
    librats::Json jsonData = qtToLibratsJson(data);
    const std::string payload = jsonData.dump();
    pubsub_->publish(topic.toStdString(),
                     librats::ByteView(reinterpret_cast<const uint8_t*>(payload.data()),
                                       payload.size()));
    return true;
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
        messageHandlers_[messageType] = handler;
    
    // If the node is already up, wire the dispatcher immediately; otherwise it
    // will be registered in start() once MessageJson is attached.
    if (messages_) {
        registerDispatcher(messageType);
            }
    
    qInfo() << "Registered P2P message handler for:" << messageType;
}

void P2PNetwork::unregisterMessageHandler(const QString& messageType)
{
    messageHandlers_.remove(messageType);
    
    if (messages_) {
        messages_->off(messageType.toStdString());
    }
    registeredDispatchers_.remove(messageType);
}

void P2PNetwork::registerDispatcher(const QString& messageType)
{
    if (!messages_ || registeredDispatchers_.contains(messageType)) {
        return;
    }
    registeredDispatchers_.insert(messageType);

    messages_->on(messageType.toStdString(),
        [this, messageType](const librats::PeerId& from, const librats::Json& data) {
            QString peerId = QString::fromStdString(from.to_hex());
            QJsonObject jsonData = libratsJsonToQt(data);
            dispatchMessage(peerId, messageType, jsonData);
        });
}

void P2PNetwork::dispatchMessage(const QString& peerId, const QString& messageType, const QJsonObject& data)
{
    auto it = messageHandlers_.find(messageType);
    if (it != messageHandlers_.end() && it.value()) {
        it.value()(peerId, data);
    } else {
        emit messageReceived(peerId, messageType, data);
    }
}

// =========================================================================
// Private Implementation
// =========================================================================

void P2PNetwork::setupLibratsCallbacks()
{
    if (!node_) {
        return;
    }
    
    // Connection callback - send our client info. Runs on a reactor thread.
    node_->on_peer_connected([this](const librats::Peer& peer) {
        QString peerId = QString::fromStdString(peer.id().to_hex());
        qInfo() << "Peer connected:" << peerId.left(8);
        
        emit peerConnected(peerId);
        emit peerCountChanged(getPeerCount());
        
        // Send our client info to the new peer
        sendClientInfo(peerId);
    });
    
    // Disconnection callback - remove peer info. Runs on a reactor thread.
    node_->on_peer_disconnected([this](const librats::PeerId& id) {
        QString peerId = QString::fromStdString(id.to_hex());
        qInfo() << "Peer disconnected:" << peerId.left(8);
        
        {
            QMutexLocker locker(&peerInfoMutex_);
            peerInfoMap_.remove(peerId);
        }
        
        emit peerDisconnected(peerId);
        emit peerCountChanged(getPeerCount());
    });
                }

void P2PNetwork::setupGossipSub()
{
    if (!pubsub_) {
        qWarning() << "GossipSub not available";
        return;
    }
    
    auto handler = [this](const librats::PeerId& from, const std::string& topic,
                          librats::ByteView data) {
        QString peerId = QString::fromStdString(from.to_hex());
        std::string s(reinterpret_cast<const char*>(data.data()), data.size());
        librats::Json j = librats::Json::parse(s, nullptr, /*allow_exceptions=*/false);
        QJsonObject jsonData = j.is_discarded() ? QJsonObject() : libratsJsonToQt(j);
        dispatchMessage(peerId, QString::fromStdString(topic), jsonData);
    };
    
    pubsub_->subscribe("rats-search", handler);
    pubsub_->subscribe("rats-announcements", handler);
    qInfo() << "Subscribed to rats-search and rats-announcements topics";
    }
    
void P2PNetwork::updatePeerCount()
{
    if (node_) {
        int count = getPeerCount();
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
    return bitTorrentEnabled_ && bittorrent_ != nullptr;
}

bool P2PNetwork::enableBitTorrent()
{
#ifdef RATS_SEARCH_FEATURES
    // BitTorrent is attached unconditionally in start(); nothing to toggle. If the
    // node is already up it is available, otherwise it will be once start() runs.
    return running_ ? (bittorrent_ != nullptr) : true;
#else
    qWarning() << "BitTorrent features not compiled in";
    return false;
#endif
}

void P2PNetwork::disableBitTorrent()
{
    // BitTorrent is a permanently-attached subsystem now; disabling at runtime is
    // not supported by the new core. No-op.
    }

void P2PNetwork::setResumeDataPath(const QString& path)
{
    // The new librats BitTorrent core stores each torrent's fast-resume record next
    // to its download (in {save_path}/.resume/), so there is no global resume-data
    // directory to configure. Kept for API compatibility; no-op.
    Q_UNUSED(path);
}

bool P2PNetwork::connectToPeer(const QString& address)
{
    if (!node_ || address.isEmpty()) {
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
    
    // Non-blocking dial; the connection surfaces via on_peer_connected.
    node_->connect(host.toStdString(), static_cast<uint16_t>(port));
    qInfo() << "Dialing bootstrap peer:" << address.left(30);
        return true;
    }
    
// =========================================================================
// Client Info Exchange
// =========================================================================

void P2PNetwork::setupClientInfoHandler()
{
    if (!messages_ || registeredDispatchers_.contains("client_info")) {
        return;
    }
    registeredDispatchers_.insert("client_info");
    
    messages_->on("client_info",
        [this](const librats::PeerId& from, const librats::Json& data) {
            QString peerId = QString::fromStdString(from.to_hex());
            QJsonObject jsonData = libratsJsonToQt(data);
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
