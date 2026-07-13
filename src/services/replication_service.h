#ifndef RATS_SERVICE_REPLICATION_SERVICE_H
#define RATS_SERVICE_REPLICATION_SERVICE_H

#include <QObject>
#include <atomic>

class QTimer;

namespace rats::net {
class P2PTransport;
}

namespace rats::service {

// Periodically asks connected peers for random torrents so the local index
// converges toward the swarm's. Owns only the timing/adaptive-interval policy
// and the broadcast; the actual "randomTorrents" request/response wire handling
// lives in the peer API, which calls notifyReceived() as torrents arrive.
class ReplicationService : public QObject {
    Q_OBJECT

public:
    explicit ReplicationService(net::P2PTransport* transport, QObject* parent = nullptr);

    void setEnabled(bool enabled); // config gate (p2pReplication)
    void start();
    void stop();

    // Called by the peer API each time a replicated torrent is inserted.
    void notifyReceived() { ++receivedThisCycle_; }

private:
    void performCycle();

    net::P2PTransport* transport_;
    QTimer* timer_;
    int interval_;
    // Reset at the start of every cycle; drives the adaptive interval.
    std::atomic<int> receivedThisCycle_ { 0 };
    qint64 totalReplicated_ = 0;
    bool enabled_ = false;
};

} // namespace rats::service

#endif // RATS_SERVICE_REPLICATION_SERVICE_H
