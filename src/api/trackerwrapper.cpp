#include "trackerwrapper.h"
#include <QDebug>
#include <QtConcurrent>
#include <tracker.h>

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
        TrackerResult result;
        
        librats::scrape_tracker(tracker_url_std, hash_std, 
            [&result](const librats::ScrapeResult& sr) {
                result.tracker = QString::fromStdString(sr.tracker);
                result.seeders = static_cast<int>(sr.seeders);
                result.leechers = static_cast<int>(sr.leechers);
                result.completed = static_cast<int>(sr.completed);
                result.success = sr.success;
                result.error = QString::fromStdString(sr.error);
            }, timeout);
        
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
    QStringList result;
    
    std::vector<std::string> trackers = librats::get_default_trackers();
    for (const auto& tracker : trackers) {
        result.append(QString::fromStdString(tracker));
    }
    
    return result;
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
        TrackerResult result;
        
        librats::scrape_multiple_trackers(hash_std, 
            [&result](const librats::ScrapeResult& sr) {
                result.tracker = QString::fromStdString(sr.tracker);
                result.seeders = static_cast<int>(sr.seeders);
                result.leechers = static_cast<int>(sr.leechers);
                result.completed = static_cast<int>(sr.completed);
                result.success = sr.success;
                result.error = QString::fromStdString(sr.error);
            }, timeout);
        
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
