#include "net/crawler.h"

#include "net/p2p_transport.h"

// librats' EventBus exposes a method named emit(), which collides with Qt's
// `emit` keyword macro. Neutralise the macro across all librats includes, then
// restore it so the crawler's own `emit signal` statements keep working.
#pragma push_macro("emit")
#pragma push_macro("slots")
#pragma push_macro("signals")
#undef emit
#undef slots
#undef signals
#ifdef RATS_SEARCH_FEATURES
#include "bittorrent/torrent_info.h"
#include "core/address.h"
#include "dht/dht.h"
#include "subsystems/bittorrent.h"
#endif
#pragma pop_macro("signals")
#pragma pop_macro("slots")
#pragma pop_macro("emit")

#include <QDateTime>
#include <QDebug>
#include <QVector>
#include <iterator>

namespace rats::net {

Crawler::Crawler(P2PTransport* transport, QObject* parent)
    : QObject(parent)
    , transport_(transport)
    , running_(false)
    , discoveredCount_(0)
    , activeFetches_(0)
    , walkIntervalMs_(DEFAULT_WALK_INTERVAL_MS)
{
    walkTimer_ = new QTimer(this);
    ignoreTimer_ = new QTimer(this);
    metadataQueueTimer_ = new QTimer(this);

    connect(walkTimer_, &QTimer::timeout, this, &Crawler::onSpiderWalk);
    connect(ignoreTimer_, &QTimer::timeout, this, &Crawler::onIgnoreToggle);
    connect(metadataQueueTimer_, &QTimer::timeout, this, &Crawler::processMetadataQueue);
}

Crawler::~Crawler()
{
    stop();
}

librats::Bittorrent* Crawler::bittorrent() const
{
    if (!transport_) {
        return nullptr;
    }
    return transport_->bittorrent();
}

bool Crawler::start()
{
    if (running_) {
        return true;
    }

    if (!transport_) {
        emit error("P2P transport not available");
        return false;
    }

    if (!transport_->isRunning()) {
        emit error("P2P transport is not running - start it first");
        return false;
    }

    librats::Bittorrent* bt = bittorrent();
    if (!bt) {
        emit error("BitTorrent subsystem not available from transport");
        return false;
    }

    if (!transport_->isDhtRunning()) {
        emit error("DHT is not running");
        return false;
    }

    qInfo() << "Starting DHT crawler...";

    try {
#ifdef RATS_SEARCH_FEATURES
        // Enable spider mode with an announce callback. The core delivers the
        // info-hash already decoded and the peer as a structured Address, so no
        // string parsing is needed. The callback fires on a librats worker
        // thread, so marshal it onto the Qt thread before touching our state.
        bt->set_spider_mode(true);

        bt->set_spider_announce_callback([this](const librats::InfoHash& info_hash, const librats::Address& peer) {
            std::array<uint8_t, 20> infoHash = info_hash;
            std::string ip = peer.ip.to_string();
            uint16_t port = peer.port;

            QMetaObject::invokeMethod(
                this, [this, infoHash, ip, port]() { onAnnounce(infoHash, ip, port); }, Qt::QueuedConnection);
        });

        qInfo() << "Spider mode enabled with announce callback";
#endif

        running_ = true;

        walkTimer_->start(walkIntervalMs_);
        ignoreTimer_->start(DEFAULT_IGNORE_INTERVAL_MS);
        metadataQueueTimer_->start(METADATA_QUEUE_INTERVAL_MS);

        emit started();
        emit statusChanged("Active");

        qInfo() << "DHT crawler started successfully";
        qInfo() << "DHT nodes:" << transport_->dhtNodeCount();
        return true;
    } catch (const std::exception& e) {
        emit error(QString("Failed to start crawler: %1").arg(e.what()));
        return false;
    }
}

void Crawler::stop()
{
    if (!running_) {
        return;
    }

    qInfo() << "Stopping DHT crawler...";

    walkTimer_->stop();
    ignoreTimer_->stop();
    metadataQueueTimer_->stop();

#ifdef RATS_SEARCH_FEATURES
    librats::Bittorrent* bt = bittorrent();
    if (bt) {
        bt->set_spider_mode(false);
    }
    activeFetches_ = 0;
#endif

    // Note: we never stop the BitTorrent subsystem here - the transport owns it.

    running_ = false;

    emit stopped();
    emit statusChanged("Stopped");

    qInfo() << "DHT crawler stopped. Total discovered:" << discoveredCount_.load();
}

bool Crawler::isRunning() const
{
    return running_;
}

void Crawler::setWalkInterval(int intervalMs)
{
    walkIntervalMs_ = intervalMs;
    if (running_ && walkTimer_) {
        walkTimer_->setInterval(intervalMs);
    }
}

void Crawler::setKnownHashFilter(KnownHashFilter filter)
{
    knownHashFilter_ = std::move(filter);
}

void Crawler::onSpiderWalk()
{
    if (!running_) {
        return;
    }

#ifdef RATS_SEARCH_FEATURES
    // Trigger a spider walk to expand the DHT routing table.
    librats::Bittorrent* bt = bittorrent();
    if (bt) {
        bt->spider_walk();
    }
#endif
}

void Crawler::onIgnoreToggle()
{
#ifdef RATS_SEARCH_FEATURES
    // Rate limiting: toggle spider ignore mode to throttle incoming DHT requests.
    librats::Bittorrent* bt = bittorrent();
    if (bt && bt->is_spider_mode()) {
        bool currentIgnore = bt->is_spider_ignoring();
        bt->set_spider_ignore(!currentIgnore);
    }
#endif
}

void Crawler::processMetadataQueue()
{
    if (!running_) {
        return;
    }

    while (activeFetches_.load() < MAX_CONCURRENT_METADATA_FETCHES) {
        MetadataRequest request;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (metadataQueue_.empty()) {
                break;
            }
            request = metadataQueue_.front();
            metadataQueue_.pop();
        }

        fetchMetadata(request);
    }
}

void Crawler::onAnnounce(const std::array<uint8_t, 20>& infoHash, const std::string& ip, uint16_t port)
{
    // Convert info-hash to a lower-case hex string.
    QString hashHex;
    for (uint8_t byte : infoHash) {
        hashHex += QString("%1").arg(byte, 2, 16, QChar('0'));
    }

    // De-duplicate: only announce and queue an info-hash the first time we see
    // it.
    {
        std::lock_guard<std::mutex> lock(recentHashesMutex_);
        if (recentHashes_.count(hashHex) > 0) {
            qDebug() << "Already seen hash:" << hashHex << "- ignoring";
            return;
        }

        recentHashes_.insert(hashHex);

        // Bound the dedup set: drop the oldest half once it grows too large.
        if (recentHashes_.size() > MAX_RECENT_HASHES) {
            auto it = recentHashes_.begin();
            std::advance(it, recentHashes_.size() / 2);
            recentHashes_.erase(recentHashes_.begin(), it);
        }
    }

    // Skip torrents we already have: fetching BEP 9 metadata for them would burn
    // a fetch slot and bandwidth only for the insert path to discard the result
    // as a duplicate. The caller injects the "already indexed?" lookup, which
    // keeps the crawler DB-free.
    if (knownHashFilter_ && knownHashFilter_(hashHex)) {
        qDebug() << "Already indexed:" << hashHex << "- skipping metadata fetch";
        return;
    }

    qDebug() << "Discovered torrent:" << hashHex << "from peer" << QString::fromStdString(ip) << ":" << port;

    // Queue a metadata fetch, remembering the announcing peer for the fast path.
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        MetadataRequest request;
        request.infoHash = hashHex;
        request.peerIp = QString::fromStdString(ip);
        request.peerPort = port;
        metadataQueue_.push(request);
    }
}

void Crawler::fetchMetadata(const MetadataRequest& request)
{
#ifdef RATS_SEARCH_FEATURES
    librats::Bittorrent* bt = bittorrent();
    if (!bt) {
        return;
    }

    activeFetches_++;

    const QString infoHash = request.infoHash;
    const QString peerIp = request.peerIp;
    const uint16_t peerPort = request.peerPort;

    // librats manages the temporary torrent, the BEP 9 fetch and the timeout; it
    // invokes this callback exactly once (success or timeout) on a worker thread.
    auto onResult = [this, infoHash, peerIp, peerPort](
                        const librats::bittorrent::TorrentInfo& torrentInfo, bool success, const std::string& err) {
        activeFetches_--;

        if (!success || !torrentInfo.is_valid()) {
            qInfo() << "Failed to get metadata for" << infoHash.left(8) << ":" << QString::fromStdString(err);
            return;
        }

        // Port of createTorrentFromLibrats: build the domain torrent here, on the
        // worker thread, then marshal the finished value onto the Qt thread.
        rats::domain::Torrent torrent;
        torrent.hash = infoHash.toLower();
        torrent.name = QString::fromStdString(torrentInfo.name());
        torrent.size = static_cast<qint64>(torrentInfo.total_size());
        torrent.files = static_cast<int>(torrentInfo.files().files().size());
        torrent.pieceLength = static_cast<int>(torrentInfo.piece_length());
        torrent.added = QDateTime::currentDateTime();
        torrent.ipv4 = peerIp;
        torrent.port = static_cast<int>(peerPort);

        for (const auto& file : torrentInfo.files().files()) {
            rats::domain::File f;
            f.path = QString::fromStdString(file.path);
            f.size = static_cast<qint64>(file.size);
            torrent.fileList.append(f);
        }

        QMetaObject::invokeMethod(this, [this, torrent]() { onMetadataReceived(torrent); }, Qt::QueuedConnection);
    };

    if (!peerIp.isEmpty() && peerPort > 0) {
        // Fast path: fetch directly from the announcing peer (no DHT search).
        qDebug() << "Fetching metadata for" << infoHash.left(8) << "directly from" << peerIp << ":" << peerPort;
        bt->get_torrent_metadata_from_peer(infoHash.toStdString(), peerIp.toStdString(), peerPort, onResult);
    } else {
        // Slow path: let librats find peers via the DHT.
        qDebug() << "Fetching metadata for" << infoHash.left(8) << "via DHT search";
        bt->get_torrent_metadata(infoHash.toStdString(), onResult);
    }
#else
    Q_UNUSED(request);
    qWarning() << "BitTorrent features not enabled, cannot fetch metadata";
#endif
}

void Crawler::onMetadataReceived(const rats::domain::Torrent& torrent)
{
    qInfo() << "Discovered torrent metadata:" << torrent.name << "(" << torrent.hash.left(8) << ")";

    discoveredCount_++;
    emit discovered(torrent);
}

} // namespace rats::net
