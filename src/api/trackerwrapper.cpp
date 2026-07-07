#include "trackerwrapper.h"
#include <QDebug>
#include <QtConcurrent>
// librats' BitTorrent headers use `emit`/`slots`/`signals` as ordinary identifiers,
// which collide with Qt's keyword macros — neutralise them across the includes.
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

namespace {
namespace bt = librats::bittorrent;

// A small set of well-known public trackers, used by scrapeMultiple(). The new
// librats core no longer bundles a default list, so we keep one here.
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

// Announce to a single tracker to read the torrent's seeder/leecher counts. An
// announce with numwant=0 doubles as a scrape: the tracker still reports the swarm
// counts (BEP 3 complete/incomplete, BEP 15 seeders/leechers).
TrackerResult announceOne(const std::string& url, const std::string& hashHex, int timeoutMs)
{
    TrackerResult result;
    result.tracker = QString::fromStdString(url);

    auto infoHash = bt::info_hash_from_hex(hashHex);
    if (!infoHash) {
        result.error = QStringLiteral("Invalid hash");
        return result;
    }

    bt::TrackerRequest req;
    req.info_hash = *infoHash;
    req.peer_id   = bt::generate_peer_id();
    req.port      = 6881;
    req.event     = bt::TrackerEvent::None;
    req.left      = 0;
    req.numwant   = 0;  // counts only, no peer list

    bt::TrackerResponse resp = bt::announce_to_tracker(url, req, timeoutMs);
    if (resp.success) {
        result.success   = true;
        result.seeders   = static_cast<int>(resp.complete);
        result.leechers  = static_cast<int>(resp.incomplete);
        result.completed = 0;  // not reported by an announce
    } else {
        result.error = QString::fromStdString(resp.failure_reason);
    }
    return result;
}
} // namespace

// ============================================================================
// Constructor / Destructor
// ============================================================================

TrackerWrapper::TrackerWrapper(QObject *parent)
    : QObject(parent)
    , timeoutMs_(15000)        // 15 seconds default timeout
    , checkIntervalSecs_(300)  // 5 minutes between checks for same hash
    , maxConcurrent_(5)        // Max 5 concurrent tracker requests
    , activeRequests_(0)
    , queueTimer_(nullptr)
{
    // Create queue processing timer
    queueTimer_ = new QTimer(this);
    queueTimer_->setInterval(500);  // Check queue every 500ms
    connect(queueTimer_, &QTimer::timeout, this, &TrackerWrapper::processQueue);
}

TrackerWrapper::~TrackerWrapper()
{
    if (queueTimer_) {
        queueTimer_->stop();
    }
}

// ============================================================================
// Public Methods
// ============================================================================

void TrackerWrapper::scrape(const QString& trackerUrl, const QString& hash,
                            std::function<void(const TrackerResult&)> callback)
{
    if (hash.length() != 40) {
        TrackerResult result;
        result.error = "Invalid hash length";
        if (callback) callback(result);
        return;
    }
    
    // Convert Qt types to std types
    std::string tracker_url_std = trackerUrl.toStdString();
    std::string hash_std = hash.toStdString();
    int timeout = timeoutMs_;
    
    // Run the blocking scrape operation in a separate thread
    // Using (void) cast to suppress nodiscard warning - we don't need the QFuture
    (void)QtConcurrent::run([this, tracker_url_std, hash_std, hash, callback, timeout]() {
        TrackerResult result = announceOne(tracker_url_std, hash_std, timeout);

        // Emit signal and call callback in main thread
        QMetaObject::invokeMethod(this, [this, hash, result, callback]() {
            emit scrapeResult(hash, result);
            if (callback) callback(result);
        }, Qt::QueuedConnection);
    });
}

void TrackerWrapper::scrapeMultiple(const QString& hash,
                                    std::function<void(const TrackerResult&)> callback)
{
    if (hash.length() != 40) {
        TrackerResult result;
        result.error = "Invalid hash length";
        if (callback) callback(result);
        return;
    }
    
    // Check if this hash was checked recently (deduplication)
    {
        QMutexLocker locker(&recentChecksMutex_);
        if (recentChecks_.contains(hash)) {
            QDateTime lastCheck = recentChecks_[hash];
            if (lastCheck.secsTo(QDateTime::currentDateTime()) < checkIntervalSecs_) {
                qDebug() << "TrackerWrapper: Skipping" << hash.left(8) 
                         << "- checked" << lastCheck.secsTo(QDateTime::currentDateTime()) << "secs ago";
                TrackerResult result;
                result.error = "Recently checked, skipping";
                if (callback) callback(result);
                return;
            }
        }
        // Mark as being checked
        recentChecks_[hash] = QDateTime::currentDateTime();
    }
    
    // Check if we can start immediately or need to queue
    {
        QMutexLocker locker(&queueMutex_);
        qInfo() << "TrackerWrapper: requesting scrape multiple for" << hash << "activeRequests:" << activeRequests_;
        if (activeRequests_ >= maxConcurrent_) {
            // Queue the request
            PendingRequest req;
            req.hash = hash;
            req.callback = callback;
            pendingQueue_.enqueue(req);
            
            qDebug() << "TrackerWrapper: Queued" << hash.left(8) 
                     << "- active:" << activeRequests_ << "queued:" << pendingQueue_.size();
            
            // Start queue timer if not running
            if (!queueTimer_->isActive()) {
                queueTimer_->start();
            }
            return;
        }
        
        activeRequests_++;
    }
    
    // Start the scrape
    doScrapeMultiple(hash, callback);
}

int TrackerWrapper::pendingCount() const
{
    QMutexLocker locker(&queueMutex_);
    return pendingQueue_.size();
}

QStringList TrackerWrapper::defaultTrackers()
{
    return kDefaultTrackers;
}

// ============================================================================
// Private Methods
// ============================================================================

void TrackerWrapper::processQueue()
{
    QMutexLocker locker(&queueMutex_);
    
    // Process as many queued requests as we have capacity
    while (!pendingQueue_.isEmpty() && activeRequests_ < maxConcurrent_) {
        PendingRequest req = pendingQueue_.dequeue();
        activeRequests_++;
        
        // Release lock before starting scrape
        locker.unlock();
        doScrapeMultiple(req.hash, req.callback);
        locker.relock();
    }
    
    // Stop timer if queue is empty
    if (pendingQueue_.isEmpty()) {
        queueTimer_->stop();
    }
}

void TrackerWrapper::doScrapeMultiple(const QString& hash,
                                       std::function<void(const TrackerResult&)> callback)
{
    // Convert Qt types to std types
    std::string hash_std = hash.toStdString();
    int timeout = timeoutMs_;
    
    // Run the blocking scrape operation in a separate thread
    (void)QtConcurrent::run([this, hash_std, hash, callback, timeout]() {
        // Announce to each default tracker and keep the best (highest seeder count)
        // successful result — different trackers see different slices of the swarm.
        TrackerResult result;
        for (const QString& trackerUrl : kDefaultTrackers) {
            TrackerResult one = announceOne(trackerUrl.toStdString(), hash_std, timeout);
            if (one.success && (!result.success || one.seeders > result.seeders)) {
                result = one;
            } else if (!result.success && !one.error.isEmpty()) {
                result.error = one.error;  // remember a reason if nothing succeeds
            }
        }

        // Emit signal and call callback in main thread
        QMetaObject::invokeMethod(this, [this, hash, result, callback]() {
            // Decrement active requests
            {
                QMutexLocker locker(&queueMutex_);
                activeRequests_--;
            }
            
            emit scrapeResult(hash, result);
            if (callback) callback(result);
            
            // Trigger queue processing
            processQueue();
        }, Qt::QueuedConnection);
    });
}
