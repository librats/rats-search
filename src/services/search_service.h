#ifndef RATS_SERVICE_SEARCH_SERVICE_H
#define RATS_SERVICE_SEARCH_SERVICE_H

#include "data/torrent_repository.h"
#include "domain/torrent.h"

#include <QObject>
#include <QVector>
#include <optional>

namespace rats::service {

// Facade over local search. It owns the search *policy* (how a query maps to a
// repository query, safe-search defaults, torrent-vs-file search) so the API
// and P2P layers share one implementation instead of each building repository
// queries by hand. Cross-source fallback (DHT metadata lookup, remote peers) is
// async source-combining and lives in the API orchestration layer, not here.
class SearchService : public QObject {
    Q_OBJECT

public:
    // Request shape shared by the front-ends; translated to a repository query.
    struct Request {
        QString query;
        int offset = 0;
        int limit = 10;
        QString sort;
        bool descending = true;
        bool safeSearch = false;
        QString contentType;
        qint64 sizeMin = 0;
        qint64 sizeMax = 0;
        int filesMin = 0;
        int filesMax = 0;
    };

    explicit SearchService(data::TorrentRepository* repository, QObject* parent = nullptr);

    QVector<domain::SearchHit> searchTorrents(const Request& request);
    QVector<domain::SearchHit> searchFiles(const Request& request);

    QVector<domain::Torrent> recent(int limit = 10);
    QVector<domain::Torrent> top(const QString& type, const QString& time, int offset, int limit);
    std::optional<domain::Torrent> get(const QString& hash, bool includeFiles = true);

    // Extracts a normalized (lower-case) 40-hex info-hash from a search query,
    // accepting both a bare hash and a full "magnet:?xt=urn:btih:..." link.
    // Returns an empty string if the query carries no info-hash. This is the
    // single place the search path recognizes "this query is a torrent hash", so
    // a magnet link resolves via the DHT fallback exactly like a bare hash.
    static QString extractInfoHash(const QString& query);

private:
    static data::TorrentRepository::SearchQuery toRepositoryQuery(const Request& request);

    data::TorrentRepository* repository_;
};

} // namespace rats::service

#endif // RATS_SERVICE_SEARCH_SERVICE_H
