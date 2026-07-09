#include "data/torrent_repository.h"

#include "data/database.h"
#include "data/query.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace rats::data {

using domain::ContentCategory;
using domain::ContentType;
using domain::File;
using domain::SearchHit;
using domain::Torrent;

namespace {
const QString kTorrents = QStringLiteral("torrents");
const QString kFiles = QStringLiteral("files");
constexpr int kInfoNameMaxLength = 800;

// Split the "\n"-joined path/size blobs stored in the files table back into a
// file list.
QVector<File> parseFileBlob(const QString& pathBlob, const QString& sizeBlob)
{
    const QStringList paths = pathBlob.split(QLatin1Char('\n'));
    const QStringList sizes = sizeBlob.split(QLatin1Char('\n'));
    QVector<File> files;
    files.reserve(paths.size());
    for (int i = 0; i < paths.size(); ++i)
        files.append(File { paths.at(i), i < sizes.size() ? sizes.at(i).toLongLong() : 0 });
    return files;
}
} // namespace

TorrentRepository::TorrentRepository(Database* db, QObject* parent) : QObject(parent), db_(db) { }

void TorrentRepository::primeFromDatabase()
{
    nextTorrentId_ = db_->maxId(kTorrents) + 1;
    nextFilesId_ = db_->maxId(kFiles) + 1;

    const auto rows = db_->query(QStringLiteral("SELECT COUNT(*) AS cnt, SUM(files) AS numfiles, "
                                                "SUM(size) AS totalsize FROM torrents"));
    if (!rows.isEmpty()) {
        stats_.torrents = rows.first().value(QStringLiteral("cnt")).toLongLong();
        stats_.files = rows.first().value(QStringLiteral("numfiles")).toLongLong();
        stats_.totalSize = rows.first().value(QStringLiteral("totalsize")).toLongLong();
    }
    qInfo() << "[TorrentRepository] primed:" << stats_.torrents << "torrents," << stats_.files << "files";
}

// ---------------------------------------------------------------------------
// CRUD
// ---------------------------------------------------------------------------

bool TorrentRepository::add(const Torrent& t, bool skipExistsCheck)
{
    if (!t.isValid())
        return false;
    if (!skipExistsCheck && exists(t.hash)) {
        qInfo() << "[TorrentRepository] already present:" << t.hash;
        return true;
    }

    QVariantMap values;
    values["id"] = static_cast<qlonglong>(nextTorrentId_++);
    values["hash"] = t.hash;
    values["name"] = t.name;
    values["nameIndex"] = buildNameIndex(t);
    values["size"] = t.size;
    values["files"] = t.files;
    values["piecelength"] = t.pieceLength;
    values["added"] = t.added.isValid() ? t.added.toSecsSinceEpoch() : QDateTime::currentSecsSinceEpoch();
    values["ipv4"] = t.ipv4; // formatValue turns an empty/null string into ''
    values["port"] = t.port;
    values["contentType"] = domain::toId(t.contentType);
    values["contentCategory"] = domain::toId(t.contentCategory);
    values["seeders"] = t.seeders;
    values["leechers"] = t.leechers;
    values["completed"] = t.completed;
    values["trackersChecked"] = t.trackersChecked.isValid() ? t.trackersChecked.toSecsSinceEpoch() : 0;
    values["good"] = t.good;
    values["bad"] = t.bad;
    if (!t.info.isEmpty())
        values["info"] = t.info;

    if (!db_->insert(kTorrents, values))
        return false;

    if (!t.fileList.isEmpty())
        saveFiles(t.hash, t.fileList);

    stats_.torrents++;
    stats_.files += t.files;
    stats_.totalSize += t.size;
    emit statisticsChanged(stats_.torrents, stats_.files, stats_.totalSize);
    return true;
}

bool TorrentRepository::update(const Torrent& t)
{
    if (!t.isValid())
        return false;

    QVariantMap values;
    values["name"] = t.name;
    values["nameIndex"] = buildNameIndex(t); // keep the full-text column in sync with a renamed torrent
    values["size"] = t.size;
    values["files"] = t.files;
    values["contentType"] = domain::toId(t.contentType);
    values["contentCategory"] = domain::toId(t.contentCategory);
    values["seeders"] = t.seeders;
    values["leechers"] = t.leechers;
    values["completed"] = t.completed;
    values["good"] = t.good;
    values["bad"] = t.bad;
    if (!t.info.isEmpty())
        values["info"] = t.info;

    if (!db_->update(kTorrents, values, { { "hash", t.hash } }))
        return false;
    emit torrentUpdated(t.hash);
    return true;
}

bool TorrentRepository::remove(const QString& hash)
{
    qint64 removedSize = 0;
    int removedFiles = 0;
    const auto rows = db_->query(QStringLiteral("SELECT size, files FROM torrents WHERE hash = ?"), { hash });
    if (!rows.isEmpty()) {
        removedSize = rows.first().value(QStringLiteral("size")).toLongLong();
        removedFiles = rows.first().value(QStringLiteral("files")).toInt();
    }

    db_->remove(kTorrents, { { "hash", hash } });
    db_->remove(kFiles, { { "hash", hash } });

    if (removedSize > 0 || removedFiles > 0) {
        stats_.torrents = qMax(0LL, stats_.torrents - 1);
        stats_.files = qMax(0LL, stats_.files - removedFiles);
        stats_.totalSize = qMax(0LL, stats_.totalSize - removedSize);
        emit statisticsChanged(stats_.torrents, stats_.files, stats_.totalSize);
    }
    return true;
}

bool TorrentRepository::exists(const QString& hash)
{
    return !db_->query(QStringLiteral("SELECT id FROM torrents WHERE hash = ? LIMIT 1"), { hash }).isEmpty();
}

std::optional<Torrent> TorrentRepository::get(const QString& hash, bool includeFiles)
{
    const auto rows = db_->query(QStringLiteral("SELECT * FROM torrents WHERE hash = ?"), { hash });
    if (rows.isEmpty())
        return std::nullopt;

    Torrent t = rowToTorrent(rows.first());
    if (includeFiles)
        t.fileList = filesOf(hash);
    return t;
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

QString TorrentRepository::resolveSortColumn(const QString& key)
{
    // Whitelist: user sort keys -> real columns. Anything else -> no ORDER BY.
    static const QHash<QString, QString> allowed = {
        { QStringLiteral("seeders"), QStringLiteral("seeders") },
        { QStringLiteral("leechers"), QStringLiteral("leechers") },
        { QStringLiteral("size"), QStringLiteral("size") },
        { QStringLiteral("files"), QStringLiteral("files") },
        { QStringLiteral("added"), QStringLiteral("added") },
        { QStringLiteral("completed"), QStringLiteral("completed") },
    };
    return allowed.value(key.toLower());
}

QString TorrentRepository::contentTypeFilter(const QString& type)
{
    if (type.isEmpty())
        return QString();
    // "application" spans both Software and Games in the UI.
    if (type == QLatin1String("application")) {
        return QStringLiteral("contentType IN (%1, %2)")
            .arg(domain::toId(ContentType::Software))
            .arg(domain::toId(ContentType::Games));
    }
    return QStringLiteral("contentType = %1").arg(domain::toId(domain::contentTypeFromString(type)));
}

QVector<Torrent> TorrentRepository::selectTorrents(const QString& sql, const QVariantList& params)
{
    QVector<Torrent> out;
    for (const auto& row : db_->query(sql, params))
        out.append(rowToTorrent(row));
    return out;
}

QVector<SearchHit> TorrentRepository::searchTorrents(const SearchQuery& q)
{
    QVector<SearchHit> hits;
    if (q.text.isEmpty())
        return hits;

    SelectQuery builder(kTorrents);

    static const QRegularExpression hexRe(QStringLiteral("^[0-9a-fA-F]{40}$"));
    if (hexRe.match(q.text).hasMatch())
        builder.whereEq(QStringLiteral("hash"), q.text.toLower());
    else
        builder.matchAgainst(q.text);

    if (q.safeSearch)
        builder.whereRaw(QStringLiteral("contentCategory != %1").arg(domain::toId(ContentCategory::XXX)));
    builder.whereRaw(contentTypeFilter(q.contentType));
    if (q.sizeMin > 0)
        builder.whereRaw(QStringLiteral("size > %1").arg(q.sizeMin));
    if (q.sizeMax > 0)
        builder.whereRaw(QStringLiteral("size < %1").arg(q.sizeMax));
    if (q.filesMin > 0)
        builder.whereRaw(QStringLiteral("files > %1").arg(q.filesMin));
    if (q.filesMax > 0)
        builder.whereRaw(QStringLiteral("files < %1").arg(q.filesMax));

    const QString sortColumn = resolveSortColumn(q.sort);
    if (!sortColumn.isEmpty())
        builder.orderBy(sortColumn, q.descending);
    builder.limit(q.offset, q.limit);

    for (const auto& row : db_->query(builder.build())) {
        SearchHit hit;
        hit.torrent = rowToTorrent(row);
        hits.append(hit);
    }
    return hits;
}

QVector<SearchHit> TorrentRepository::searchFiles(const SearchQuery& q)
{
    QVector<SearchHit> hits;
    if (q.text.isEmpty())
        return hits;

    // SNIPPET highlights matching file paths; MATCH selects the rows.
    const QString sql = QStringLiteral("SELECT *, SNIPPET(path, ?, 'around=100', "
                                       "'force_all_words=1') AS snipplet FROM "
                                       "files WHERE MATCH(?) LIMIT ?,?");
    const auto fileRows = db_->query(sql, { q.text, sql::escapeMatch(q.text), q.offset, q.limit });
    if (fileRows.isEmpty())
        return hits;

    QHash<QString, QStringList> snippetsByHash;
    QStringList orderedHashes;
    for (const auto& row : fileRows) {
        const QString hash = row.value(QStringLiteral("hash")).toString();
        for (const QString& line : row.value(QStringLiteral("snipplet")).toString().split(QLatin1Char('\n'))) {
            if (line.contains(QLatin1String("<b>"))) {
                if (!snippetsByHash.contains(hash))
                    orderedHashes << hash;
                snippetsByHash[hash].append(line);
            }
        }
    }
    if (orderedHashes.isEmpty())
        return hits;

    const QString inSql = SelectQuery(kTorrents).whereIn(QStringLiteral("hash"), orderedHashes).build();
    for (const auto& row : db_->query(inSql)) {
        Torrent t = rowToTorrent(row);
        if (q.safeSearch && t.contentCategory == ContentCategory::XXX)
            continue;

        SearchHit hit;
        hit.torrent = t;
        hit.fromFileMatch = true;
        hit.matchingPaths = snippetsByHash.value(t.hash);
        // File-match hits carry only the matched paths, so give consumers a
        // lightweight fileList built from them (sizes are unknown here).
        for (const QString& path : hit.matchingPaths)
            hit.torrent.fileList.append(File { path, 0 });
        hits.append(hit);
    }

    if (const QString column = resolveSortColumn(q.sort); !column.isEmpty()) {
        std::sort(hits.begin(), hits.end(), [&](const SearchHit& a, const SearchHit& b) {
            if (column == QLatin1String("seeders"))
                return q.descending ? a.torrent.seeders > b.torrent.seeders : a.torrent.seeders < b.torrent.seeders;
            if (column == QLatin1String("size"))
                return q.descending ? a.torrent.size > b.torrent.size : a.torrent.size < b.torrent.size;
            return false;
        });
    }
    return hits;
}

QVector<Torrent> TorrentRepository::recent(int limit)
{
    return selectTorrents(QStringLiteral("SELECT * FROM torrents ORDER BY added DESC LIMIT 0,%1").arg(limit));
}

QVector<Torrent> TorrentRepository::top(const QString& type, const QString& time, int offset, int limit)
{
    SelectQuery builder(kTorrents);
    builder.whereRaw(QStringLiteral("seeders > 0"));
    builder.whereRaw(QStringLiteral("contentCategory != %1").arg(domain::toId(ContentCategory::XXX)));
    builder.whereRaw(contentTypeFilter(type));

    if (!time.isEmpty()) {
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        qint64 cutoff = 0;
        if (time == QLatin1String("hours"))
            cutoff = now - 60 * 60 * 24;
        else if (time == QLatin1String("week"))
            cutoff = now - 60 * 60 * 24 * 7;
        else if (time == QLatin1String("month"))
            cutoff = now - 60LL * 60 * 24 * 30;
        if (cutoff > 0)
            builder.whereRaw(QStringLiteral("added > %1").arg(cutoff));
    }

    builder.orderBy(QStringLiteral("seeders"), true);
    builder.limit(offset, limit);
    return selectTorrents(builder.build());
}

QVector<Torrent> TorrentRepository::random(int limit, bool includeFiles)
{
    // RAND() is not an identifier, so this one query is built directly.
    const QString sql = QStringLiteral("SELECT * FROM torrents WHERE seeders > 0 AND contentCategory != %1 "
                                       "ORDER BY RAND() LIMIT %2")
                            .arg(domain::toId(ContentCategory::XXX))
                            .arg(limit);
    QVector<Torrent> results = selectTorrents(sql);

    if (includeFiles && !results.isEmpty()) {
        QStringList hashes;
        for (const Torrent& t : results)
            hashes << t.hash;
        const auto filesMap = filesOf(hashes);
        for (Torrent& t : results)
            t.fileList = filesMap.value(t.hash);
    }
    return results;
}

QVector<Torrent> TorrentRepository::page(int offset, int limit)
{
    return selectTorrents(SelectQuery(kTorrents).orderBy(QStringLiteral("id"), false).limit(offset, limit).build());
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

QVector<File> TorrentRepository::filesOf(const QString& hash)
{
    const auto rows = db_->query(QStringLiteral("SELECT * FROM files WHERE hash = ?"), { hash });
    if (rows.isEmpty())
        return {};
    return parseFileBlob(
        rows.first().value(QStringLiteral("path")).toString(), rows.first().value(QStringLiteral("size")).toString());
}

QHash<QString, QVector<File>> TorrentRepository::filesOf(const QStringList& hashes)
{
    QHash<QString, QVector<File>> result;
    if (hashes.isEmpty())
        return result;

    const QString sql = SelectQuery(kFiles).whereIn(QStringLiteral("hash"), hashes).build();
    for (const auto& row : db_->query(sql)) {
        const QString hash = row.value(QStringLiteral("hash")).toString();
        result[hash]
            = parseFileBlob(row.value(QStringLiteral("path")).toString(), row.value(QStringLiteral("size")).toString());
    }
    return result;
}

void TorrentRepository::saveFiles(const QString& hash, const QVector<File>& files)
{
    if (files.isEmpty())
        return;

    db_->remove(kFiles, { { "hash", hash } });

    QStringList paths;
    QStringList sizes;
    paths.reserve(files.size());
    sizes.reserve(files.size());
    for (const File& f : files) {
        paths << f.path;
        sizes << QString::number(f.size);
    }

    QVariantMap values;
    values["id"] = static_cast<qlonglong>(nextFilesId_++);
    values["hash"] = hash;
    values["path"] = paths.join(QLatin1Char('\n'));
    values["size"] = sizes.join(QLatin1Char('\n'));
    db_->insert(kFiles, values);
}

// ---------------------------------------------------------------------------
// Partial updates
// ---------------------------------------------------------------------------

bool TorrentRepository::updateTrackerCounts(const QString& hash, int seeders, int leechers, int completed)
{
    return db_->update(kTorrents,
        { { "seeders", seeders }, { "leechers", leechers }, { "completed", completed },
            { "trackersChecked", QDateTime::currentSecsSinceEpoch() } },
        { { "hash", hash } });
}

bool TorrentRepository::mergeInfo(const QString& hash, const QJsonObject& info)
{
    const auto existing = get(hash);
    QJsonObject merged = existing ? existing->info : QJsonObject();
    for (auto it = info.constBegin(); it != info.constEnd(); ++it)
        merged[it.key()] = it.value();
    return db_->update(kTorrents, { { "info", merged } }, { { "hash", hash } });
}

bool TorrentRepository::updateClassification(const QString& hash, ContentType type, ContentCategory category)
{
    return db_->update(kTorrents,
        { { "contentType", domain::toId(type) }, { "contentCategory", domain::toId(category) } }, { { "hash", hash } });
}

// ---------------------------------------------------------------------------
// Mapping helpers
// ---------------------------------------------------------------------------

Torrent TorrentRepository::rowToTorrent(const QVariantMap& row) const
{
    // Manticore returns every column name lower-cased.
    Torrent t;
    t.id = row.value(QStringLiteral("id")).toLongLong();
    t.hash = row.value(QStringLiteral("hash")).toString();
    t.name = row.value(QStringLiteral("name")).toString();
    t.size = row.value(QStringLiteral("size")).toLongLong();
    t.files = row.value(QStringLiteral("files")).toInt();
    t.pieceLength = row.value(QStringLiteral("piecelength")).toInt();
    t.added = QDateTime::fromSecsSinceEpoch(row.value(QStringLiteral("added")).toLongLong());
    t.ipv4 = row.value(QStringLiteral("ipv4")).toString();
    t.port = row.value(QStringLiteral("port")).toInt();
    t.contentType = domain::contentTypeFromId(row.value(QStringLiteral("contenttype")).toInt());
    t.contentCategory = domain::contentCategoryFromId(row.value(QStringLiteral("contentcategory")).toInt());
    t.seeders = row.value(QStringLiteral("seeders")).toInt();
    t.leechers = row.value(QStringLiteral("leechers")).toInt();
    t.completed = row.value(QStringLiteral("completed")).toInt();

    const qint64 checked = row.value(QStringLiteral("trackerschecked")).toLongLong();
    if (checked > 0)
        t.trackersChecked = QDateTime::fromSecsSinceEpoch(checked);

    t.good = row.value(QStringLiteral("good")).toInt();
    t.bad = row.value(QStringLiteral("bad")).toInt();

    const QString infoStr = row.value(QStringLiteral("info")).toString();
    if (!infoStr.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(infoStr.toUtf8());
        if (doc.isObject())
            t.info = doc.object();
    }
    return t;
}

QString TorrentRepository::buildNameIndex(const Torrent& t) const
{
    QString index = t.name;
    if (t.info.contains(QLatin1String("name"))) {
        const QString infoName = t.info.value(QLatin1String("name")).toString();
        if (!infoName.isEmpty() && infoName.length() < kInfoNameMaxLength)
            index += QLatin1Char(' ') + infoName;
    }
    return index;
}

} // namespace rats::data
