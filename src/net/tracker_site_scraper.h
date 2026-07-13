#ifndef RATS_NET_TRACKER_SITE_SCRAPER_H
#define RATS_NET_TRACKER_SITE_SCRAPER_H

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>

class QNetworkAccessManager;

namespace rats::net {

// Result of scraping a single tracker website for one info-hash. Internal DTO:
// the parse* helpers fill it in and checkAllComplete() folds the per-tracker
// DTOs into the single JSON object carried by scraped().
struct TrackerSiteInfo {
    QString trackerName; // "rutracker" | "nyaa"
    QString name; // torrent title as shown on the tracker
    QString poster; // poster image URL
    QString description; // plain-text description
    QString contentCategory; // category / breadcrumb path
    int threadId = 0; // topic / view id on the tracker
    bool success = false;
};

// Scrapes tracker websites (RuTracker, Nyaa) for a torrent's poster image,
// description and category, using hand-rolled QRegularExpression HTML parsing
// (faithfully ported from the legacy Electron "strategies"). Not to be confused
// with SwarmScraper, which announces to trackers for seeder/leecher counts.
//
// Pure network-side helper: it NEVER touches the database. All strategies for a
// hash run in parallel; once they finish the merged metadata is delivered via
// the scraped() signal as a JSON object whose keys are those of the torrent's
// `info` field (poster, description, contentCategory, trackers[],
// rutrackerThreadId, nyaaThreadId, trackerName). A higher-level
// service listens for scraped() and persists those fields onto the torrent.
class TrackerSiteScraper : public QObject {
    Q_OBJECT

public:
    explicit TrackerSiteScraper(QObject* parent = nullptr);
    ~TrackerSiteScraper() override;

    // Scrape every supported tracker for `infoHash` (40-char hex). `name` is the
    // torrent name, carried for logging / context. Non-blocking: on success
    // scraped() is emitted later on this object's thread. Requests inside the
    // per-hash cooldown window are dropped. Whether scraping happens at all is
    // decided by the caller (TrackerService).
    void scrape(const QString& infoHash, const QString& name);

    // Stop scraping: reject further scrape() calls and abort every in-flight
    // network request so shutdown is not held up waiting on tracker websites.
    // Idempotent; must be called on this object's thread.
    void stop();

    static constexpr int kTimeoutMs = 20000; // 20 s per request
    static constexpr int kCooldownSecs = 3600; // 1 h per-hash cooldown

signals:
    // Emitted once all strategies for `infoHash` have finished AND at least one
    // tracker yielded data. `info` carries only the freshly scraped keys; the
    // listener is responsible for merging them into the stored torrent.
    void scraped(const QString& infoHash, const QJsonObject& info);

private:
    // Strategy launchers — one network round-trip family each.
    void scrapeRutracker(const QString& hash);
    void scrapeNyaa(const QString& hash);
    void scrapeNyaaViewPage(const QString& hash, const QString& viewUrl);

    // HTML parsers (faithful ports of the legacy regex parsing).
    TrackerSiteInfo parseRutrackerHtml(const QByteArray& rawData);
    TrackerSiteInfo parseNyaaSearchHtml(const QByteArray& rawData);
    TrackerSiteInfo parseNyaaViewHtml(const QByteArray& rawData);

    // Called by each strategy when it finishes; merges once all have reported.
    void onStrategyComplete(const QString& hash, const TrackerSiteInfo& info);
    void checkAllComplete(const QString& hash);

    // Strip HTML tags / decode entities into plain text.
    static QString stripHtml(const QString& html);

    // Decode Windows-1251 bytes to a QString. Kept verbatim: Qt6 without ICU has
    // no codec for this legacy encoding, which RuTracker still serves.
    static QString decodeWindows1251(const QByteArray& data);

    // Clamp a description to kMaxDescriptionLength, appending an ellipsis when
    // truncated. Factored out of the three legacy copy-paste sites.
    static QString truncateDescription(const QString& text);

    // Member-function pointer type for a parallel strategy. The strategy list
    // (kStrategies) is the single source of truth for how many results a scrape
    // waits on — the count is derived from its size, never hardcoded.
    using Strategy = void (TrackerSiteScraper::*)(const QString&);
    static const QVector<Strategy> kStrategies;

    // Named constants (no magic numbers in the logic below).
    static constexpr int kInfoHashHexLength = 40; // 20-byte hash as hex
    static constexpr int kEncodingSniffLength = 2000; // bytes scanned for charset
    static constexpr int kMaxDescriptionLength = 5000; // description clamp

    QNetworkAccessManager* networkManager_;

    // Set by stop(): rejects new scrapes while the app is shutting down.
    std::atomic<bool> stopping_{ false };

    // Per-hash cooldown bookkeeping.
    mutable QMutex recentChecksMutex_;
    QHash<QString, QDateTime> recentChecks_;

    // In-flight scrapes: accumulate per-strategy results until all report.
    struct PendingScrape {
        QString name;
        int pendingCount = 0;
        QVector<TrackerSiteInfo> results;
    };
    mutable QMutex pendingMutex_;
    QHash<QString, PendingScrape> pendingScrapes_;
};

} // namespace rats::net

#endif // RATS_NET_TRACKER_SITE_SCRAPER_H
