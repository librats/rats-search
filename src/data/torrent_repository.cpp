#include "data/torrent_repository.h"

#include "data/database.h"
#include "data/query.h"
#include "data/row_id.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
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

// Hashes reach us from the DHT, from peers and from dumps, and not all of those
// agree on case. Row ids already ignore it (rowIdFromHash parses hex either
// way), so the verification that guards against id collisions must too.
bool sameHash(const QString& a, const QString& b)
{
    return !a.isEmpty() && a.compare(b, Qt::CaseInsensitive) == 0;
}

// Whether a row that came back under some id belongs to one of the hashes that
// asked for it. Distinct hashes can share an id, so this is a search over the
// requesters rather than a comparison against a single one.
bool wantedMatches(const QStringList& wanted, const QString& hash)
{
    for (const QString& w : wanted) {
        if (sameHash(w, hash))
            return true;
    }
    return false;
}

// Resolve a batch of hashes to row ids, keeping the reverse map so each returned
// row can be checked against the hashes that asked for it.
//
// The map holds a list per id, not a single hash: two different torrents whose
// hashes agree on their first 63 bits ask for the same row, and keeping only the
// first left the other one invisible — neither returned as found nor reported as
// collided, which let a batch write REPLACE a stranger's row.
QVector<qint64> idsFor(const QStringList& hashes, QHash<qint64, QStringList>& wantedById)
{
    QVector<qint64> ids;
    ids.reserve(hashes.size());
    for (const QString& hash : hashes) {
        const qint64 id = rowIdFromHash(hash);
        if (id == 0)
            continue;
        QStringList& wanted = wantedById[id];
        // Ask the database for the id once, however many hashes want it.
        if (wanted.isEmpty())
            ids.append(id);
        // Two spellings of one hash are one request; two hashes that merely
        // collide are two, and both need an answer.
        if (!wantedMatches(wanted, hash))
            wanted.append(hash);
    }
    return ids;
}

// Size / file-count range filters, shared by the torrent search and the parent
// lookup of the file search so both lanes answer one request the same way.
//
// Bounds are inclusive: "up to 1 GB" has to keep a torrent of exactly 1 GB, and
// a "from 1 file" filter must not drop every single-file torrent. 0 means the
// bound is unset — a torrent of size 0 is nothing anyone filters *for*.
void applyRangeFilters(SelectQuery& builder, const TorrentRepository::SearchQuery& q)
{
    if (q.sizeMin > 0)
        builder.whereRaw(QStringLiteral("size >= %1").arg(q.sizeMin));
    if (q.sizeMax > 0)
        builder.whereRaw(QStringLiteral("size <= %1").arg(q.sizeMax));
    if (q.filesMin > 0)
        builder.whereRaw(QStringLiteral("files >= %1").arg(q.filesMin));
    if (q.filesMax > 0)
        builder.whereRaw(QStringLiteral("files <= %1").arg(q.filesMax));
}
} // namespace

TorrentRepository::TorrentRepository(Database* db, QObject* parent) : QObject(parent), db_(db) { }

TorrentRepository::Statistics TorrentRepository::statistics() const
{
    QMutexLocker lock(&statsMutex_);
    return stats_;
}

void TorrentRepository::bumpStatistics(qint64 torrents, qint64 files, qint64 totalSize)
{
    Statistics now;
    {
        QMutexLocker lock(&statsMutex_);
        // Clamped, not asserted: a delta is derived from what a write reported, and
        // a row that vanished underneath us would otherwise drive a counter
        // negative and keep it there for the life of the process.
        stats_.torrents = qMax(0LL, stats_.torrents + torrents);
        stats_.files = qMax(0LL, stats_.files + files);
        stats_.totalSize = qMax(0LL, stats_.totalSize + totalSize);
        now = stats_;
    }
    emit statisticsChanged(now.torrents, now.files, now.totalSize);
}

void TorrentRepository::primeFromDatabase()
{
    const auto rows = db_->query(QStringLiteral("SELECT COUNT(*) AS cnt, SUM(files) AS numfiles, "
                                                "SUM(size) AS totalsize FROM torrents"));
    if (!rows.isEmpty()) {
        QMutexLocker lock(&statsMutex_);
        stats_.torrents = rows.first().value(QStringLiteral("cnt")).toLongLong();
        stats_.files = rows.first().value(QStringLiteral("numfiles")).toLongLong();
        stats_.totalSize = rows.first().value(QStringLiteral("totalsize")).toLongLong();
    }
    const Statistics primed = statistics();
    qInfo() << "[TorrentRepository] primed:" << primed.torrents << "torrents," << primed.files << "files";
    emit statisticsChanged(primed.torrents, primed.files, primed.totalSize);
}

// ---------------------------------------------------------------------------
// CRUD
// ---------------------------------------------------------------------------

TorrentRepository::RowSlot TorrentRepository::resolve(const QString& hash, bool withRow)
{
    RowSlot slot;
    slot.id = rowIdFromHash(hash);
    if (slot.id == 0)
        return slot;

    // One docid lookup answers both questions a write has: is this torrent
    // already stored, and is its slot claimed by a different one? `withRow`
    // widens it to the whole row for callers that would otherwise follow up with
    // a get() on the same id — the crawler's insert path asks every torrent this
    // question, so the columns are only paid for when someone needs them.
    const QString columns = withRow ? QStringLiteral("*") : QStringLiteral("hash");
    const auto rows
        = db_->query(QStringLiteral("SELECT %1 FROM torrents WHERE id = ? LIMIT 1").arg(columns), { slot.id });
    if (rows.isEmpty())
        return slot;

    slot.taken = true;
    if (!withRow) {
        slot.ours = sameHash(rows.first().value(QStringLiteral("hash")).toString(), hash);
        return slot;
    }

    Torrent stored = rowToTorrent(rows.first());
    slot.ours = sameHash(stored.hash, hash);
    if (slot.ours)
        slot.stored = std::move(stored);
    return slot;
}

QVariantMap TorrentRepository::torrentRow(const Torrent& t)
{
    QVariantMap values;
    values["id"] = static_cast<qlonglong>(rowIdFromHash(t.hash));
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
    // A multi-row write takes its column list from the first row, so every row
    // must carry the same keys — "info" is always present, empty object or not.
    values["info"] = t.info;
    return values;
}

QVariantMap TorrentRepository::filesRow(const QString& hash, const QVector<File>& files)
{
    QStringList paths;
    QStringList sizes;
    paths.reserve(files.size());
    sizes.reserve(files.size());
    for (const File& f : files) {
        paths << f.path;
        sizes << QString::number(f.size);
    }

    QVariantMap values;
    // Deliberately the same id as the torrent's row: it turns the export's
    // "files of these 500 torrents" join into the same docid lookup, and makes
    // replacing a file list a single REPLACE instead of a delete + insert that
    // would hand the row a fresh id.
    values["id"] = static_cast<qlonglong>(rowIdFromHash(hash));
    values["hash"] = hash;
    values["path"] = paths.join(QLatin1Char('\n'));
    values["size"] = sizes.join(QLatin1Char('\n'));
    return values;
}

bool TorrentRepository::add(const Torrent& t)
{
    if (!t.isValid())
        return false;
    return add(t, resolve(t.hash));
}

bool TorrentRepository::add(const Torrent& t, const RowSlot& slot)
{
    if (!t.isValid())
        return false;

    // A slot resolved for a different hash would send the write to a stranger's
    // row, so it is refused rather than re-resolved: a caller passing the wrong
    // one has a bug, and silently papering over it would hide it.
    if (slot.id != rowIdFromHash(t.hash)) {
        qWarning() << "[TorrentRepository] slot does not belong to" << t.hash << "- refusing to store";
        return false;
    }
    if (slot.id == 0)
        return false;
    if (slot.collided()) {
        qWarning() << "[TorrentRepository] row id collision, refusing to store" << t.hash;
        return false;
    }
    if (slot.taken) {
        qInfo() << "[TorrentRepository] already present:" << t.hash;
        return true;
    }

    QVariantMap values = torrentRow(t);
    if (t.info.isEmpty())
        values.remove("info");

    // REPLACE rather than INSERT: the id is derived, and Manticore rejects an
    // INSERT onto an id that already exists. The slot check above proved it is
    // free, so this only guards against a concurrent writer claiming it first.
    if (!db_->replace(kTorrents, values))
        return false;

    if (!t.fileList.isEmpty())
        saveFiles(t.hash, t.fileList);

    bumpStatistics(1, t.files, t.size);
    return true;
}

bool TorrentRepository::update(const Torrent& t)
{
    if (!t.isValid())
        return false;

    // Rewrite the whole row rather than SET the changed columns. Manticore
    // refuses to UPDATE a full-text field, and refuses the *entire* statement
    // when one is named — "attribute 'nameindex' can not be updated (full-text
    // field)" — so the previous partial UPDATE, which had to name nameIndex to
    // keep it in step with a renamed torrent, never applied anything at all: vote
    // merges and rename were both silently dropped. A derived row id makes the
    // honest fix trivial, since REPLACE addresses exactly this torrent's row.
    //
    // Every caller passes a Torrent it just read back with get(), so the row
    // rebuilt here is complete rather than a partial overwrite.
    const RowSlot slot = resolve(t.hash);
    if (slot.id == 0)
        return false;
    if (slot.collided()) {
        qWarning() << "[TorrentRepository] row id collision, refusing to update" << t.hash;
        return false;
    }

    QVariantMap values = torrentRow(t);
    if (t.info.isEmpty())
        values.remove("info");

    if (!db_->replace(kTorrents, values))
        return false;
    emit torrentUpdated(t.hash);
    return true;
}

bool TorrentRepository::remove(const QString& hash)
{
    qint64 removedSize = 0;
    int removedFiles = 0;
    const qint64 id = rowIdFromHash(hash);
    if (id == 0)
        return false;

    // Verify the slot really holds this torrent before deleting it: on a 63-bit
    // id collision the row belongs to somebody else.
    const auto rows = db_->query(QStringLiteral("SELECT hash, size, files FROM torrents WHERE id = ?"), { id });
    if (!rows.isEmpty()) {
        if (!sameHash(rows.first().value(QStringLiteral("hash")).toString(), hash))
            return false;
        removedSize = rows.first().value(QStringLiteral("size")).toLongLong();
        removedFiles = rows.first().value(QStringLiteral("files")).toInt();
    }

    db_->remove(kTorrents, { { "id", id } });
    db_->remove(kFiles, { { "id", id } });

    if (removedSize > 0 || removedFiles > 0) {
        bumpStatistics(-1, -removedFiles, -removedSize);
    }
    return true;
}

bool TorrentRepository::exists(const QString& hash)
{
    const RowSlot slot = resolve(hash);
    // A collided slot is "not present": the torrent is absent, it simply cannot
    // be stored either.
    return slot.taken && slot.ours;
}

std::optional<Torrent> TorrentRepository::get(const QString& hash, bool includeFiles)
{
    const qint64 id = rowIdFromHash(hash);
    if (id == 0)
        return std::nullopt;

    const auto rows = db_->query(QStringLiteral("SELECT * FROM torrents WHERE id = ?"), { id });
    if (rows.isEmpty())
        return std::nullopt;

    Torrent t = rowToTorrent(rows.first());
    if (!sameHash(t.hash, hash))
        return std::nullopt; // id collision: this row is a different torrent
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
        { QStringLiteral("name"), QStringLiteral("name") },
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
    // If the incoming SQL uses SELECT *, rewrite to exclude 'info' to avoid
    // blowing up the MySQL wire protocol buffer.
    QString safeSql = sql;
    if (safeSql.startsWith(QStringLiteral("SELECT * "))) {
        static const QString kCols = QStringLiteral(
            "SELECT id, hash, name, size, files, piecelength, added, ipv4, port, "
            "contentType, contentCategory, seeders, leechers, completed, "
            "trackersChecked, good, bad ");
        safeSql = kCols + safeSql.mid(9); // skip "SELECT * "
    }
    for (const auto& row : db_->query(safeSql, params))
        out.append(rowToTorrent(row));
    return out;
}

QVector<SearchHit> TorrentRepository::searchTorrents(const SearchQuery& q)
{
    QVector<SearchHit> hits;
    if (q.text.isEmpty())
        return hits;

    SelectQuery builder(kTorrents);
    // Exclude the large 'info' JSON column from search results — it blows up
    // the MySQL wire protocol buffer on large result sets.
    builder.columns(QStringLiteral(
        "id, hash, name, size, files, piecelength, added, ipv4, port, "
        "contentType, contentCategory, seeders, leechers, completed, "
        "trackersChecked, good, bad"));

    static const QRegularExpression hexRe(QStringLiteral("^[0-9a-fA-F]{40}$"));
    const bool byHash = hexRe.match(q.text).hasMatch();
    if (byHash)
        builder.whereEq(QStringLiteral("id"), rowIdFromHash(q.text));
    else
        builder.matchAgainst(q.text);

    if (q.safeSearch)
        builder.whereRaw(QStringLiteral("contentCategory != %1").arg(domain::toId(ContentCategory::XXX)));
    builder.whereRaw(contentTypeFilter(q.contentType));
    applyRangeFilters(builder, q);

    const QString sortColumn = resolveSortColumn(q.sort);
    if (!sortColumn.isEmpty())
        builder.orderBy(sortColumn, q.descending);
    builder.limit(q.offset, q.limit);

    for (const auto& row : db_->query(builder.build())) {
        SearchHit hit;
        hit.torrent = rowToTorrent(row);
        // An id lookup can land on a different torrent that shares the slot;
        // a hash search must not answer with it.
        if (byHash && !sameHash(hit.torrent.hash, q.text))
            continue;
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
                                       "'force_all_words=1') AS snippet FROM "
                                       "files WHERE MATCH(?) LIMIT ?,?");
    const auto fileRows = db_->query(sql, { q.text, sql::escapeMatch(q.text), q.offset, q.limit });
    if (fileRows.isEmpty())
        return hits;

    QHash<QString, QStringList> snippetsByHash;
    QStringList orderedHashes;
    for (const auto& row : fileRows) {
        const QString hash = row.value(QStringLiteral("hash")).toString();
        for (const QString& line : row.value(QStringLiteral("snippet")).toString().split(QLatin1Char('\n'))) {
            if (line.contains(QLatin1String("<b>"))) {
                if (!snippetsByHash.contains(hash))
                    orderedHashes << hash;
                snippetsByHash[hash].append(line);
            }
        }
    }
    if (orderedHashes.isEmpty())
        return hits;

    // The content-type filter applies to the parent torrent, so it belongs on
    // this lookup rather than on the file-path MATCH above.
    QHash<qint64, QStringList> wantedParents;
    const QVector<qint64> parentIds = idsFor(orderedHashes, wantedParents);
    SelectQuery parents(kTorrents);
    parents.whereInIds(QStringLiteral("id"), parentIds);
    parents.whereRaw(contentTypeFilter(q.contentType));
    applyRangeFilters(parents, q);
    for (const auto& row : db_->query(parents.build())) {
        Torrent t = rowToTorrent(row);
        if (!wantedMatches(wantedParents.value(t.id), t.hash))
            continue; // id collision
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
    builder.columns(QStringLiteral(
        "id, hash, name, size, files, piecelength, added, ipv4, port, "
        "contentType, contentCategory, seeders, leechers, completed, "
        "trackersChecked, good, bad"));
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

QHash<QString, Torrent> TorrentRepository::getMany(const QStringList& hashes, QSet<QString>* collided)
{
    QHash<QString, Torrent> result;
    if (hashes.isEmpty())
        return result;

    QHash<qint64, QStringList> wanted;
    const QVector<qint64> ids = idsFor(hashes, wanted);
    const QString sql = SelectQuery(kTorrents).whereInIds(QStringLiteral("id"), ids).limit(0, ids.size()).build();
    for (const Torrent& t : selectTorrents(sql)) {
        // One row answers every hash that asked for its id: the one it actually
        // belongs to is found, and each of the others lost the slot and must be
        // reported so the caller drops it instead of writing over this row.
        for (const QString& want : wanted.value(t.id)) {
            if (sameHash(t.hash, want))
                result.insert(t.hash, t);
            else if (collided)
                *collided += want;
        }
    }
    return result;
}

int TorrentRepository::addMany(const QVector<Torrent>& torrents)
{
    if (torrents.isEmpty())
        return 0;

    QVector<QVariantMap> torrentRows;
    QVector<QVariantMap> fileRows;
    torrentRows.reserve(torrents.size());
    qint64 addedFiles = 0;
    qint64 addedSize = 0;
    // Which hash already claimed each row id in this batch. Callers dedupe against
    // the *index* before getting here (insertBatch does it with one getMany), but
    // nothing dedupes the batch against itself, and two rows sharing an id inside
    // one REPLACE are resolved by Manticore keeping whichever it applies last.
    QHash<qint64, QString> claimedBy;
    claimedBy.reserve(torrents.size());

    for (const Torrent& t : torrents) {
        if (!t.isValid())
            continue;
        // The same refusal add() makes, and for the same reason: a hash that
        // derives to id 0 is one Manticore auto-assigns an id for, producing a
        // row no lookup by hash can reach while it still counts towards the
        // statistics and is re-inserted on every import. Validation upstream
        // should already have caught it — this is the guard on the write itself,
        // which is where the invariant has to hold.
        const qint64 id = rowIdFromHash(t.hash);
        if (id == 0) {
            qWarning() << "[TorrentRepository] unusable hash, refusing to store" << t.hash;
            continue;
        }
        // One id may leave this loop once. Both cases that reach here are silent
        // corruption otherwise: the batch write has no per-row outcome, so the
        // loser is neither stored nor reported, yet the statistics below count it.
        const auto claimed = claimedBy.constFind(id);
        if (claimed != claimedBy.constEnd()) {
            // A hash repeated inside one dump batch is ordinary — same torrent,
            // same row, nothing lost by writing it once.
            if (!sameHash(*claimed, t.hash)) {
                // A genuine 63-bit collision between two torrents that are both new
                // to the index. Neither add() nor the caller's getMany() can catch
                // this one: there is no stored row yet for either to compare against.
                qWarning() << "[TorrentRepository] row id collision inside batch, refusing to store" << t.hash << "- id"
                           << id << "already claimed by" << *claimed;
            }
            continue;
        }
        claimedBy.insert(id, t.hash);

        torrentRows.append(torrentRow(t));
        if (!t.fileList.isEmpty())
            fileRows.append(filesRow(t.hash, t.fileList));
        addedFiles += t.files;
        addedSize += t.size;
    }
    if (torrentRows.isEmpty())
        return 0;

    // REPLACE rather than INSERT: Manticore fails an entire multi-row INSERT on a
    // single duplicate id, and an id already held in the index by *this* torrent
    // (a re-import, a retried batch) is a duplicate. The loop above is what keeps
    // a batch from carrying two rows with one id into this statement.
    if (!db_->replaceMany(kTorrents, torrentRows))
        return 0;
    if (!fileRows.isEmpty())
        db_->replaceMany(kFiles, fileRows);

    bumpStatistics(torrentRows.size(), addedFiles, addedSize);
    return torrentRows.size();
}

QVector<Torrent> TorrentRepository::pageAfterId(qint64 afterId, int limit)
{
    SelectQuery builder(kTorrents);
    if (afterId > 0)
        builder.whereRaw(QStringLiteral("id > %1").arg(afterId));
    builder.orderBy(QStringLiteral("id"), false).limit(0, limit);
    return selectTorrents(builder.build());
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

QVector<File> TorrentRepository::filesOf(const QString& hash)
{
    const qint64 id = rowIdFromHash(hash);
    if (id == 0)
        return {};

    const auto rows = db_->query(QStringLiteral("SELECT * FROM files WHERE id = ?"), { id });
    if (rows.isEmpty())
        return {};
    if (!sameHash(rows.first().value(QStringLiteral("hash")).toString(), hash))
        return {}; // id collision: these files belong to a different torrent
    return parseFileBlob(
        rows.first().value(QStringLiteral("path")).toString(), rows.first().value(QStringLiteral("size")).toString());
}

QHash<QString, QVector<File>> TorrentRepository::filesOf(const QStringList& hashes)
{
    QHash<QString, QVector<File>> result;
    if (hashes.isEmpty())
        return result;

    QHash<qint64, QStringList> wanted;
    const QVector<qint64> ids = idsFor(hashes, wanted);
    // The LIMIT is not optional: Manticore returns 20 rows for a SELECT without
    // one, which would silently drop most of the batch.
    const QString sql = SelectQuery(kFiles).whereInIds(QStringLiteral("id"), ids).limit(0, ids.size()).build();
    for (const auto& row : db_->query(sql)) {
        const QString hash = row.value(QStringLiteral("hash")).toString();
        if (!wantedMatches(wanted.value(row.value(QStringLiteral("id")).toLongLong()), hash))
            continue; // id collision
        result[hash]
            = parseFileBlob(row.value(QStringLiteral("path")).toString(), row.value(QStringLiteral("size")).toString());
    }
    return result;
}

void TorrentRepository::saveFiles(const QString& hash, const QVector<File>& files)
{
    if (files.isEmpty())
        return;

    // One statement: the files row carries the torrent's own id, so replacing it
    // in place is exact. The old delete + insert handed the row a fresh counter
    // id every time a file list was backfilled.
    db_->replace(kFiles, filesRow(hash, files));
}

// ---------------------------------------------------------------------------
// Partial updates
// ---------------------------------------------------------------------------

bool TorrentRepository::updateTrackerCounts(const QString& hash, int seeders, int leechers, int completed)
{
    const RowSlot slot = resolve(hash);
    if (!slot.taken || !slot.ours)
        return false;
    return db_->update(kTorrents,
        { { "seeders", seeders }, { "leechers", leechers }, { "completed", completed },
            { "trackersChecked", QDateTime::currentSecsSinceEpoch() } },
        { { "id", slot.id } });
}

bool TorrentRepository::updateFiles(const QString& hash, const QVector<File>& files)
{
    if (files.isEmpty())
        return false;

    const qint64 id = rowIdFromHash(hash);
    if (id == 0)
        return false;

    const auto rows = db_->query(QStringLiteral("SELECT hash, files FROM torrents WHERE id = ?"), { id });
    if (rows.isEmpty())
        return false;
    if (!sameHash(rows.first().value(QStringLiteral("hash")).toString(), hash))
        return false; // id collision
    const int oldCount = rows.first().value(QStringLiteral("files")).toInt();

    saveFiles(hash, files); // replaces the file row for this hash

    if (!db_->update(kTorrents, { { "files", files.size() } }, { { "id", id } }))
        return false;

    bumpStatistics(0, files.size() - oldCount, 0);
    emit torrentUpdated(hash);
    return true;
}

bool TorrentRepository::mergeInfo(const QString& hash, const QJsonObject& info)
{
    // get() already refuses a collided row, so reaching here means the slot is
    // ours to write.
    const auto existing = get(hash);
    if (!existing)
        return false;

    QJsonObject merged = existing->info;
    for (auto it = info.constBegin(); it != info.constEnd(); ++it)
        merged[it.key()] = it.value();
    return db_->update(kTorrents, { { "info", merged } }, { { "id", existing->id } });
}

bool TorrentRepository::updateClassification(const QString& hash, ContentType type, ContentCategory category)
{
    const RowSlot slot = resolve(hash);
    if (!slot.taken || !slot.ours)
        return false;
    return db_->update(kTorrents,
        { { "contentType", domain::toId(type) }, { "contentCategory", domain::toId(category) } },
        { { "id", slot.id } });
}

// ---------------------------------------------------------------------------
// Row-id migration
// ---------------------------------------------------------------------------

TorrentRepository::RowIdMigrationPage TorrentRepository::migrateRowIdPage(
    const QString& table, qint64& cursor, int limit)
{
    RowIdMigrationPage page;
    const bool isTorrents = (table == kTorrents);
    // Where this page started. Every failure path below rewinds to it, so a
    // page is either fully committed or fully retried — never skipped.
    const qint64 startCursor = cursor;

    bool readOk = false;
    const auto rows = db_->query(
        QStringLiteral("SELECT * FROM %1 WHERE id > %2 ORDER BY id ASC LIMIT %3").arg(table).arg(cursor).arg(limit), {},
        &readOk);
    if (!readOk) {
        // A failed query returns no rows just like an exhausted table does.
        // Telling the two apart is what keeps the migration from declaring
        // itself done after one transient searchd error.
        qWarning() << "[TorrentRepository] migration: failed to read" << table << "page past id" << startCursor;
        page.failed = true;
        return page;
    }
    if (rows.isEmpty()) {
        page.finished = true;
        return page;
    }

    // What each row wants to become. The row itself is rebuilt here, while the
    // source columns are still in hand.
    struct Move {
        qint64 oldId = 0;
        qint64 newId = 0;
        QString hash;
        QVariantMap row;
    };
    QVector<Move> moves;
    QVector<qint64> doomed; // rows to delete outright

    for (const auto& row : rows) {
        const qint64 oldId = row.value(QStringLiteral("id")).toLongLong();
        cursor = oldId;
        ++page.scanned;

        const QString hash = row.value(QStringLiteral("hash")).toString();
        const qint64 newId = rowIdFromHash(hash);
        if (newId == 0) {
            qWarning() << "[TorrentRepository] migration: dropping" << table << "row" << oldId << "with unusable hash"
                       << hash;
            doomed.append(oldId);
            ++page.dropped;
            continue;
        }
        if (newId == oldId)
            continue; // already migrated

        Move move;
        move.oldId = oldId;
        move.newId = newId;
        move.hash = hash;
        if (isTorrents) {
            // Round-trip through the domain type so the rewritten row is byte for
            // byte what a normal insert would have produced — nameIndex rebuilt,
            // info re-encoded as JSON rather than re-quoted as a string.
            move.row = torrentRow(rowToTorrent(row));
        } else {
            // A files row is four plain columns; there is nothing to re-derive.
            move.row = QVariantMap { { "id", static_cast<qlonglong>(newId) }, { "hash", hash },
                { "path", row.value(QStringLiteral("path")) }, { "size", row.value(QStringLiteral("size")) } };
        }
        moves.append(move);
    }

    if (moves.isEmpty() && doomed.isEmpty())
        return page;

    // Refuse a move whose destination is already held by a different torrent.
    QHash<qint64, QString> ownerById;
    if (!moves.isEmpty()) {
        QVector<qint64> targets;
        targets.reserve(moves.size());
        for (const Move& move : moves)
            targets.append(move.newId);
        const QString sql = SelectQuery(table)
                                .columns(QStringLiteral("id, hash"))
                                .whereInIds(QStringLiteral("id"), targets)
                                .limit(0, targets.size())
                                .build();
        bool ownersOk = false;
        const auto owners = db_->query(sql, {}, &ownersOk);
        if (!ownersOk) {
            // Without this lookup a taken destination reads as free, and the move
            // would overwrite a stranger's row. Retry the page instead.
            qWarning() << "[TorrentRepository] migration: failed to check" << table << "destinations past id"
                       << startCursor;
            cursor = startCursor;
            page.failed = true;
            return page;
        }
        for (const auto& row : owners) {
            ownerById.insert(
                row.value(QStringLiteral("id")).toLongLong(), row.value(QStringLiteral("hash")).toString());
        }
    }

    QVector<QVariantMap> rewrites;
    QSet<qint64> claimed; // destinations taken earlier in this same page
    rewrites.reserve(moves.size());

    for (const Move& move : moves) {
        const QString owner = ownerById.value(move.newId);
        if (!owner.isEmpty() && !sameHash(owner, move.hash)) {
            qWarning() << "[TorrentRepository] migration: dropping" << table << "row" << move.oldId << "-"
                       << "row id" << move.newId << "already belongs to a different torrent";
            doomed.append(move.oldId);
            ++page.dropped;
            continue;
        }
        if (claimed.contains(move.newId)) {
            // The same hash stored twice under the old counter ids. The first
            // move carries it; this one is only a duplicate to retire.
            doomed.append(move.oldId);
            continue;
        }
        claimed.insert(move.newId);
        rewrites.append(move.row);
        doomed.append(move.oldId);
        ++page.rewritten;
    }

    // Write the replacements before retiring the originals: interrupted here, the
    // table holds the same torrent under two ids and the next pass re-does the
    // move. Interrupted the other way round, the row would simply be gone.
    if (!rewrites.isEmpty() && !db_->replaceMany(table, rewrites)) {
        qWarning() << "[TorrentRepository] migration: failed to write" << rewrites.size() << table << "rows";
        cursor = startCursor;
        page.rewritten = 0;
        page.failed = true;
        return page;
    }

    if (!doomed.isEmpty()) {
        QStringList ids;
        ids.reserve(doomed.size());
        for (qint64 id : doomed)
            ids << QString::number(id);
        if (!db_->execute(
                QStringLiteral("DELETE FROM %1 WHERE id IN (%2)").arg(table, ids.join(QLatin1String(", "))))) {
            // The replacements are in, the originals are not out: the table now
            // holds some torrents twice. Rewinding re-runs the page, which skips
            // the rows already carrying their derived id and retries this delete.
            qWarning() << "[TorrentRepository] migration: failed to retire" << doomed.size() << table << "rows";
            cursor = startCursor;
            page.failed = true;
            return page;
        }
    }

    return page;
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
