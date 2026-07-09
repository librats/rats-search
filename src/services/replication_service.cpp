#include "services/replication_service.h"

#include "net/p2p_transport.h"

#include <QDebug>
#include <QJsonObject>
#include <QTimer>

namespace rats::service {

namespace {
constexpr int kInitialIntervalMs = 5000;
constexpr int kIdleIntervalMs = 10000;
constexpr int kMaxIntervalMs = 60000;
constexpr int kSettleDelayMs = 3000; // wait for peer replies before adapting
constexpr int kBusyThreshold = 8; // received above this => back off
constexpr int kBackoffPerTorrentMs = 600;
constexpr int kTorrentsPerPeer = 5;
} // namespace

ReplicationService::ReplicationService(net::P2PTransport* transport, QObject* parent)
    : QObject(parent), transport_(transport), timer_(new QTimer(this)), interval_(kInitialIntervalMs)
{
    connect(timer_, &QTimer::timeout, this, &ReplicationService::performCycle);
}

void ReplicationService::setEnabled(bool enabled)
{
    enabled_ = enabled;
    if (!enabled_)
        stop();
}

void ReplicationService::start()
{
    if (!enabled_ || timer_->isActive())
        return;
    interval_ = kInitialIntervalMs;
    received_ = 0;
    timer_->start(interval_);
    qInfo() << "[Replication] started, interval" << interval_ << "ms";
    emit started();
}

void ReplicationService::stop()
{
    if (!timer_->isActive())
        return;
    timer_->stop();
    qInfo() << "[Replication] stopped, total replicated:" << totalReplicated_;
    emit stopped();
}

bool ReplicationService::isActive() const
{
    return timer_->isActive();
}

void ReplicationService::performCycle()
{
    if (!enabled_) {
        stop();
        return;
    }
    if (transport_->peerCount() == 0)
        return;

    received_ = 0;
    transport_->broadcastMessage(QStringLiteral("randomTorrents"),
        QJsonObject { { "limit", kTorrentsPerPeer }, { "version", QStringLiteral("2.0") } });

    // After peers have had time to reply, adapt the interval: back off when we
    // are pulling a lot, speed up when the well is dry.
    QTimer::singleShot(kSettleDelayMs, this, [this]() {
        const int received = received_.load();
        interval_ = received > kBusyThreshold ? qMin(kMaxIntervalMs, received * kBackoffPerTorrentMs) : kIdleIntervalMs;
        if (timer_->isActive())
            timer_->setInterval(interval_);
        if (received > 0) {
            totalReplicated_ += received;
            qInfo() << "[Replication] +" << received << "torrents, total" << totalReplicated_ << "next" << interval_
                    << "ms";
        }
    });
}

} // namespace rats::service
