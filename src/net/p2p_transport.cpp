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
#include "librats/dht/dht.h"
#include "librats/node/node.h"
#include "librats/peer/peer.h"
#include "librats/peer/peer_id.h"
#include "librats/subsystems/dht_discovery.h"
#include "librats/subsystems/file_transfer.h"
#include "librats/subsystems/hole_punch.h"
#include "librats/subsystems/mdns_discovery.h"
#include "librats/subsystems/message_json.h"
#include "librats/subsystems/peer_exchange.h"
#include "librats/subsystems/port_mapping_service.h"
#include "librats/subsystems/reconnection.h"
#include "librats/subsystems/relay.h"
#include "librats/util/json.h"
#ifdef RATS_SEARCH_FEATURES
#include "librats/subsystems/bittorrent.h"
#endif
#ifdef RATS_STORAGE
#include "librats/storage/storage.h"
#endif
#pragma pop_macro("signals")
#pragma pop_macro("slots")
#pragma pop_macro("emit")

#include <QDebug>
#include <QDir>
#include <QJsonDocument>
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
    librats::MessageJson* messages = nullptr;
    librats::PortMappingService* portMapping = nullptr;
    librats::ReconnectionService* reconnect = nullptr;
    librats::PeerExchange* pex = nullptr;
    librats::HolePunch* holePunch = nullptr;
    librats::Relay* relay = nullptr;
    librats::FileTransfer* fileTransfer = nullptr;
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

    QTimer* updateTimer = nullptr;
    // Last value broadcast by the poll timer. Instance member (not a function
    // static) so multiple P2PTransport instances don't share change detection.
    int lastPeerCount = -1;
    // Last NAT verdict logged, as librats::NatMapping. -1 = nothing logged yet.
    int lastNatMapping = -1;

    int peerCount() const { return node ? static_cast<int>(node->peer_count()) : 0; }

    void setupCallbacks();
    void setupFileTransferCallbacks();
    void registerDispatcher(const QString& type);
    void dispatchMessage(const QString& peerId, const QString& type, const QJsonObject& data);
    void updatePeerCount();
    void requestPeerCountUpdate();
};

void P2PTransport::Private::setupCallbacks()
{
    if (!node) {
        return;
    }

    // Connection callback. Runs on a reactor thread.
    node->on_peer_connected([this](const librats::Peer& peer) {
        QString peerId = QString::fromStdString(peer.id().to_hex());
        qInfo() << "Peer connected:" << peerId.left(8) << "total peers:" << peerCount();
        emit q->peerConnected(peerId);
        requestPeerCountUpdate();
    });

    // Disconnection callback. Runs on a reactor thread.
    node->on_peer_disconnected([this](const librats::PeerId& id, librats::CloseReason reason) {
        QString peerId = QString::fromStdString(id.to_hex());
        // The reason stays inside net/ — it is a librats type, and nothing above
        // this layer knows one. Logging it here is the point of carrying it: a
        // SlowConsumer drop and a peer that simply left are indistinguishable
        // otherwise, which is exactly what made the storage-snapshot flapping
        // read as an ordinary reconnect for as long as it did.
        qInfo() << "Peer disconnected:" << peerId.left(8) << "reason:" << librats::to_string(reason)
                << "total peers:" << peerCount();
        emit q->peerDisconnected(peerId);
        requestPeerCountUpdate();
    });
}

// File-transfer callbacks all fire on librats threads (reactor / worker / disk
// pool). Every one of them hops to q's thread before emitting, for the same
// reason dispatchMessage() does: the services that react to these signals are
// single-threaded, and posting to `q` also lets Qt drop the event if the
// transport is torn down first.
void P2PTransport::Private::setupFileTransferCallbacks()
{
    if (!fileTransfer) {
        return;
    }

    fileTransfer->on_offer([this](const librats::FileTransfer::Offer& offer) {
        const QString peerId = QString::fromStdString(offer.from.to_hex());
        const quint64 id = offer.id;
        const QString name = QString::fromStdString(offer.name);
        const qint64 size = static_cast<qint64>(offer.size);
        // A directory offer is never something this app asked for; refuse it here
        // rather than letting a listener forget to.
        if (offer.is_directory) {
            fileTransfer->reject(offer.from, offer.id);
            return;
        }
        QMetaObject::invokeMethod(
            q, [this, peerId, id, name, size]() { emit q->fileOffered(peerId, id, name, size); }, Qt::QueuedConnection);
    });

    fileTransfer->on_progress([this](const librats::FileTransfer::Progress& progress) {
        const QString peerId = QString::fromStdString(progress.peer.to_hex());
        const quint64 id = progress.id;
        const bool sending = progress.direction == librats::FileTransfer::Direction::Sending;
        const qint64 done = static_cast<qint64>(progress.bytes_transferred);
        const qint64 total = static_cast<qint64>(progress.total_bytes);
        const double rate = progress.transfer_rate_bps;
        QMetaObject::invokeMethod(
            q,
            [this, peerId, id, sending, done, total, rate]() {
                emit q->fileTransferProgress(peerId, id, sending, done, total, rate);
            },
            Qt::QueuedConnection);
    });

    fileTransfer->on_complete([this](const librats::PeerId& peer, uint64_t id, bool success, const std::string& path) {
        const QString peerId = QString::fromStdString(peer.to_hex());
        const quint64 transferId = id;
        const QString file = QString::fromStdString(path);
        QMetaObject::invokeMethod(
            q,
            [this, peerId, transferId, success, file]() {
                emit q->fileTransferFinished(peerId, transferId, success, file);
            },
            Qt::QueuedConnection);
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
    // writes would race main-thread reads/writes of the services' unguarded
    // state. (Posting to `q` also makes Qt drop the event automatically if the
    // transport is torn down before it is delivered.)
    QMetaObject::invokeMethod(
        q,
        [this, peerId, type, data]() {
            auto it = messageHandlers.find(type);
            if (it != messageHandlers.end() && it.value()) {
                it.value()(peerId, data);
            } else {
                qDebug() << "No handler for P2P message type:" << type;
            }
        },
        Qt::QueuedConnection);
}

// Single source of peerCountChanged, and it runs only on q's thread: lastPeerCount
// needs no locking, and a stale value can never overtake a fresher one (a direct
// emit from the timer used to be able to jump ahead of a queued emit from a
// reactor thread, leaving the count behind reality until the next change).
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

    // The mesh's verdict on our own NAT, logged whenever it changes. It only becomes
    // known once a couple of datagram peers have reported where they see us, and it
    // is what decides whether hole punching can reach us at all — so it is the first
    // thing to look at when a node stays unreachable.
    const int mapping = static_cast<int>(node->nat_status().udp_mapping());
    if (mapping != lastNatMapping) {
        lastNatMapping = mapping;
        qInfo() << "NAT mapping:" << librats::to_string(node->nat_status().udp_mapping());
    }
}

// Callable from a reactor thread: hops to q's thread and re-reads the live count
// there, so peers show up immediately instead of waiting for the next poll tick.
void P2PTransport::Private::requestPeerCountUpdate()
{
    QMetaObject::invokeMethod(q, [this]() { updatePeerCount(); }, Qt::QueuedConnection);
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
        // discovery, so bumping it partitions the swarm: only peers carrying the
        // same string can complete a handshake or find each other. Bump it when a
        // wire change makes older peers actively misbehave (not merely miss a
        // feature) — patch releases must keep it identical.
        config.protocol = "rats-search/4";
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

        // Typed JSON messaging: the on()/send() surface used by the peer layer.
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

        // NAT hole punching. Attached before PeerExchange only for readability —
        // PEX resolves the punch capability in start(), by which point every
        // subsystem has attached. Relaying other peers' rendezvous is on: a mesh in
        // which nobody relays cannot punch at all, and one rendezvous costs a few
        // dozen forwarded bytes to a peer we already hold.
        if (holePunchEnabled_) {
            librats::HolePunch::Config hpCfg;
            hpCfg.enable_relay = true;
            d_->holePunch = d_->node->add_subsystem(std::make_unique<librats::HolePunch>(std::move(hpCfg)));
        }

        // The rung below punching: for the peers no punch can ever reach (symmetric
        // NAT on one side, UDP dropped outright), borrow a path through a node both
        // ends already hold. Attach order against HolePunch does not matter — each
        // resolves the other through the ServiceRegistry in start() — and with both
        // on, the ladder runs itself: a peer PEX cannot dial goes to HolePunch, a
        // hopeless one goes to Relay, and a circuit that comes up asks for a punch to
        // replace itself with a direct link.
        //
        // Carrying *other* peers' circuits is a separate, off-by-default decision
        // (relayServe): unlike a hole-punch rendezvous, it spends real uplink on
        // somebody else's transfer.
        if (relayEnabled_) {
            librats::Relay::Config rlCfg;
            rlCfg.serve = relayServeEnabled_;
            d_->relay = d_->node->add_subsystem(std::make_unique<librats::Relay>(std::move(rlCfg)));
        }

        // Peer exchange: peers gossip who else they hold, so the mesh keeps healing
        // when the DHT is throttled or blocked. It is also what makes punching
        // reachable in practice — a PEX entry carries the peer *id* an unreachable
        // address belongs to, and a failed dial there falls back to a punch.
        // Bounded by the same connection budget as everything else: NodeConfig's
        // max_peers only refuses *inbound* peers, and PEX is the one discovery
        // source that compounds — each peer it finds is asked for more peers.
        {
            librats::PeerExchange::Config pexCfg;
            pexCfg.peer_target = d_->maxPeers > 0 ? static_cast<size_t>(d_->maxPeers) : 0;
            d_->pex = d_->node->add_subsystem(std::make_unique<librats::PeerExchange>(std::move(pexCfg)));
        }

        // Bulk file exchange (whole-database dumps). Its callbacks fire on librats
        // threads, so each one hops to q's thread before touching Qt.
        {
            const QString transferDir = d_->dataDirectory + QStringLiteral("/dbsync");
            QDir().mkpath(transferDir); // librats writes in-progress files here
            librats::FileTransfer::Config ftCfg;
            ftCfg.temp_directory = transferDir.toStdString();
            // A database dump is a single multi-gigabyte file, often over a relayed
            // path: the 60 s default drops it on any congestion stall that outlasts
            // a minute, and restarting means resending everything. The offer
            // deadline is separate because the far side answers an offer from its
            // GUI thread, which can legitimately be busy for a while.
            ftCfg.transfer_timeout_secs = 5 * 60;
            ftCfg.offer_timeout_secs = 10 * 60;
            d_->fileTransfer = d_->node->add_subsystem(std::make_unique<librats::FileTransfer>(std::move(ftCfg)));
            d_->setupFileTransferCallbacks();
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
            d_->node.reset();
            d_->dht = nullptr;
            d_->mdns = nullptr;
            d_->messages = nullptr;
            d_->portMapping = nullptr;
            d_->reconnect = nullptr;
            d_->pex = nullptr;
            d_->holePunch = nullptr;
            d_->relay = nullptr;
            d_->fileTransfer = nullptr;
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
        // ReconnectionService persists the peer book; identity persists via
        // data_dir.
        d_->node->stop();
        d_->node.reset();
    }

    d_->dht = nullptr;
    d_->mdns = nullptr;
    d_->messages = nullptr;
    d_->portMapping = nullptr;
    d_->reconnect = nullptr;
    d_->pex = nullptr;
    d_->holePunch = nullptr;
    d_->relay = nullptr;
    d_->fileTransfer = nullptr;
    d_->storage = nullptr;
    d_->bittorrent = nullptr;
    d_->registeredDispatchers.clear();
    d_->bitTorrentEnabled = false;

    d_->running = false;
    d_->lastPeerCount = -1;
    emit peerCountChanged(0);
    emit stopped();

    qInfo() << "P2P transport stopped";
}

bool P2PTransport::isRunning() const
{
    return d_->running && d_->node != nullptr;
}

void P2PTransport::setPortMappingEnabled(bool enabled)
{
    // Subsystems are attached before start(), so this only takes effect on the
    // next P2P (re)start. We just record the preference here.
    portMappingEnabled_ = enabled;
}

void P2PTransport::setHolePunchEnabled(bool enabled)
{
    // Same deal as port mapping: a preference read when the subsystems are built.
    holePunchEnabled_ = enabled;
}

void P2PTransport::setRelayEnabled(bool enabled)
{
    // Same deal again: takes effect on the next (re)start.
    relayEnabled_ = enabled;
}

void P2PTransport::setRelayServeEnabled(bool enabled)
{
    relayServeEnabled_ = enabled;
}

int P2PTransport::relayedPeerCount() const
{
    return d_->relay ? static_cast<int>(d_->relay->circuits()) : 0;
}

int P2PTransport::carriedCircuitCount() const
{
    return d_->relay ? static_cast<int>(d_->relay->carried_circuits()) : 0;
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

// =========================================================================
// File transfer
// =========================================================================

bool P2PTransport::isFileTransferAvailable() const
{
    return isRunning() && d_->fileTransfer != nullptr;
}

quint64 P2PTransport::sendFile(const QString& peerId, const QString& path)
{
    if (!isFileTransferAvailable()) {
        qWarning() << "Cannot send file: P2P transport not running";
        return 0;
    }
    auto id = librats::PeerId::from_hex(peerId.toStdString());
    if (!id) {
        qWarning() << "Cannot send file: invalid peer id" << peerId.left(8);
        return 0;
    }
    return d_->fileTransfer->send_file(*id, path.toStdString());
}

bool P2PTransport::acceptFile(const QString& peerId, quint64 transferId, const QString& destPath)
{
    if (!isFileTransferAvailable()) {
        return false;
    }
    auto id = librats::PeerId::from_hex(peerId.toStdString());
    if (!id) {
        return false;
    }
    d_->fileTransfer->accept(*id, transferId, destPath.toStdString());
    return true;
}

bool P2PTransport::acceptFileResume(
    const QString& peerId, quint64 transferId, const QString& destPath, const QString& partialPath)
{
    if (!isFileTransferAvailable()) {
        return false;
    }
    auto id = librats::PeerId::from_hex(peerId.toStdString());
    if (!id) {
        return false;
    }
    return d_->fileTransfer->accept_resume(*id, transferId, destPath.toStdString(), partialPath.toStdString());
}

bool P2PTransport::rejectFile(const QString& peerId, quint64 transferId)
{
    if (!isFileTransferAvailable()) {
        return false;
    }
    auto id = librats::PeerId::from_hex(peerId.toStdString());
    if (!id) {
        return false;
    }
    d_->fileTransfer->reject(*id, transferId);
    return true;
}

bool P2PTransport::cancelFile(const QString& peerId, quint64 transferId)
{
    if (!isFileTransferAvailable()) {
        return false;
    }
    auto id = librats::PeerId::from_hex(peerId.toStdString());
    if (!id) {
        return false;
    }
    return d_->fileTransfer->cancel(*id, transferId);
}

// =========================================================================
// BitTorrent subsystem
// =========================================================================

bool P2PTransport::isBitTorrentEnabled() const
{
    return d_->bitTorrentEnabled && d_->bittorrent != nullptr;
}

// =========================================================================
// Borrowed librats subsystems
// =========================================================================

librats::Bittorrent* P2PTransport::bittorrent() const
{
    return d_->bittorrent;
}

librats::StorageManager* P2PTransport::storage() const
{
    return d_->storage;
}

} // namespace rats::net
