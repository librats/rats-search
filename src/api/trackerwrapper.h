#ifndef TRACKERWRAPPER_H
#define TRACKERWRAPPER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QQueue>
#include <QDateTime>
#include <QTimer>
#include <QMutex>
#include <functional>

/**
 * @brief TrackerResult - Result from tracker scrape
 * 
 * This struct maintains compatibility with the old TrackerChecker interface.
 */
struct TrackerResult {
    QString tracker;
    int seeders = 0;
    int completed = 0;
    int leechers = 0;
    bool success = false;
    QString error;
    
    bool isValid() const { return success && seeders >= 0; }
};

/**
 * @brief TrackerWrapper - Qt wrapper for librats tracker functionality
 * 
 * This class provides a Qt-friendly interface to the librats tracker scraping
 * functionality. It uses QtConcurrent to run blocking scrape operations
 * asynchronously.
 * 
 * Features:
 * - Automatic rate limiting (max concurrent requests)
 * - Deduplication (skip recently checked hashes)
 * - Queue-based processing to avoid tracker overload
 * 
 * Usage:
 *   TrackerWrapper wrapper;
 *   wrapper.scrapeMultiple(hash, [](const TrackerResult& r) {
 *       if (r.success) {
 *           qInfo() << "Seeders:" << r.seeders;
 *       }
 *   });
 */
class TrackerWrapper : public QObject
{
    Q_OBJECT

public:
    explicit TrackerWrapper(QObject *parent = nullptr);
    ~TrackerWrapper();
    
    /**
     * @brief Scrape a single tracker for torrent statistics
     * @param trackerUrl Full tracker URL (udp://... or http://...)
     * @param hash Torrent info hash (40 char hex)
     * @param callback Called with result
     */
    void scrape(const QString& trackerUrl, const QString& hash,
                std::function<void(const TrackerResult&)> callback);
    
    /**
     * @brief Scrape from multiple default trackers
     * @param hash Torrent info hash (40 char hex)
     * @param callback Called with aggregated result (best values)
     * 
     * This method includes rate limiting and deduplication:
     * - Skips if hash was checked within last checkInterval_ seconds
     * - Queues request if too many concurrent requests are running
     */
    void scrapeMultiple(const QString& hash,
                        std::function<void(const TrackerResult&)> callback);
    
    /**
     * @brief Get request timeout in milliseconds
     */
    int timeout() const { return timeoutMs_; }
    
    /**
     * @brief Set request timeout
     */
    void setTimeout(int ms) { timeoutMs_ = ms; }
    
    /**
     * @brief Set minimum interval between checks for the same hash (seconds)
     * Default: 300 (5 minutes)
     */
    void setCheckInterval(int seconds) { checkIntervalSecs_ = seconds; }
    
    /**
     * @brief Set maximum concurrent tracker checks
     * Default: 5
     */
    void setMaxConcurrent(int max) { maxConcurrent_ = max; }
    
    /**
     * @brief Get list of default trackers
     */
    static QStringList defaultTrackers();
    
    /**
     * @brief Get number of pending requests in queue
     */
    int pendingCount() const;

signals:
    /**
     * @brief Emitted when scrape result is received
     */
    void scrapeResult(const QString& hash, const TrackerResult& result);

private slots:
    void processQueue();

private:
    void doScrapeMultiple(const QString& hash,
                          std::function<void(const TrackerResult&)> callback);
    
    int timeoutMs_;
    int checkIntervalSecs_;
    int maxConcurrent_;
    
    // Rate limiting
    QHash<QString, QDateTime> recentChecks_;  // hash -> last check time
    mutable QMutex recentChecksMutex_;
    
    // Queue for pending requests
    struct PendingRequest {
        QString hash;
        std::function<void(const TrackerResult&)> callback;
    };
    QQueue<PendingRequest> pendingQueue_;
    mutable QMutex queueMutex_;
    
    int activeRequests_;
    QTimer* queueTimer_;
};

#endif // TRACKERWRAPPER_H
