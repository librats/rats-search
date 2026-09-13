#ifndef RATS_NET_P2P_TRANSPORT_H
#define RATS_NET_P2P_TRANSPORT_H

#include <QJsonObject>
#include <QObject>
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
class PeerExchange;
class HolePunch;
class Relay;
class FileTransfer;
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
    void setHolePunchEnabled(bool enabled);
    // Reach peers through a third node when no punch can land, and (separately)
    // carry other peers' circuits — the latter spends our uplink, so it is opted
    // into rather than assumed.
    void setRelayEnabled(bool enabled);
    void setRelayServeEnabled(bool enabled);

    // Peers (generic — identity and count only, no application stats)
    int peerCount() const;
    QString ourPeerId() const;
    // Peers we reach through a relay, and circuits we carry for others. Both 0
    // when relaying is off or the node is down.
    int relayedPeerCount() const;
    int carriedCircuitCount() const;
    size_t dhtNodeCount() const;
    bool isDhtRunning() const;

    // Messaging
    bool sendMessage(const QString& peerId, const QString& type, const QJsonObject& data);
    int broadcastMessage(const QString& type, const QJsonObject& data);

    // Handler registration
    using MessageHandler = std::function<void(const QString& peerId, const QJsonObject& data)>;
    void registerHandler(const QString& type, MessageHandler handler);

    // File transfer ------------------------------------------------------------
    // Bulk file exchange with a single peer, used for whole-database dumps. The
    // model is push: the sender offers, the receiver accepts (choosing where the
    // bytes land) or rejects, then the file streams with integrity checking and
    // backpressure. Everything below marshals onto this object's thread, so the
    // signals arrive where the services live.
    bool isFileTransferAvailable() const;

    // Offer `path` to `peerId`. Returns the transfer id, or 0 if the offer could
    // not be made. Nothing is sent until the peer accepts.
    quint64 sendFile(const QString& peerId, const QString& path);
    // Answer an offer surfaced by fileOffered(). Accepting streams the file into
    // `destPath` (its directory must exist).
    bool acceptFile(const QString& peerId, quint64 transferId, const QString& destPath);
    // Accept into `destPath`, continuing whatever is already in `partialPath` and
    // writing there until the file is complete. The partial survives a failed
    // transfer — that is what makes the next attempt a resume — so the caller owns
    // it and must be sure it belongs to the file being offered. A peer too old to
    // understand the request sends the whole file and the partial is discarded.
    bool acceptFileResume(
        const QString& peerId, quint64 transferId, const QString& destPath, const QString& partialPath);
    bool rejectFile(const QString& peerId, quint64 transferId);
    bool cancelFile(const QString& peerId, quint64 transferId);

    // BitTorrent subsystem (optional feature)
    bool isBitTorrentEnabled() const;

    // Borrowed librats subsystems — non-owning, valid only while running.
    librats::Bittorrent* bittorrent() const;
    librats::StorageManager* storage() const;

signals:
    void started();
    void stopped();
    void peerCountChanged(int count);
    void peerConnected(const QString& peerId);
    void peerDisconnected(const QString& peerId);

    // A peer offered us a file. Answer with acceptFile()/rejectFile() — an offer
    // left unanswered is dropped by both sides once the offer deadline passes.
    void fileOffered(const QString& peerId, quint64 transferId, const QString& name, qint64 size);
    // Progress for both directions; `sending` tells them apart.
    void fileTransferProgress(
        const QString& peerId, quint64 transferId, bool sending, qint64 transferred, qint64 total, double bytesPerSec);
    // Terminal outcome. `path` is the destination file on the receiving side and
    // empty on the sending side.
    //
    // The peer id is part of the identity, not a convenience: incoming transfer
    // ids are allocated by the *sender*, so every peer's first offer is id 1 and
    // two peers routinely have the same id in flight at once. Correlate on the
    // (peerId, transferId) pair — matching on the id alone lets one peer's
    // outcome be mistaken for another's.
    void fileTransferFinished(const QString& peerId, quint64 transferId, bool success, const QString& path);

private:
    struct Private;
    std::unique_ptr<Private> d_;

    // Preferences only; both subsystems are attached before start(), so a change
    // takes effect on the next (re)start.
    bool portMappingEnabled_ = true;
    bool holePunchEnabled_ = true;
    bool relayEnabled_ = true;
    bool relayServeEnabled_ = false;
};

} // namespace rats::net

#endif // RATS_NET_P2P_TRANSPORT_H
