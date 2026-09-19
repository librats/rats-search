#include "services/indexing_service.h"

#include "data/torrent_repository.h"
#include "domain/content_classifier.h"
#include "services/filter_policy.h"

#include <QDebug>
#include <QSet>
#include <QStringList>

namespace rats::service {

IndexingService::IndexingService(data::TorrentRepository* repository, FilterPolicy* filter, QObject* parent)
    : QObject(parent), repository_(repository), filter_(filter)
{
}

IndexingService::Result IndexingService::insert(domain::Torrent torrent)
{
    Result result;

    if (!torrent.isValid()) {
        result.error = QStringLiteral("Invalid hash");
        return result;
    }
    if (torrent.name.isEmpty()) {
        result.error = QStringLiteral("Empty torrent name");
        return result;
    }

    // One lookup serves both halves of this method: it answers the dedup question
    // below and is handed to add() further down, so a torrent the crawler has
    // never seen costs a single docid query instead of one here and another
    // inside add().
    const data::TorrentRepository::RowSlot slot = repository_->resolve(torrent.hash, /*withRow*/ true);

    // Already indexed? Merge what the incoming copy adds over the stored one
    // (e.g. from a peer) and return the stored entity.
    if (slot.stored) {
        domain::Torrent existing = *slot.stored;
        result.success = true;
        result.alreadyExists = true;
        result.torrent = existing;

        // Backfill the file list when the stored copy has none but the incoming
        // one does. This heals a metadata-only row into a complete torrent
        // instead of leaving the two out of sync.
        if (existing.files == 0 && !torrent.fileList.isEmpty()
            && repository_->updateFiles(existing.hash, torrent.fileList)) {
            existing.fileList = torrent.fileList;
            existing.files = torrent.fileList.size();
            // Re-classify now that we have the file list.
            domain::ContentClassifier::classify(existing);
            repository_->updateClassification(existing.hash, existing.contentType, existing.contentCategory);
            result.torrent = existing;
        }

        if (torrent.good > existing.good || torrent.bad > existing.bad) {
            existing.good = qMax(existing.good, torrent.good);
            existing.bad = qMax(existing.bad, torrent.bad);
            repository_->update(existing);
            result.torrent = existing;
        }
        return result;
    }

    if (torrent.contentType == domain::ContentType::Unknown)
        domain::ContentClassifier::classify(torrent);

    if (filter_) {
        if (const QString reason = filter_->rejectionReason(torrent); !reason.isEmpty()) {
            qInfo() << "[Indexing] rejected" << torrent.hash.left(16) << "-" << reason;
            result.error = QStringLiteral("Rejected: ") + reason;
            return result;
        }
    }

    // The slot resolved above is still the one this hash maps to; add() only has
    // to trust it, not look it up again.
    if (!repository_->add(torrent, slot)) {
        result.error = slot.collided() ? QStringLiteral("Row id belongs to a different torrent")
                                       : QStringLiteral("Database insert failed");
        return result;
    }

    result.success = true;
    result.torrent = torrent;
    qInfo() << "[Indexing] indexed" << torrent.hash.left(16) << torrent.name.left(50)
            << "size:" << (torrent.size / (1024 * 1024)) << "MB files:" << torrent.files;

    emit torrentIndexed(torrent);
    return result;
}

IndexingService::BatchResult IndexingService::insertBatch(
    QVector<domain::Torrent> torrents, const BatchOptions& options)
{
    BatchResult result;
    if (torrents.isEmpty())
        return result;

    // 1. Validate and de-duplicate inside the batch itself. A dump can carry the
    //    same hash twice (two exports concatenated); letting both through would
    //    insert one row and then treat the second one as new as well.
    QVector<domain::Torrent> candidates;
    candidates.reserve(torrents.size());
    QSet<QString> seen;
    for (const domain::Torrent& t : torrents) {
        if (!t.isValid() || t.name.isEmpty()) {
            ++result.invalid;
            continue;
        }
        if (seen.contains(t.hash))
            continue;
        seen.insert(t.hash);
        candidates.append(t);
    }
    if (candidates.isEmpty())
        return result;

    // 2. Dedupe against the index with one lookup for the whole batch instead of
    //    a get() per torrent. Like insert(), this happens BEFORE classification
    //    and filtering: an already-stored torrent keeps its row and merges, and
    //    is never re-classified or re-judged by a filter it predates.
    QStringList hashes;
    hashes.reserve(candidates.size());
    for (const domain::Torrent& t : candidates)
        hashes << t.hash;
    QSet<QString> collided;
    const QHash<QString, domain::Torrent> existing = repository_->getMany(hashes, &collided);

    QVector<domain::Torrent> fresh;
    fresh.reserve(candidates.size());

    for (domain::Torrent& incoming : candidates) {
        // A torrent whose row id is held by a different one cannot be written
        // without destroying that one, so it is dropped here rather than in the
        // repository — the batch write below has no way to skip a single row.
        if (collided.contains(incoming.hash)) {
            ++result.collided;
            continue;
        }

        auto it = existing.constFind(incoming.hash);
        if (it == existing.constEnd()) {
            if (incoming.contentType == domain::ContentType::Unknown)
                domain::ContentClassifier::classify(incoming);
            if (options.applyFilters && filter_ && !filter_->accepts(incoming)) {
                ++result.rejected;
                continue;
            }
            fresh.append(incoming);
            continue;
        }

        ++result.merged;
        if (!options.mergeExisting)
            continue;

        // Same merge rules as insert(): heal a metadata-only row with the file
        // list the incoming copy carries, and keep the higher vote counts.
        domain::Torrent stored = *it;
        // Keep the in-memory copy in step with what updateFiles() just wrote:
        // update() below REPLACEs the whole row from this snapshot, so a stale
        // files == 0 here would undo the backfill and leave the row claiming no
        // files while the files table holds the list — which also re-triggers
        // this branch (and its statistics delta) on every later import.
        if (stored.files == 0 && !incoming.fileList.isEmpty()
            && repository_->updateFiles(stored.hash, incoming.fileList)) {
            stored.fileList = incoming.fileList;
            stored.files = incoming.fileList.size();
        }

        if (incoming.good > stored.good || incoming.bad > stored.bad) {
            stored.good = qMax(stored.good, incoming.good);
            stored.bad = qMax(stored.bad, incoming.bad);
            repository_->update(stored);
        }
    }

    // 3. Everything genuinely new goes in as one multi-row write.
    if (!fresh.isEmpty())
        result.inserted = repository_->addMany(fresh);

    return result;
}

bool IndexingService::accepts(const domain::Torrent& torrent) const
{
    return !filter_ || filter_->accepts(torrent);
}

} // namespace rats::service
