#ifndef RATS_DOMAIN_PEER_H
#define RATS_DOMAIN_PEER_H

#include <QJsonObject>
#include <QString>

namespace rats::domain {

// Statistics a rats-search peer advertises about itself during the handshake.
// This is application data (how many torrents a peer indexes), deliberately kept
// out of the transport layer — the transport only moves bytes.
struct PeerStats {
    QString clientVersion;
    qint64 torrents = 0;
    qint64 files = 0;
    qint64 totalSize = 0;
    int peersConnected = 0;
    qint64 connectedAt = 0; // ms since epoch, set locally on connect

    QJsonObject toJson() const
    {
        return QJsonObject {
            { "clientVersion", clientVersion },
            { "torrents", torrents },
            { "files", files },
            { "totalSize", totalSize },
            { "peersConnected", peersConnected },
            { "connectedAt", connectedAt },
        };
    }

    static PeerStats fromJson(const QJsonObject& obj)
    {
        PeerStats s;
        s.clientVersion = obj["clientVersion"].toString();
        // Accept both the new keys and the legacy *Count spellings.
        s.torrents = obj.contains("torrents") ? obj["torrents"].toVariant().toLongLong()
                                              : obj["torrentsCount"].toVariant().toLongLong();
        s.files = obj.contains("files") ? obj["files"].toVariant().toLongLong()
                                        : obj["filesCount"].toVariant().toLongLong();
        s.totalSize = obj["totalSize"].toVariant().toLongLong();
        s.peersConnected = obj["peersConnected"].toInt();
        s.connectedAt = obj["connectedAt"].toVariant().toLongLong();
        return s;
    }
};

} // namespace rats::domain

#endif // RATS_DOMAIN_PEER_H
