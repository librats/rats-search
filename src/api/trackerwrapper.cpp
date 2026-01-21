#include "trackerwrapper.h"
#include <QDebug>
#include <QtConcurrent>
#include <tracker.h>

// ============================================================================
// Constructor / Destructor
// ============================================================================

TrackerWrapper::TrackerWrapper(QObject *parent)
    : QObject(parent)
    , timeoutMs_(15000)  // 15 seconds default timeout
{
}

TrackerWrapper::~TrackerWrapper()
{
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
    
    // Convert Qt types to std types
    std::string hash_std = hash.toStdString();
    int timeout = timeoutMs_;
    
    // Run the blocking scrape operation in a separate thread
    // Using (void) cast to suppress nodiscard warning - we don't need the QFuture
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
            emit scrapeResult(hash, result);
            if (callback) callback(result);
        }, Qt::QueuedConnection);
    });
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
