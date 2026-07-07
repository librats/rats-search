#include "services/tracker_service.h"

#include "data/torrent_repository.h"
#include "net/tracker_info_scraper.h"
#include "net/tracker_scraper.h"

namespace rats::service {

TrackerService::TrackerService(
    net::TrackerScraper* counts, net::TrackerInfoScraper* info, data::TorrentRepository* repository, QObject* parent)
    : QObject(parent), counts_(counts), info_(info), repository_(repository)
{
    connect(counts_, &net::TrackerScraper::scraped, this, &TrackerService::onCountsScraped);
    connect(info_, &net::TrackerInfoScraper::scraped, this, &TrackerService::onInfoScraped);
}

void TrackerService::setCountScrapingEnabled(bool enabled)
{
    countEnabled_ = enabled;
}

void TrackerService::setInfoScrapingEnabled(bool enabled)
{
    infoEnabled_ = enabled;
}

void TrackerService::checkCounts(const QString& hash)
{
    if (countEnabled_)
        counts_->requestScrape(hash);
}

void TrackerService::checkInfo(const QString& hash, const QString& name)
{
    if (infoEnabled_)
        info_->scrape(hash, name);
}

void TrackerService::onTorrentIndexed(const domain::Torrent& torrent)
{
    checkCounts(torrent.hash);
    checkInfo(torrent.hash, torrent.name);
}

void TrackerService::onCountsScraped(const QString& hash, int seeders, int leechers, int completed)
{
    repository_->updateTrackerCounts(hash, seeders, leechers, completed);
}

void TrackerService::onInfoScraped(const QString& hash, const QJsonObject& info)
{
    repository_->mergeInfo(hash, info);
}

} // namespace rats::service
