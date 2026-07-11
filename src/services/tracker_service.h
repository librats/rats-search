#ifndef RATS_SERVICE_TRACKER_SERVICE_H
#define RATS_SERVICE_TRACKER_SERVICE_H

#include "domain/torrent.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace rats::net {
class SwarmScraper;
class TrackerSiteScraper;
} // namespace rats::net

namespace rats::data {
class TorrentRepository;
}

namespace rats::service {

// Orchestrates the two tracker scrapers and persists their results. It listens
// for newly indexed torrents and, when enabled, kicks off a seeders/leechers
// scrape and a website-metadata (poster/description) scrape; results flow back
// through the scrapers' signals and are written to the repository. The scrapers
// themselves never touch the database — this is the only glue that does.
class TrackerService : public QObject {
    Q_OBJECT

public:
    TrackerService(net::SwarmScraper* swarmScraper, net::TrackerSiteScraper* siteScraper,
        data::TorrentRepository* repository, QObject* parent = nullptr);

    void setCountScrapingEnabled(bool enabled);
    void setInfoScrapingEnabled(bool enabled);

    // Stop forwarding scrapes and tear down the underlying scrapers. Called on
    // shutdown so no fresh tracker work is issued while the app is closing.
    void stop();

    // Explicit requests (also used by the API "tracker.check" method).
    void checkCounts(const QString& hash);
    void checkInfo(const QString& hash, const QString& name);

public slots:
    // Wire to IndexingService::torrentIndexed.
    void onTorrentIndexed(const domain::Torrent& torrent);

private slots:
    void onCountsScraped(const QString& hash, int seeders, int leechers, int completed);
    void onInfoScraped(const QString& hash, const QJsonObject& info);

private:
    net::SwarmScraper* swarmScraper_;
    net::TrackerSiteScraper* siteScraper_;
    data::TorrentRepository* repository_;
    bool countEnabled_ = false;
    bool infoEnabled_ = false;
};

} // namespace rats::service

#endif // RATS_SERVICE_TRACKER_SERVICE_H
