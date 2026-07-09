#ifndef RATS_DATA_TORRENT_REPOSITORY_H
#define RATS_DATA_TORRENT_REPOSITORY_H

#include "domain/torrent.h"

#include <QHash>
#include <QObject>
#include <QVector>
#include <atomic>
#include <optional>

namespace rats::data {

class Database;

// Repository for the `torrents` and `files` Manticore tables. Owns the mapping
// between rows and domain::Torrent, all search queries, and the incrementally
// maintained statistics. Process lifecycle lives in Manticore and raw SQL in
// Database/SelectQuery.
class TorrentRepository : public QObject {
    Q_OBJECT

public:
    struct SearchQuery {
        QString text;
        int offset = 0;
        int limit = 10;
        QString sort; // user sort key ("seeders", "size", "added"); mapped to a
                      // safe column
        bool descending = true;
        bool safeSearch = false;
        QString contentType; // "video".."archive", or "application" (Software+Games)
        qint64 sizeMin = 0;
        qint64 sizeMax = 0;
        int filesMin = 0;
        int filesMax = 0;
    };

    struct Statistics {
        qint64 torrents = 0;
        qint64 files = 0;
        qint64 totalSize = 0;
    };

    explicit TorrentRepository(Database* db, QObject* parent = nullptr);

    // Load id counters and statistics from the existing tables. Call once after
    // the database is up.
    void primeFromDatabase();

    // CRUD ---------------------------------------------------------------------
    // Pass skipExistsCheck when the caller has just proven the hash is absent
    // (e.g. IndexingService's dedup get()), to avoid a redundant existence query.
    bool add(const domain::Torrent& torrent, bool skipExistsCheck = false);
    bool update(const domain::Torrent& torrent);
    bool remove(const QString& hash);
    bool exists(const QString& hash);
    std::optional<domain::Torrent> get(const QString& hash, bool includeFiles = false);

    // Search -------------------------------------------------------------------
    QVector<domain::SearchHit> searchTorrents(const SearchQuery& query);
    QVector<domain::SearchHit> searchFiles(const SearchQuery& query);
    QVector<domain::Torrent> recent(int limit = 10);
    QVector<domain::Torrent> top(const QString& type, const QString& time, int offset, int limit);
    QVector<domain::Torrent> random(int limit = 5, bool includeFiles = false);
    // A page of all torrents ordered by id — used by maintenance sweeps.
    QVector<domain::Torrent> page(int offset, int limit);

    // Files --------------------------------------------------------------------
    QVector<domain::File> filesOf(const QString& hash);
    QHash<QString, QVector<domain::File>> filesOf(const QStringList& hashes);

    // Partial updates ----------------------------------------------------------
    bool updateTrackerCounts(const QString& hash, int seeders, int leechers, int completed);
    bool mergeInfo(const QString& hash, const QJsonObject& info);
    bool updateClassification(const QString& hash, domain::ContentType type, domain::ContentCategory category);

    Statistics statistics() const { return stats_; }

signals:
    void torrentUpdated(const QString& hash);
    void statisticsChanged(qint64 torrents, qint64 files, qint64 totalSize);

private:
    domain::Torrent rowToTorrent(const QVariantMap& row) const;
    QString buildNameIndex(const domain::Torrent& torrent) const;
    void saveFiles(const QString& hash, const QVector<domain::File>& files);
    QVector<domain::Torrent> selectTorrents(const QString& sql, const QVariantList& params = {});

    // Map a user-supplied sort key to a whitelisted column, or empty if unknown.
    static QString resolveSortColumn(const QString& key);
    // Build the "contentType = N" / "contentType IN (5,6)" fragment for a filter.
    static QString contentTypeFilter(const QString& type);

    Database* db_;
    std::atomic<qint64> nextTorrentId_ { 1 };
    std::atomic<qint64> nextFilesId_ { 1 };
    Statistics stats_;
};

} // namespace rats::data

#endif // RATS_DATA_TORRENT_REPOSITORY_H
