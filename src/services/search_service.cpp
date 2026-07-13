#include "services/search_service.h"

#include "common/infohash.h"
#include "net/torrent_engine.h"

namespace rats::service {

SearchService::SearchService(data::TorrentRepository* repository, QObject* parent)
    : QObject(parent), repository_(repository)
{
}

QString SearchService::extractInfoHash(const QString& query)
{
    // parseInfoHash accepts a bare hex hash or a magnet link and returns an empty
    // string when the query carries no hash (base32 hashes are not decoded).
    // Normalize so callers can match it against the lower-case hashes in the index.
    const QString hash = net::TorrentEngine::parseInfoHash(query.trimmed());
    return infohash::isValid(hash) ? infohash::normalize(hash) : QString();
}

data::TorrentRepository::SearchQuery SearchService::toRepositoryQuery(const Request& r)
{
    data::TorrentRepository::SearchQuery q;
    q.text = r.query;
    q.offset = r.offset;
    q.limit = r.limit;
    q.sort = r.sort;
    q.descending = r.descending;
    q.safeSearch = r.safeSearch;
    q.contentType = r.contentType;
    q.sizeMin = r.sizeMin;
    q.sizeMax = r.sizeMax;
    q.filesMin = r.filesMin;
    q.filesMax = r.filesMax;
    return q;
}

QVector<domain::SearchHit> SearchService::searchTorrents(const Request& request)
{
    return repository_->searchTorrents(toRepositoryQuery(request));
}

QVector<domain::SearchHit> SearchService::searchFiles(const Request& request)
{
    return repository_->searchFiles(toRepositoryQuery(request));
}

QVector<domain::Torrent> SearchService::recent(int limit)
{
    return repository_->recent(limit);
}

QVector<domain::Torrent> SearchService::top(const QString& type, const QString& time, int offset, int limit)
{
    return repository_->top(type, time, offset, limit);
}

std::optional<domain::Torrent> SearchService::get(const QString& hash, bool includeFiles)
{
    return repository_->get(hash, includeFiles);
}

} // namespace rats::service
