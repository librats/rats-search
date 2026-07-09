#ifndef RATS_NET_LIBRATS_CONVERT_H
#define RATS_NET_LIBRATS_CONVERT_H

#include "domain/torrent.h"

#include <QString>

namespace librats::bittorrent {
class TorrentInfo;
}

namespace rats::net {

// Convert a librats BitTorrent metadata record into our domain entity. Shared by
// the crawler, the DHT search fallback and peer replication so the librats->domain
// mapping exists in exactly one place.
// `peerIp`/`peerPort` are the announcing peer when known (empty on a DHT lookup).
domain::Torrent toDomainTorrent(const QString& infoHash, const librats::bittorrent::TorrentInfo& info,
    const QString& peerIp = {}, int peerPort = 0);

} // namespace rats::net

#endif // RATS_NET_LIBRATS_CONVERT_H
