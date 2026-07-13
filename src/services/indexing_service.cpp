#include "services/indexing_service.h"

#include "data/torrent_repository.h"
#include "domain/content_classifier.h"
#include "services/filter_policy.h"

#include <QDebug>

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

    // Already indexed? Merge what the incoming copy adds over the stored one
    // (e.g. from a peer) and return the stored entity.
    if (auto existing = repository_->get(torrent.hash)) {
        result.success = true;
        result.alreadyExists = true;
        result.torrent = *existing;

        // Backfill the file list when the stored copy has none but the incoming
        // one does. This heals a metadata-only row into a complete torrent
        // instead of leaving the two out of sync.
        if (existing->files == 0 && !torrent.fileList.isEmpty()
            && repository_->updateFiles(existing->hash, torrent.fileList)) {
            existing->fileList = torrent.fileList;
            existing->files = torrent.fileList.size();
            result.torrent = *existing;
        }

        if (torrent.good > existing->good || torrent.bad > existing->bad) {
            existing->good = qMax(existing->good, torrent.good);
            existing->bad = qMax(existing->bad, torrent.bad);
            repository_->update(*existing);
            result.torrent = *existing;
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

    // The get() above already proved the hash is absent (single-threaded insert
    // path), so skip the redundant existence query inside add().
    if (!repository_->add(torrent, /*skipExistsCheck*/ true)) {
        result.error = QStringLiteral("Database insert failed");
        return result;
    }

    result.success = true;
    result.torrent = torrent;
    qInfo() << "[Indexing] indexed" << torrent.hash.left(16) << torrent.name.left(50)
            << "size:" << (torrent.size / (1024 * 1024)) << "MB files:" << torrent.files;

    emit torrentIndexed(torrent);
    return result;
}

bool IndexingService::accepts(const domain::Torrent& torrent) const
{
    return !filter_ || filter_->accepts(torrent);
}

} // namespace rats::service
