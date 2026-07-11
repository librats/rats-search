#ifndef RATS_NET_SWARM_SCRAPER_H
#define RATS_NET_SWARM_SCRAPER_H

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>

#include <atomic>

namespace rats::net {

// Announces to UDP/HTTP BitTorrent trackers to read a torrent's swarm counts
// (seeders / leechers / completed). Not to be confused with TrackerSiteScraper,
// which scrapes tracker *websites* for poster/description metadata.
//
// Pure transport-side helper: it only talks to trackers and emits results — it
// never touches the database. A higher-level service listens for scraped() and
// persists the values.
//
// Scrapes run on the Qt thread pool via QtConcurrent and are governed by:
//   - a concurrency cap (kMaxConcurrent) with a FIFO queue for overflow,
//   - a per-hash cooldown (kCheckIntervalSecs) so the same torrent is not
//     re-scraped in a tight loop.
class SwarmScraper : public QObject {
    Q_OBJECT

public:
    explicit SwarmScraper(QObject* parent = nullptr);
    ~SwarmScraper() override;

    // Request a scrape for a torrent. `infoHash` must be a 40-char hex string.
    // If `trackers` is empty the built-in default tracker list is used. The call
    // is non-blocking: on success scraped() is emitted later on this object's
    // thread. Duplicate requests within the cooldown window are dropped.
    void requestScrape(const QString& infoHash, const QStringList& trackers = QStringList());

    // Stop scraping: reject any further requests, drop the queue, and wait for
    // in-flight announces to drain so no thread-pool task outlives this object
    // (its completion callback dereferences `this`). Idempotent; safe to call
    // from the destructor. Must be called on this object's thread.
    void stop();

    static constexpr int kTimeoutMs = 15000; // 15 s per announce
    static constexpr int kCheckIntervalSecs = 300; // 5 min per-hash cooldown
    static constexpr int kMaxConcurrent = 5; // concurrent scrapes

signals:
    // Emitted once, on success, with the best swarm counts seen across the
    // torrent's trackers. Not emitted when every tracker fails.
    void scraped(const QString& infoHash, int seeders, int leechers, int completed);

private slots:
    void processQueue();

private:
    // Fan out to `trackers`, keep the best successful result, then emit/dispatch.
    void startScrape(const QString& infoHash, const QStringList& trackers);

    // Drop cooldown entries older than the cooldown window so recentChecks_
    // cannot grow without bound over long uptimes. Caller must hold
    // recentChecksMutex_.
    void pruneStaleChecks(const QDateTime& now);

    // Timing / concurrency constants not exposed as settings.
    static constexpr int kQueuePollIntervalMs = 500; // queue drain cadence
    static constexpr int kInfoHashHexLength = 40; // 20-byte hash as hex
    static constexpr int kMaxPoolThreads = 16; // announce worker threads

    // Per-hash cooldown bookkeeping, pruned periodically in pruneStaleChecks().
    QHash<QString, QDateTime> recentChecks_; // hash -> last check time
    QDateTime lastPrune_;
    mutable QMutex recentChecksMutex_;

    // Overflow queue used when maxConcurrent_ scrapes are already in flight.
    struct PendingRequest {
        QString infoHash;
        QStringList trackers;
    };
    QQueue<PendingRequest> pendingQueue_;
    mutable QMutex queueMutex_;

    int activeRequests_;
    QTimer* queueTimer_;

    // Dedicated pool for the blocking tracker announces so shutdown can drain it
    // deterministically (the global pool is shared and not ours to wait on).
    QThreadPool threadPool_;

    // Set true by stop(): gates new requests and makes in-flight announce loops
    // bail early instead of hammering every remaining tracker while shutting down.
    std::atomic<bool> stopping_{ false };
};

} // namespace rats::net

#endif // RATS_NET_SWARM_SCRAPER_H
