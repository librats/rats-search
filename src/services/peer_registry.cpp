#include "services/peer_registry.h"

#include "net/p2p_transport.h"

#include <QDateTime>
#include <QDebug>

namespace rats::service {

PeerRegistry::PeerRegistry(net::P2PTransport* transport, QString clientVersion, QObject* parent)
    : QObject(parent), transport_(transport), clientVersion_(std::move(clientVersion))
{
    connect(transport_, &net::P2PTransport::peerConnected, this, &PeerRegistry::onPeerConnected);
    connect(transport_, &net::P2PTransport::peerDisconnected, this, &PeerRegistry::onPeerDisconnected);
    transport_->registerHandler(QStringLiteral("client_info"),
        [this](const QString& peerId, const QJsonObject& data) { onClientInfo(peerId, data); });
}

void PeerRegistry::updateOurStats(qint64 torrents, qint64 files, qint64 totalSize)
{
    ourTorrents_ = torrents;
    ourFiles_ = files;
    ourTotalSize_ = totalSize;
}

domain::PeerStats PeerRegistry::ourStats() const
{
    domain::PeerStats s;
    s.clientVersion = clientVersion_;
    s.torrents = ourTorrents_;
    s.files = ourFiles_;
    s.totalSize = ourTotalSize_;
    s.peersConnected = transport_->peerCount();
    return s;
}

void PeerRegistry::onPeerConnected(const QString& peerId)
{
    // Introduce ourselves; the peer replies with its own client_info.
    transport_->sendMessage(peerId, QStringLiteral("client_info"), ourStats().toJson());
}

void PeerRegistry::onPeerDisconnected(const QString& peerId)
{
    QMutexLocker locker(&mutex_);
    peers_.remove(peerId);
}

void PeerRegistry::onClientInfo(const QString& peerId, const QJsonObject& data)
{
    domain::PeerStats stats = domain::PeerStats::fromJson(data);
    stats.connectedAt = QDateTime::currentMSecsSinceEpoch();

    {
        QMutexLocker locker(&mutex_);
        peers_[peerId] = stats;
    }

    qInfo() << "[PeerRegistry] peer" << peerId.left(8) << "v" << stats.clientVersion << "torrents:" << stats.torrents
            << "files:" << stats.files;
    emit peerStatsReceived(peerId, stats);
}

QHash<QString, domain::PeerStats> PeerRegistry::connectedPeers() const
{
    QMutexLocker locker(&mutex_);
    return peers_;
}

qint64 PeerRegistry::remoteTorrentsCount() const
{
    QMutexLocker locker(&mutex_);
    qint64 total = 0;
    for (const domain::PeerStats& s : peers_)
        total += s.torrents;
    return total;
}

} // namespace rats::service
