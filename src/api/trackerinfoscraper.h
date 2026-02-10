#ifndef TRACKERINFOSCRAPER_H
#define TRACKERINFOSCRAPER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QHash>
#include <QMutex>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

/**
 * @brief Scraped information from a tracker website
 * 
 * Mirrors the data returned by legacy strategies (rutracker.js, nyaa.js).
 * This data is stored in the torrent's `info` JSON field in the database.
 */
struct TrackerScrapedInfo {
    QString trackerName;       // "rutracker", "nyaa"
    QString name;              // Torrent name from tracker
    QString poster;            // Poster image URL
    QString description;       // Description text
    QString contentCategory;   // Category from tracker (e.g. breadcrumb path)
    int threadId = 0;          // Thread/topic ID on the tracker
    QString threadUrl;         // Direct URL to the torrent page
    bool success = false;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        if (!name.isEmpty()) obj["name"] = name;
        if (!poster.isEmpty()) obj["poster"] = poster;
        if (!description.isEmpty()) obj["description"] = description;
        if (!contentCategory.isEmpty()) obj["contentCategory"] = contentCategory;
        if (threadId > 0) obj["threadId"] = threadId;
        if (!threadUrl.isEmpty()) obj["threadUrl"] = threadUrl;
        return obj;
    }
};

/**
 * @brief TrackerInfoScraper - Scrapes torrent details from tracker websites
 * 
 * Restores the legacy "strategies" functionality from the old Electron client.
 * Each strategy fetches a tracker's website by info hash and extracts:
 * - Torrent name, poster image, description, category
 * - Tracker-specific thread/topic IDs for linking
 * 
 * Supported trackers:
 * - RuTracker (rutracker.org) - Russian torrent tracker
 * - Nyaa (nyaa.si) - Anime/Japanese media tracker
 * 
 * Results are merged into the torrent's `info` JSON field in the database,
 * including a `trackers` array listing which trackers had data (like legacy).
 * 
 * Usage:
 *   scraper->scrapeAll(hash, [](const QString& hash, const QJsonObject& mergedInfo) {
 *       // mergedInfo contains: poster, description, trackers[], 
 *       //                      rutrackerThreadId, contentCategory, etc.
 *   });
 */
class TrackerInfoScraper : public QObject
{
    Q_OBJECT

public:
    explicit TrackerInfoScraper(QObject *parent = nullptr);
    ~TrackerInfoScraper();
    
    /**
     * @brief Callback with merged info from all strategies
     * @param hash The torrent info hash
     * @param mergedInfo JSON object to merge into torrent's `info` field
     *        Contains: poster, description, trackers[], rutrackerThreadId, etc.
     */
    using MergedCallback = std::function<void(const QString& hash, const QJsonObject& mergedInfo)>;
    
    /**
     * @brief Scrape all supported trackers for a torrent hash
     * 
     * Runs all strategies in parallel. As results arrive, they are merged.
     * The callback is called once when all strategies have completed (or timed out).
     * 
     * @param hash 40-char hex info hash
     * @param existingInfo Existing info JSON (for merging, preserves existing data)
     * @param callback Called with merged results
     */
    void scrapeAll(const QString& hash, 
                   const QJsonObject& existingInfo,
                   MergedCallback callback);
    
    /**
     * @brief Check if a hash was recently scraped (to avoid duplicate work)
     */
    bool wasRecentlyScraped(const QString& hash) const;
    
    /**
     * @brief Enable/disable scraping
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }
    
    /**
     * @brief Set HTTP request timeout (ms)
     */
    void setTimeout(int ms) { timeoutMs_ = ms; }
    
    /**
     * @brief Set cooldown between scrapes for the same hash (seconds)
     * Default: 3600 (1 hour)
     */
    void setCooldownSecs(int secs) { cooldownSecs_ = secs; }

signals:
    /**
     * @brief Emitted when tracker info is found for a torrent
     * @param hash Torrent info hash
     * @param trackerName Name of the tracker ("rutracker", "nyaa")
     * @param info Scraped information
     */
    void trackerInfoFound(const QString& hash, const QString& trackerName, 
                          const TrackerScrapedInfo& info);
    
    /**
     * @brief Emitted when all strategies complete for a hash
     * @param hash Torrent info hash
     * @param mergedInfo Merged info JSON to store in database
     */
    void scrapeCompleted(const QString& hash, const QJsonObject& mergedInfo);

private:
    void scrapeRutracker(const QString& hash);
    void scrapeNyaa(const QString& hash);
    void scrapeNyaaViewPage(const QString& hash, const QString& viewUrl);
    
    TrackerScrapedInfo parseRutrackerHtml(const QByteArray& rawData);
    TrackerScrapedInfo parseNyaaSearchHtml(const QByteArray& rawData);
    TrackerScrapedInfo parseNyaaViewHtml(const QByteArray& rawData);
    
    void onStrategyComplete(const QString& hash, const TrackerScrapedInfo& info);
    void checkAllComplete(const QString& hash);
    
    /**
     * @brief Extract text content between HTML tags matching a pattern
     */
    static QString extractTagContent(const QString& html, const QString& tagPattern);
    
    /**
     * @brief Extract attribute value from an HTML element
     */
    static QString extractAttribute(const QString& html, const QString& elementPattern, 
                                     const QString& attrName);
    
    /**
     * @brief Strip HTML tags and decode entities, return plain text
     */
    static QString stripHtml(const QString& html);
    
    /**
     * @brief Decode Windows-1251 encoded bytes to QString
     * Manual implementation since Qt6 without ICU doesn't support this encoding.
     */
    static QString decodeWindows1251(const QByteArray& data);
    
    QNetworkAccessManager* networkManager_;
    bool enabled_ = true;
    int timeoutMs_ = 20000;      // 20 seconds
    int cooldownSecs_ = 3600;    // 1 hour
    
    // Cooldown tracking
    mutable QMutex recentChecksMutex_;
    QHash<QString, QDateTime> recentChecks_;
    
    // Pending scrape tracking (for merging results from multiple strategies)
    struct PendingScrape {
        QJsonObject existingInfo;
        MergedCallback callback;
        int pendingCount = 0;      // Number of strategies still in progress
        QVector<TrackerScrapedInfo> results;
    };
    mutable QMutex pendingMutex_;
    QHash<QString, PendingScrape> pendingScrapes_;
    
    static constexpr int STRATEGY_COUNT = 2;  // rutracker + nyaa
};

#endif // TRACKERINFOSCRAPER_H
