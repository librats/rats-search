#include "net/swarm_scraper.h"

#include <QDebug>
#include <QMutex>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>

// librats' BitTorrent headers use `emit`/`slots`/`signals` as ordinary
// identifiers, which collide with Qt's keyword macros — neutralise them across
// the includes.
#pragma push_macro("emit")
#pragma push_macro("slots")
#pragma push_macro("signals")
#undef emit
#undef slots
#undef signals
#include "librats/bittorrent/tracker.h"
#include "librats/bittorrent/types.h"
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
    QStringLiteral("udp://tracker.evergarden.uk:1914/announce"),
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

// Shared state for one hash's parallel tracker announces. Each tracker runs as
// its own pool task; they merge into `best` under `mutex` and the task that
// drops `remaining` to zero reports the aggregate back on the object thread.
struct ScrapeState {
    QMutex mutex;
    AnnounceResult best;
    std::atomic<int> remaining { 0 };
};

// Announce to a single tracker to read the torrent's seeder/leecher counts. An
// announce with numwant=0 doubles as a scrape: the tracker still reports the
// swarm counts (BEP 3 complete/incomplete, BEP 15 seeders/leechers).
AnnounceResult announceOne(
    const std::string& url, const std::string& hashHex, int timeoutMs, const std::function<bool()>& cancelled)
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

    bt::TrackerResponse resp = bt::announce_to_tracker(url, req, timeoutMs, cancelled);
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

SwarmScraper::SwarmScraper(QObject* parent)
    : QObject(parent), queue_({ kMaxConcurrent, kInteractiveSlots, kMaxQueued, kCooldownSecs, kRetrySecs })
{
    // Sized to run a hash's trackers concurrently (plus some cross-hash overlap).
    // The tasks are blocking UDP announces (I/O-bound, not CPU-bound), so a cap
    // above the core count is fine; it just bounds how many sockets wait at once.
    threadPool_.setMaxThreadCount(kMaxPoolThreads);
}

SwarmScraper::~SwarmScraper()
{
    stop();
}

void SwarmScraper::stop()
{
    stopping_.store(true);
    queue_.clear();
    // Drop tasks that have not started yet, then wait for the running announces to
    // finish. In-flight tasks check stopping_ before each announce, so they bail
    // after at most the announce already in progress rather than walking every
    // remaining tracker. Waiting here guarantees no pool task outlives `this`.
    threadPool_.clear();
    threadPool_.waitForDone();
}

// ============================================================================
// Public Methods
// ============================================================================

bool SwarmScraper::requestScrape(const QString& infoHash, ScrapePriority priority)
{
    if (stopping_.load()) {
        return false; // shutting down — accept no new work
    }

    if (infoHash.length() != kInfoHashHexLength) {
        qWarning() << "SwarmScraper: ignoring invalid info-hash" << infoHash;
        return false;
    }

    switch (queue_.submit(infoHash, priority)) {
    case ScrapeQueue::Outcome::Started:
        startScrape(infoHash);
        return true;
    case ScrapeQueue::Outcome::Queued:
    case ScrapeQueue::Outcome::Running:
        return true;
    case ScrapeQueue::Outcome::CoolingDown:
        return false;
    }
    return false;
}

// ============================================================================
// Private Methods
// ============================================================================

void SwarmScraper::startScrape(const QString& infoHash)
{
    const std::string hashStd = infoHash.toStdString();
    const int timeout = kTimeoutMs;

    // A hash's trackers announce in parallel (each its own pool task) rather than
    // walking them one-by-one: different trackers see different swarm slices, and
    // parallelising caps a hash's latency at ~one announce round instead of
    // trackers × timeout. They merge into a shared best-result; the last one to
    // finish reports back on this object's thread and frees the hash's slot.
    auto state = std::make_shared<ScrapeState>();
    state->remaining.store(static_cast<int>(kDefaultTrackers.size()));

    for (const QString& trackerUrl : kDefaultTrackers) {
        const std::string url = trackerUrl.toStdString();
        threadPool_.start([this, infoHash, state, url, hashStd, timeout]() {
            // Skip the (blocking) announce entirely once shutdown starts, so a
            // stop() drains the pool promptly instead of firing every tracker.
            // The cancel predicate also aborts an announce already in progress
            // within ~one poll slice, so waitForDone() in stop() returns fast.
            if (!stopping_.load()) {
                AnnounceResult one = announceOne(url, hashStd, timeout, [this] { return stopping_.load(); });
                if (one.success) {
                    QMutexLocker locker(&state->mutex);
                    if (!state->best.success || one.seeders > state->best.seeders) {
                        state->best = one;
                    }
                }
            }

            // Last tracker for this hash done → aggregate and report back.
            if (state->remaining.fetch_sub(1, std::memory_order_acq_rel) != 1) {
                return;
            }

            AnnounceResult best;
            {
                QMutexLocker locker(&state->mutex);
                best = state->best;
            }
            QMetaObject::invokeMethod(
                this,
                [this, infoHash, best]() {
                    onScrapeFinished(infoHash, best.success, best.seeders, best.leechers, best.completed);
                },
                Qt::QueuedConnection);
        });
    }
}

void SwarmScraper::onScrapeFinished(const QString& infoHash, bool success, int seeders, int leechers, int completed)
{
    const QStringList next = queue_.finish(infoHash, success);

    if (success) {
        emit scraped(infoHash, seeders, leechers, completed);
    }

    if (!stopping_.load()) {
        for (const QString& hash : next) {
            startScrape(hash);
        }
    }
}

} // namespace rats::net
