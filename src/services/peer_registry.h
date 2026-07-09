#ifndef RATS_SERVICE_PEER_REGISTRY_H
#define RATS_SERVICE_PEER_REGISTRY_H

#include "domain/peer.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>

namespace rats::net {
class P2PTransport;
}

namespace rats::service {

// The rats-search peer handshake. On every new connection it exchanges a
// "client_info" message carrying our index stats, and it tracks each peer's
// advertised stats so the UI/API can show swarm-wide totals. This is
// application logic, so it lives above the transport layer rather than inside
// it.
class PeerRegistry : public QObject {
    Q_OBJECT

public:
    PeerRegistry(net::P2PTransport* transport, QString clientVersion, QObject* parent = nullptr);

    // Our own advertised stats; call when the database totals change.
    void updateOurStats(qint64 torrents, qint64 files, qint64 totalSize);

    QHash<QString, domain::PeerStats> connectedPeers() const;
    qint64 remoteTorrentsCount() const; // sum of torrents over all connected peers

signals:
    void peerStatsReceived(const QString& peerId, const domain::PeerStats& stats);

private:
    void onPeerConnected(const QString& peerId);
    void onPeerDisconnected(const QString& peerId);
    void onClientInfo(const QString& peerId, const QJsonObject& data);
    domain::PeerStats ourStats() const;

    net::P2PTransport* transport_;
    QString clientVersion_;
    qint64 ourTorrents_ = 0;
    qint64 ourFiles_ = 0;
    qint64 ourTotalSize_ = 0;

    mutable QMutex mutex_;
    QHash<QString, domain::PeerStats> peers_;
};

} // namespace rats::service

#endif // RATS_SERVICE_PEER_REGISTRY_H
