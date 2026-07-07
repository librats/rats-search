#include "net/p2p_transport.h"

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

#include <QDebug>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>
#include <QTimer>

namespace rats::net {

namespace {

// Helper: Convert librats::Json to QJsonObject (via compact text round-trip).
QJsonObject libratsJsonToQt(const librats::Json& j)
{
    if (!j.is_object()) {
        return QJsonObject();
    }
    const std::string s = j.dump();
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(s));
    return doc.object();
}

// Helper: Convert QJsonObject to librats::Json (via compact text round-trip).
librats::Json qtToLibratsJson(const QJsonObject& obj)
{
    const QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    librats::Json j = librats::Json::parse(std::string(bytes.constData(), static_cast<size_t>(bytes.size())), nullptr,
        /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        return librats::Json::object();
    }
    return j;
}

} // namespace

// All transport state lives here; the librats raw pointers are non-owning and
// valid for the Node's lifetime (attached before start(), torn down on stop()).
struct P2PTransport::Private {
    P2PTransport* q = nullptr;

    std::unique_ptr<librats::Node> node;
    librats::DhtDiscovery* dht = nullptr;
    librats::MdnsDiscovery* mdns = nullptr;
    librats::PubSub* pubsub = nullptr;
    librats::MessageJson* messages = nullptr;
    librats::PortMappingService* portMapping = nullptr;
    librats::ReconnectionService* reconnect = nullptr;
    librats::StorageManager* storage = nullptr;
    librats::Bittorrent* bittorrent = nullptr;

    int port = 0;
    int dhtPort = 0;
    int maxPeers = 10;
    QString dataDirectory;
    bool running = false;
    bool bitTorrentEnabled = false;

    // Registered message handlers, and the message types for which a MessageJson
    // dispatcher has already been wired (idempotency guard).
    QHash<QString, MessageHandler> messageHandlers;
    QSet<QString> registeredDispatchers;

    // Identity of currently connected peers, kept in sync by the librats
    // connect/disconnect callbacks (which run on reactor threads).
    mutable QMutex peersMutex;
    QSet<QString> connectedPeerIds;

    QTimer* updateTimer = nullptr;
    // Last value broadcast by the poll timer. Instance member (not a function
    // static) so multiple P2PTransport instances don't share change detection.
    int lastPeerCount = -1;

    int peerCount() const { return node ? static_cast<int>(node->peer_count()) : 0; }

    void setupCallbacks();
    void registerDispatcher(const QString& type);
    void dispatchMessage(const QString& peerId, const QString& type, const QJsonObject& data);
    void updatePeerCount();
};

void P2PTransport::Private::setupCallbacks()
{
    if (!node) {
        return;
    }

    // Connection callback. Runs on a reactor thread.
    node->on_peer_connected([this](const librats::Peer& peer) {
        QString peerId = QString::fromStdString(peer.id().to_hex());
        qInfo() << "Peer connected:" << peerId.left(8);
        {
            QMutexLocker locker(&peersMutex);
            connectedPeerIds.insert(peerId);
        }
        emit q->peerConnected(peerId);
        emit q->peerCountChanged(peerCount());
    });

    // Disconnection callback. Runs on a reactor thread.
    node->on_peer_disconnected([this](const librats::PeerId& id) {
        QString peerId = QString::fromStdString(id.to_hex());
        qInfo() << "Peer disconnected:" << peerId.left(8);
        {
            QMutexLocker locker(&peersMutex);
            connectedPeerIds.remove(peerId);
        }
        emit q->peerDisconnected(peerId);
        emit q->peerCountChanged(peerCount());
    });
}

void P2PTransport::Private::registerDispatcher(const QString& type)
{
    if (!messages || registeredDispatchers.contains(type)) {
        return;
    }
    registeredDispatchers.insert(type);

    messages->on(type.toStdString(), [this, type](const librats::PeerId& from, const librats::Json& data) {
        QString peerId = QString::fromStdString(from.to_hex());
        QJsonObject jsonData = libratsJsonToQt(data);
        dispatchMessage(peerId, type, jsonData);
    });
}

void P2PTransport::Private::dispatchMessage(const QString& peerId, const QString& type, const QJsonObject& data)
{
    // Inbound messages arrive on a librats reactor thread. Marshal delivery onto
    // the thread that owns this transport (the main thread), so every handler —
    // and the services it drives (indexing, feed, voting) — runs single-threaded,
    // exactly like the crawler's queued discovered() path. Without this, P2P
    // writes would race main-thread reads/writes of the services' unguarded state.
    // (Posting to `q` also makes Qt drop the event automatically if the transport
    // is torn down before it is delivered.)
    QMetaObject::invokeMethod(
        q,
        [this, peerId, type, data]() {
            auto it = messageHandlers.find(type);
            if (it != messageHandlers.end() && it.value()) {
                it.value()(peerId, data);
            } else {
                emit q->messageReceived(peerId, type, data);
            }
        },
        Qt::QueuedConnection);
}

void P2PTransport::Private::updatePeerCount()
{
    if (!node) {
        return;
    }
    int count = peerCount();
    if (count != lastPeerCount) {
        emit q->peerCountChanged(count);
        lastPeerCount = count;
    }
}

// =========================================================================
// Construction / lifecycle
// =========================================================================

P2PTransport::P2PTransport(int port, int dhtPort, QString dataDirectory, int maxPeers, QObject* parent)
    : QObject(parent), d_(std::make_unique<Private>())
{
    d_->q = this;
    d_->port = port;
    d_->dhtPort = dhtPort;
    d_->dataDirectory = std::move(dataDirectory);
    d_->maxPeers = maxPeers;

    d_->updateTimer = new QTimer(this);
    connect(d_->updateTimer, &QTimer::timeout, this, [this]() { d_->updatePeerCount(); });
}

P2PTransport::~P2PTransport()
{
    stop();
}

bool P2PTransport::start()
{
    if (d_->running) {
        return true;
    }

    try {
        qInfo() << "Starting P2P transport on port" << d_->port;

        // --- Build the node configuration ---------------------------------
        librats::NodeConfig config;
        config.listen_port = static_cast<uint16_t>(d_->port);
        config.max_peers = d_->maxPeers > 0 ? static_cast<size_t>(d_->maxPeers) : 0;
        // Protocol identity is bound into the Noise handshake AND namespaces DHT
        // discovery. Keep it version-less so peers across patch releases meet.
        config.protocol = "rats-search/2";
        config.data_dir = d_->dataDirectory.toStdString();
        config.security = librats::NodeConfig::Security::Noise;

        d_->node = std::make_unique<librats::Node>(std::move(config));

        // Peer connect/disconnect callbacks MUST be registered before start().
        d_->setupCallbacks();

        // --- Attach subsystems (all BEFORE node->start()) -----------------

        // DHT discovery (shared by the BitTorrent subsystem and the spider).
        {
            librats::DhtDiscovery::Config dhtCfg;
            dhtCfg.dht_port = static_cast<uint16_t>(d_->dhtPort);
            dhtCfg.data_dir = d_->dataDirectory.toStdString();
            d_->dht = d_->node->add_subsystem(std::make_unique<librats::DhtDiscovery>(std::move(dhtCfg)));
        }

        // Local-network discovery.
        d_->mdns = d_->node->add_subsystem(std::make_unique<librats::MdnsDiscovery>());

        // Pub/sub (GossipSub) for topic dissemination.
        d_->pubsub = d_->node->add_subsystem(std::make_unique<librats::PubSub>());

        // Typed JSON messaging (the old RatsClient on()/send() surface).
        d_->messages = d_->node->add_subsystem(std::make_unique<librats::MessageJson>());

        // Automatic NAT port forwarding (UPnP + NAT-PMP), gated by preference.
        if (portMappingEnabled_) {
            librats::PortMappingConfig pmCfg;
            pmCfg.enabled = true;
            pmCfg.enable_upnp = true;
            pmCfg.enable_natpmp = true;
            d_->portMapping = d_->node->add_subsystem(std::make_unique<librats::PortMappingService>(pmCfg));
        }

        // Remember + re-dial known peers across restarts.
        {
            librats::ReconnectionService::Config rc;
            if (!d_->dataDirectory.isEmpty()) {
                rc.store_path = (d_->dataDirectory + "/peers.json").toStdString();
            }
            d_->reconnect = d_->node->add_subsystem(std::make_unique<librats::ReconnectionService>(rc));
        }

#ifdef RATS_STORAGE
        // Distributed key/value store (used by the voting system).
        {
            librats::StorageConfig sc;
            sc.data_directory = (d_->dataDirectory + "/storage").toStdString();
            d_->storage = d_->node->add_subsystem(std::make_unique<librats::StorageManager>(sc));
        }
#endif

#ifdef RATS_SEARCH_FEATURES
        // BitTorrent (downloads + DHT spider). Attached after DhtDiscovery so it
        // borrows the same Kademlia swarm. Always available.
        {
            librats::Bittorrent::Config btCfg;
            btCfg.client.download_path = d_->dataDirectory.toStdString();
            btCfg.client.listen_port = static_cast<uint16_t>(d_->dhtPort);
            btCfg.use_node_dht = true;
            d_->bittorrent = d_->node->add_subsystem(std::make_unique<librats::Bittorrent>(std::move(btCfg)));
        }
#endif

        // --- Bring the node (and all subsystems) up -----------------------
        if (!d_->node->start()) {
            qWarning() << "Failed to start librats node";
            emit error("Failed to start P2P transport");
            d_->node.reset();
            d_->dht = nullptr;
            d_->mdns = nullptr;
            d_->pubsub = nullptr;
            d_->messages = nullptr;
            d_->portMapping = nullptr;
            d_->reconnect = nullptr;
            d_->storage = nullptr;
            d_->bittorrent = nullptr;
            return false;
        }

        d_->bitTorrentEnabled = (d_->bittorrent != nullptr);

        // Post-start wiring: (re)register dispatchers for handlers that callers
        // registered before the node was up.
        for (auto it = d_->messageHandlers.begin(); it != d_->messageHandlers.end(); ++it) {
            d_->registerDispatcher(it.key());
        }

        if (d_->dht && d_->dht->is_running()) {
            qInfo() << "DHT discovery started on port" << d_->dhtPort;
        } else {
            qWarning() << "DHT discovery not running";
        }

        d_->running = true;
        d_->lastPeerCount = -1;
        d_->updateTimer->start(1000); // Update every second

        emit started();

        qInfo() << "P2P transport started successfully";
        qInfo() << "Our peer ID:" << ourPeerId();

        return true;

    } catch (const std::exception& e) {
        qCritical() << "Exception starting P2P transport:" << e.what();
        emit error(QString("Failed to start P2P transport: %1").arg(e.what()));
        return false;
    }
}

void P2PTransport::stop()
{
    if (!d_->running) {
        return;
    }

    qInfo() << "Stopping P2P transport...";

    d_->updateTimer->stop();

    if (d_->node) {
        // ReconnectionService persists the peer book; identity persists via data_dir.
        d_->node->stop();
        d_->node.reset();
    }

    d_->dht = nullptr;
    d_->mdns = nullptr;
    d_->pubsub = nullptr;
    d_->messages = nullptr;
    d_->portMapping = nullptr;
    d_->reconnect = nullptr;
    d_->storage = nullptr;
    d_->bittorrent = nullptr;
    d_->registeredDispatchers.clear();
    d_->bitTorrentEnabled = false;
    {
        QMutexLocker locker(&d_->peersMutex);
        d_->connectedPeerIds.clear();
    }

    d_->running = false;
    emit stopped();

    qInfo() << "P2P transport stopped";
}

bool P2PTransport::isRunning() const
{
    return d_->running && d_->node != nullptr;
}

bool P2PTransport::isConnected() const
{
    return isRunning() && peerCount() > 0;
}

void P2PTransport::setPortMappingEnabled(bool enabled)
{
    // Subsystems are attached before start(), so this only takes effect on the
    // next P2P (re)start. We just record the preference here.
    portMappingEnabled_ = enabled;
}

void P2PTransport::setMaxPeers(int maxPeers)
{
    d_->maxPeers = maxPeers;
    if (d_->node) {
        d_->node->set_max_peers(maxPeers > 0 ? static_cast<size_t>(maxPeers) : 0);
        qInfo() << "P2P max peers updated to" << maxPeers;
    }
}

// =========================================================================
// Peers
// =========================================================================

int P2PTransport::peerCount() const
{
    return d_->peerCount();
}

QString P2PTransport::ourPeerId() const
{
    if (!d_->node) {
        return QString();
    }
    return QString::fromStdString(d_->node->local_id().to_hex());
}

size_t P2PTransport::dhtNodeCount() const
{
    if (!d_->dht) {
        return 0;
    }
    librats::DhtClient* client = d_->dht->dht_client();
    return client ? client->get_routing_table_size() : 0;
}

bool P2PTransport::isDhtRunning() const
{
    return d_->dht && d_->dht->is_running();
}

QStringList P2PTransport::connectedPeerIds() const
{
    QMutexLocker locker(&d_->peersMutex);
    return QStringList(d_->connectedPeerIds.values());
}

bool P2PTransport::connectToPeer(const QString& address)
{
    if (!d_->node || address.isEmpty()) {
        return false;
    }

    // Parse address - host:port (IP or hostname); default port if omitted.
    QString host;
    int port = 9000;

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
    d_->node->connect(host.toStdString(), static_cast<uint16_t>(port));
    qInfo() << "Dialing bootstrap peer:" << address.left(30);
    return true;
}

// =========================================================================
// Messaging
// =========================================================================

bool P2PTransport::sendMessage(const QString& peerId, const QString& type, const QJsonObject& data)
{
    if (!isRunning() || !d_->messages) {
        qWarning() << "Cannot send message: P2P transport not running";
        return false;
    }

    auto id = librats::PeerId::from_hex(peerId.toStdString());
    if (!id) {
        qWarning() << "Cannot send message: invalid peer id" << peerId.left(8);
        return false;
    }

    librats::Json jsonData = qtToLibratsJson(data);
    d_->messages->send(*id, type.toStdString(), jsonData);
    return true;
}

int P2PTransport::broadcastMessage(const QString& type, const QJsonObject& data)
{
    if (!isRunning() || !d_->messages) {
        qWarning() << "Cannot broadcast: P2P transport not running";
        return 0;
    }

    librats::Json jsonData = qtToLibratsJson(data);
    d_->messages->send(type.toStdString(), jsonData);
    return peerCount(); // Approximate count
}

bool P2PTransport::publishToTopic(const QString& topic, const QJsonObject& data)
{
    if (!isRunning() || !d_->pubsub) {
        return false;
    }

    librats::Json jsonData = qtToLibratsJson(data);
    const std::string payload = jsonData.dump();
    d_->pubsub->publish(
        topic.toStdString(), librats::ByteView(reinterpret_cast<const uint8_t*>(payload.data()), payload.size()));
    return true;
}

bool P2PTransport::subscribeToTopic(const QString& topic)
{
    if (!isRunning() || !d_->pubsub) {
        return false;
    }

    // Same dispatching handler the old setupGossipSub used, but for a
    // caller-supplied topic instead of the hardcoded app topics.
    d_->pubsub->subscribe(
        topic.toStdString(), [this](const librats::PeerId& from, const std::string& t, librats::ByteView data) {
            QString peerId = QString::fromStdString(from.to_hex());
            std::string s(reinterpret_cast<const char*>(data.data()), data.size());
            librats::Json j = librats::Json::parse(s, nullptr, /*allow_exceptions=*/false);
            QJsonObject jsonData = j.is_discarded() ? QJsonObject() : libratsJsonToQt(j);
            d_->dispatchMessage(peerId, QString::fromStdString(t), jsonData);
        });
    qInfo() << "Subscribed to topic:" << topic;
    return true;
}

// =========================================================================
// Handler registration
// =========================================================================

void P2PTransport::registerHandler(const QString& type, MessageHandler handler)
{
    d_->messageHandlers[type] = std::move(handler);

    // If the node is already up, wire the dispatcher immediately; otherwise it
    // will be registered in start() once MessageJson is attached.
    if (d_->messages) {
        d_->registerDispatcher(type);
    }

    qInfo() << "Registered P2P message handler for:" << type;
}

void P2PTransport::unregisterHandler(const QString& type)
{
    d_->messageHandlers.remove(type);

    if (d_->messages) {
        d_->messages->off(type.toStdString());
    }
    d_->registeredDispatchers.remove(type);
}

// =========================================================================
// BitTorrent subsystem
// =========================================================================

bool P2PTransport::isBitTorrentEnabled() const
{
    return d_->bitTorrentEnabled && d_->bittorrent != nullptr;
}

bool P2PTransport::enableBitTorrent()
{
#ifdef RATS_SEARCH_FEATURES
    // BitTorrent is attached unconditionally in start(); nothing to toggle. If the
    // node is already up it is available, otherwise it will be once start() runs.
    return d_->running ? (d_->bittorrent != nullptr) : true;
#else
    qWarning() << "BitTorrent features not compiled in";
    return false;
#endif
}

// =========================================================================
// Borrowed librats subsystems
// =========================================================================

librats::Node* P2PTransport::node() const
{
    return d_->node.get();
}

librats::Bittorrent* P2PTransport::bittorrent() const
{
    return d_->bittorrent;
}

librats::StorageManager* P2PTransport::storage() const
{
    return d_->storage;
}

} // namespace rats::net
