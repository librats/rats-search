#ifndef RATS_NET_TRACKER_SCRAPER_H
#define RATS_NET_TRACKER_SCRAPER_H

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace rats::net {

// Scrapes UDP/HTTP BitTorrent trackers for swarm statistics (seeders / leechers
// / completed) for a given info-hash. Pure transport-side helper: it only talks
// to trackers and emits results — it never touches the database. A higher-level
// service is expected to listen for scraped() and persist the values.
//
// Scrapes run on the Qt thread pool via QtConcurrent and are governed by:
//   - a concurrency cap (kDefaultMaxConcurrent) with a FIFO queue for overflow,
//   - a per-hash cooldown (kDefaultCheckIntervalSecs) so the same torrent is
//   not
//     re-scraped in a tight loop.
class TrackerScraper : public QObject {
    Q_OBJECT

public:
    explicit TrackerScraper(QObject* parent = nullptr);
    ~TrackerScraper() override;

    // Request a scrape for a torrent. `infoHash` must be a 40-char hex string.
    // If `trackers` is empty the built-in default tracker list is used. The call
    // is non-blocking: on success scraped() is emitted later on this object's
    // thread. Duplicate requests within the cooldown window are dropped.
    void requestScrape(const QString& infoHash, const QStringList& trackers = QStringList());

    // Request timeout per tracker announce, in milliseconds.
    int timeout() const { return timeoutMs_; }
    void setTimeout(int ms) { timeoutMs_ = ms; }

    // Minimum interval between scrapes of the same hash, in seconds.
    void setCheckInterval(int seconds) { checkIntervalSecs_ = seconds; }

    // Maximum number of concurrent tracker scrapes.
    void setMaxConcurrent(int max) { maxConcurrent_ = max; }

    // Tunable defaults (override via the setters above).
    static constexpr int kDefaultTimeoutMs = 15000; // 15 s per announce
    static constexpr int kDefaultCheckIntervalSecs = 300; // 5 min per-hash cooldown
    static constexpr int kDefaultMaxConcurrent = 5; // concurrent scrapes

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

    int timeoutMs_;
    int checkIntervalSecs_;
    int maxConcurrent_;

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
};

} // namespace rats::net

#endif // RATS_NET_TRACKER_SCRAPER_H
