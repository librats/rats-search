#include "net/librats_convert.h"

#pragma push_macro("emit")
#pragma push_macro("slots")
#pragma push_macro("signals")
#undef emit
#undef slots
#undef signals
#include "bittorrent/torrent_info.h"
#pragma pop_macro("signals")
#pragma pop_macro("slots")
#pragma pop_macro("emit")

#include <QDateTime>

namespace rats::net {

domain::Torrent toDomainTorrent(
    const QString& infoHash, const librats::bittorrent::TorrentInfo& info, const QString& peerIp, int peerPort)
{
    domain::Torrent torrent;
    torrent.hash = infoHash.toLower();
    torrent.name = QString::fromStdString(info.name());
    torrent.size = static_cast<qint64>(info.total_size());
    torrent.files = static_cast<int>(info.files().files().size());
    torrent.pieceLength = static_cast<int>(info.piece_length());
    torrent.added = QDateTime::currentDateTime();
    torrent.ipv4 = peerIp;
    torrent.port = peerPort;

    for (const auto& file : info.files().files()) {
        domain::File f;
        f.path = QString::fromStdString(file.path);
        f.size = static_cast<qint64>(file.size);
        torrent.fileList.append(f);
    }
    return torrent;
}

} // namespace rats::net
