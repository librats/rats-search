#ifndef RATS_SERVICE_INDEXING_SERVICE_H
#define RATS_SERVICE_INDEXING_SERVICE_H

#include "domain/torrent.h"

#include <QObject>

namespace rats::data {
class TorrentRepository;
}

namespace rats::service {

class FilterPolicy;

// The single entry point for putting a torrent into the index, no matter the
// source (DHT crawler, P2P replication, .torrent import, feed sync). The flow
// is: validate -> dedupe (merging incoming votes) -> classify -> filter ->
// persist -> notify.
class IndexingService : public QObject {
    Q_OBJECT

public:
    struct Result {
        bool success = false;
        bool alreadyExists = false;
        domain::Torrent torrent;
        QString error;
    };

    IndexingService(data::TorrentRepository* repository, FilterPolicy* filter, QObject* parent = nullptr);

    // Insert `torrent`, running the content classifier first when its content
    // type is unknown. Returns the stored torrent (or the pre-existing one) on
    // success.
    Result insert(domain::Torrent torrent);

    // Whether a torrent passes the current filter policy (used by the
    // maintenance sweep that re-applies filters to the existing index).
    bool accepts(const domain::Torrent& torrent) const;

signals:
    // Emitted once when a genuinely new torrent has been indexed. Carries the
    // full stored entity so listeners (tracker checks, UI, replication) need no
    // second lookup.
    void torrentIndexed(const domain::Torrent& torrent);

private:
    data::TorrentRepository* repository_;
    FilterPolicy* filter_;
};

} // namespace rats::service

#endif // RATS_SERVICE_INDEXING_SERVICE_H
