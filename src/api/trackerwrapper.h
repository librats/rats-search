#ifndef TRACKERWRAPPER_H
#define TRACKERWRAPPER_H

#include <QObject>
#include <QString>
#include <QStringList>
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
     * @brief Get list of default trackers
     */
    static QStringList defaultTrackers();

signals:
    /**
     * @brief Emitted when scrape result is received
     */
    void scrapeResult(const QString& hash, const TrackerResult& result);

private:
    int timeoutMs_;
};

#endif // TRACKERWRAPPER_H
