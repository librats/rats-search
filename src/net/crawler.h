#ifndef RATS_NET_CRAWLER_H
#define RATS_NET_CRAWLER_H

#include "domain/torrent.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <set>
#include <string>

namespace librats {
class Bittorrent;
}

namespace rats::net {

class P2PTransport;

// DHT crawler for the rats::net layer.
//
// Borrows the librats BitTorrent subsystem from a P2PTransport (non-owning)
// and:
//   1. Walks the DHT to expand the routing table.
//   2. Captures announce_peer messages from other clients (spider mode).
//   3. Fetches BEP 9 metadata for freshly discovered info-hashes.
//   4. Emits the resulting domain::Torrent — and nothing more.
//
// The crawler's responsibility ENDS at discovery: it never touches a database
// and never inserts anything. Persistence, filtering and content classification
// are the caller's job, driven off the discovered() signal.
class Crawler : public QObject {
    Q_OBJECT

public:
    // transport is borrowed (non-owning) and must outlive the crawler.
    explicit Crawler(P2PTransport* transport, QObject* parent = nullptr);
    ~Crawler() override;

    // Lifecycle
    bool start();
    void stop();
    bool isRunning() const;

    // Tuning
    void setWalkInterval(int intervalMs);

    // Predicate consulted for each freshly seen info-hash before a BEP 9 metadata
    // fetch is queued: returning true skips the fetch (e.g. the torrent is
    // already indexed). Keeps the crawler DB-free — the caller injects the
    // lookup. Called on the Qt thread (onAnnounce is already marshalled there).
    using KnownHashFilter = std::function<bool(const QString& infoHash)>;
    void setKnownHashFilter(KnownHashFilter filter);

signals:
    void started();
    void stopped();
    void statusChanged(const QString& status);
    // Emitted once metadata has been fetched and a full torrent model built.
    void discovered(const rats::domain::Torrent& torrent);
    void error(const QString& message);

private slots:
    void onSpiderWalk();
    void onIgnoreToggle();
    void processMetadataQueue();

private:
    // A pending metadata fetch.
    // An empty peerIp means "no announcing peer known" — fall back to a DHT
    // search (slow path) instead of a direct peer connection (fast path).
    struct MetadataRequest {
        QString infoHash;
        QString peerIp;
        uint16_t peerPort = 0;
    };

    // Borrowed BitTorrent subsystem (null if the transport has none).
    librats::Bittorrent* bittorrent() const;

    // announce_peer callback, already marshalled onto the Qt thread.
    void onAnnounce(const std::array<uint8_t, 20>& infoHash, const std::string& ip, uint16_t port);

    // Kick off a BEP 9 metadata fetch (fast path if peer is known, else DHT).
    void fetchMetadata(const MetadataRequest& request);

    // Handle a completed metadata fetch (runs on the Qt thread).
    void onMetadataReceived(const rats::domain::Torrent& torrent);

    // Tuning defaults — no magic numbers scattered through the logic.
    static constexpr int DEFAULT_WALK_INTERVAL_MS = 100;
    static constexpr int DEFAULT_IGNORE_INTERVAL_MS = 1000;
    static constexpr int METADATA_QUEUE_INTERVAL_MS = 100;
    static constexpr int MAX_CONCURRENT_METADATA_FETCHES = 10;
    static constexpr size_t MAX_RECENT_HASHES = 10000;

    P2PTransport* transport_; // borrowed, non-owning

    std::atomic<bool> running_;
    std::atomic<int> discoveredCount_;
    std::atomic<int> activeFetches_;

    QTimer* walkTimer_;
    QTimer* ignoreTimer_;
    QTimer* metadataQueueTimer_;

    int walkIntervalMs_;

    std::queue<MetadataRequest> metadataQueue_;
    std::mutex queueMutex_;

    std::set<QString> recentHashes_;
    std::mutex recentHashesMutex_;

    KnownHashFilter knownHashFilter_;
};

} // namespace rats::net

#endif // RATS_NET_CRAWLER_H
