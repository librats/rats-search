#ifndef RATS_NET_P2P_TRANSPORT_H
#define RATS_NET_P2P_TRANSPORT_H

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <functional>
#include <memory>

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
} // namespace librats

namespace rats::net {

// Pure P2P transport over librats. Moves bytes between peers and nothing more:
// it owns the librats Node, runs discovery (DHT/mDNS) and pub/sub, delivers
// inbound messages to registered handlers, and exposes the librats subsystems
// (bittorrent, storage) that the engine/crawler/store borrow.
//
// Everything application-specific that used to live here — the peer handshake
// with torrent counts, the "rats-search" search/announce topics — now lives in
// the service/peer layer, which builds on registerHandler()/sendMessage().
class P2PTransport : public QObject {
    Q_OBJECT

public:
    P2PTransport(int port, int dhtPort, QString dataDirectory, int maxPeers = 10, QObject* parent = nullptr);
    ~P2PTransport() override;

    // Lifecycle
    bool start();
    void stop();
    bool isRunning() const;

    void setPortMappingEnabled(bool enabled);
    bool portMappingEnabled() const { return portMappingEnabled_; }

    // Peers (generic — identity and count only, no application stats)
    int peerCount() const;
    QString ourPeerId() const;
    size_t dhtNodeCount() const;
    bool isDhtRunning() const;

    // Messaging
    bool sendMessage(const QString& peerId, const QString& type, const QJsonObject& data);
    int broadcastMessage(const QString& type, const QJsonObject& data);

    // Handler registration
    using MessageHandler = std::function<void(const QString& peerId, const QJsonObject& data)>;
    void registerHandler(const QString& type, MessageHandler handler);

    // BitTorrent subsystem (optional feature)
    bool isBitTorrentEnabled() const;

    // Borrowed librats subsystems — non-owning, valid only while running.
    librats::Node* node() const;
    librats::Bittorrent* bittorrent() const;
    librats::StorageManager* storage() const;

signals:
    void started();
    void stopped();
    void error(const QString& message);
    void peerCountChanged(int count);
    void peerConnected(const QString& peerId);
    void peerDisconnected(const QString& peerId);

private:
    struct Private;
    std::unique_ptr<Private> d_;

    // Preference only; the port-mapping subsystem is attached before start(), so
    // this takes effect on the next (re)start. Kept as a direct member because
    // the inline portMappingEnabled() getter needs it while Private is still
    // opaque.
    bool portMappingEnabled_ = true;
};

} // namespace rats::net

#endif // RATS_NET_P2P_TRANSPORT_H
