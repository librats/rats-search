#include "net/tracker_scraper.h"

#include <QDebug>
#include <QtConcurrent>

// librats' BitTorrent headers use `emit`/`slots`/`signals` as ordinary
// identifiers, which collide with Qt's keyword macros — neutralise them across
// the includes.
#pragma push_macro("emit")
#pragma push_macro("slots")
#pragma push_macro("signals")
#undef emit
#undef slots
#undef signals
#include "bittorrent/tracker.h"
#include "bittorrent/types.h"
#pragma pop_macro("signals")
#pragma pop_macro("slots")
#pragma pop_macro("emit")

namespace rats::net {

namespace {
namespace bt = librats::bittorrent;

// Port announced to trackers. We never accept connections here, so any valid
// BitTorrent port works — trackers only echo it back in their swarm accounting.
constexpr int kAnnouncePort = 6881;

// A small set of well-known public trackers, used when a caller supplies none.
// The librats core no longer bundles a default list, so we keep one here.
const QStringList kDefaultTrackers = {
    QStringLiteral("udp://tracker.opentrackr.org:1337/announce"),
    QStringLiteral("udp://open.tracker.cl:1337/announce"),
    QStringLiteral("udp://tracker.openbittorrent.com:6969/announce"),
    QStringLiteral("udp://exodus.desync.com:6969/announce"),
    QStringLiteral("udp://tracker.torrent.eu.org:451/announce"),
    QStringLiteral("udp://open.demonii.com:1337/announce"),
    QStringLiteral("udp://explodie.org:6969/announce"),
    QStringLiteral("udp://tracker.moeking.me:6969/announce"),
};

// Outcome of a single tracker announce.
struct AnnounceResult {
    int seeders = 0;
    int leechers = 0;
    int completed = 0;
    bool success = false;
    QString error;
};

// Announce to a single tracker to read the torrent's seeder/leecher counts. An
// announce with numwant=0 doubles as a scrape: the tracker still reports the
// swarm counts (BEP 3 complete/incomplete, BEP 15 seeders/leechers).
AnnounceResult announceOne(const std::string& url, const std::string& hashHex, int timeoutMs)
{
    AnnounceResult result;

    auto infoHash = bt::info_hash_from_hex(hashHex);
    if (!infoHash) {
        result.error = QStringLiteral("Invalid hash");
        return result;
    }

    bt::TrackerRequest req;
    req.info_hash = *infoHash;
    req.peer_id = bt::generate_peer_id();
    req.port = kAnnouncePort;
    req.event = bt::TrackerEvent::None;
    req.left = 0;
    req.numwant = 0; // counts only, no peer list

    bt::TrackerResponse resp = bt::announce_to_tracker(url, req, timeoutMs);
    if (resp.success) {
        result.success = true;
        result.seeders = static_cast<int>(resp.complete);
        result.leechers = static_cast<int>(resp.incomplete);
        result.completed = 0; // not reported by an announce
    } else {
        result.error = QString::fromStdString(resp.failure_reason);
    }
    return result;
}
} // namespace

// ============================================================================
// Constructor / Destructor
// ============================================================================

TrackerScraper::TrackerScraper(QObject* parent)
    : QObject(parent)
    , timeoutMs_(kDefaultTimeoutMs)
    , checkIntervalSecs_(kDefaultCheckIntervalSecs)
    , maxConcurrent_(kDefaultMaxConcurrent)
    , activeRequests_(0)
    , queueTimer_(nullptr)
{
    queueTimer_ = new QTimer(this);
    queueTimer_->setInterval(kQueuePollIntervalMs);
    connect(queueTimer_, &QTimer::timeout, this, &TrackerScraper::processQueue);
}

TrackerScraper::~TrackerScraper()
{
    if (queueTimer_) {
        queueTimer_->stop();
    }
}

// ============================================================================
// Public Methods
// ============================================================================

void TrackerScraper::requestScrape(const QString& infoHash, const QStringList& trackers)
{
    if (infoHash.length() != kInfoHashHexLength) {
        qWarning() << "TrackerScraper: ignoring invalid info-hash" << infoHash;
        return;
    }

    const QStringList effectiveTrackers = trackers.isEmpty() ? kDefaultTrackers : trackers;
    if (effectiveTrackers.isEmpty()) {
        return;
    }

    // Deduplicate against the per-hash cooldown, and opportunistically prune
    // stale entries so recentChecks_ stays bounded regardless of uptime.
    {
        QMutexLocker locker(&recentChecksMutex_);
        const QDateTime now = QDateTime::currentDateTime();

        if (!lastPrune_.isValid() || lastPrune_.secsTo(now) >= checkIntervalSecs_) {
            pruneStaleChecks(now);
            lastPrune_ = now;
        }

        auto it = recentChecks_.constFind(infoHash);
        if (it != recentChecks_.constEnd() && it.value().secsTo(now) < checkIntervalSecs_) {
            qDebug() << "TrackerScraper: skipping" << infoHash.left(8) << "- checked" << it.value().secsTo(now)
                     << "secs ago";
            return;
        }
        recentChecks_[infoHash] = now; // mark as being checked
    }

    // Start immediately if under the concurrency cap, otherwise queue.
    {
        QMutexLocker locker(&queueMutex_);
        if (activeRequests_ >= maxConcurrent_) {
            pendingQueue_.enqueue({ infoHash, effectiveTrackers });
            qDebug() << "TrackerScraper: queued" << infoHash.left(8) << "- active:" << activeRequests_
                     << "queued:" << pendingQueue_.size();
            if (!queueTimer_->isActive()) {
                queueTimer_->start();
            }
            return;
        }
        activeRequests_++;
    }

    startScrape(infoHash, effectiveTrackers);
}

// ============================================================================
// Private Methods
// ============================================================================

void TrackerScraper::pruneStaleChecks(const QDateTime& now)
{
    for (auto it = recentChecks_.begin(); it != recentChecks_.end();) {
        if (it.value().secsTo(now) >= checkIntervalSecs_) {
            it = recentChecks_.erase(it);
        } else {
            ++it;
        }
    }
}

void TrackerScraper::processQueue()
{
    QMutexLocker locker(&queueMutex_);

    // Drain as many queued requests as the concurrency cap allows.
    while (!pendingQueue_.isEmpty() && activeRequests_ < maxConcurrent_) {
        PendingRequest req = pendingQueue_.dequeue();
        activeRequests_++;

        // Release the lock while kicking off the (thread-pool) scrape.
        locker.unlock();
        startScrape(req.infoHash, req.trackers);
        locker.relock();
    }

    if (pendingQueue_.isEmpty()) {
        queueTimer_->stop();
    }
}

void TrackerScraper::startScrape(const QString& infoHash, const QStringList& trackers)
{
    const std::string hashStd = infoHash.toStdString();
    const int timeout = timeoutMs_;

    // Run the blocking announces off the Qt thread pool.
    // (void) suppresses the [[nodiscard]] warning on the unused QFuture.
    (void)QtConcurrent::run([this, infoHash, trackers, hashStd, timeout]() {
        // Announce to each tracker and keep the best (highest seeder count)
        // successful result — different trackers see different slices of the swarm.
        AnnounceResult best;
        for (const QString& trackerUrl : trackers) {
            AnnounceResult one = announceOne(trackerUrl.toStdString(), hashStd, timeout);
            if (one.success && (!best.success || one.seeders > best.seeders)) {
                best = one;
            }
        }

        // Return to this object's thread to update state and emit.
        QMetaObject::invokeMethod(
            this,
            [this, infoHash, best]() {
                {
                    QMutexLocker locker(&queueMutex_);
                    if (activeRequests_ > 0) {
                        activeRequests_--;
                    }
                }

                if (best.success) {
                    emit scraped(infoHash, best.seeders, best.leechers, best.completed);
                }

                // Give any queued requests a chance to start now that a slot freed.
                processQueue();
            },
            Qt::QueuedConnection);
    });
}

} // namespace rats::net
