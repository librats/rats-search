#include "torrentspider.h"
#include "torrentdatabase.h"
#include "p2pnetwork.h"
// Neutralise Qt's `emit` macro across librats includes (EventBus::emit collides).
#pragma push_macro("emit")
#pragma push_macro("slots")
#pragma push_macro("signals")
#undef emit
#undef slots
#undef signals
#ifdef RATS_SEARCH_FEATURES
#include "subsystems/bittorrent.h"
#include "bittorrent/torrent_info.h"
#include "dht/dht.h"
#include "core/address.h"
#endif
#pragma pop_macro("signals")
#pragma pop_macro("slots")
#pragma pop_macro("emit")
#include <QDebug>
#include <QDateTime>
#include <QVector>
#include <set>

TorrentSpider::TorrentSpider(TorrentDatabase *database, P2PNetwork *p2pNetwork, QObject *parent)
    : QObject(parent)
    , database_(database)
    , p2pNetwork_(p2pNetwork)
    , running_(false)
    , indexedCount_(0)
    , pendingCount_(0)
    , walkIntervalMs_(100)       // Walk every 100ms
    , ignoreIntervalMs_(1000)    // Toggle ignore every 1s for rate limiting
    , metadataFetchEnabled_(true)
    , activeFetches_(0)
{
    walkTimer_ = new QTimer(this);
    ignoreTimer_ = new QTimer(this);
    metadataQueueTimer_ = new QTimer(this);
    
    connect(walkTimer_, &QTimer::timeout, this, &TorrentSpider::onSpiderWalk);
    connect(ignoreTimer_, &QTimer::timeout, this, &TorrentSpider::onIgnoreToggle);
    connect(metadataQueueTimer_, &QTimer::timeout, this, &TorrentSpider::processMetadataQueue);
}

TorrentSpider::~TorrentSpider()
{
    stop();
}

librats::Bittorrent* TorrentSpider::bittorrent() const
{
    if (!p2pNetwork_) {
        return nullptr;
    }
    return p2pNetwork_->bittorrent();
}

bool TorrentSpider::start()
{
    if (running_) {
        return true;
    }
    
    // Check that P2PNetwork is running and provides RatsClient
    if (!p2pNetwork_) {
        emit error("P2PNetwork not available");
        return false;
    }
    
    if (!p2pNetwork_->isRunning()) {
        emit error("P2PNetwork is not running - start it first");
        return false;
    }

    librats::Bittorrent* bt = bittorrent();
    if (!bt) {
        emit error("BitTorrent subsystem not available from P2PNetwork");
        return false;
    }

    qInfo() << "Starting torrent spider...";
    
    try {
        // Check if DHT is running
        if (!p2pNetwork_->isDhtRunning()) {
            emit error("DHT is not running");
            return false;
        }
        
#ifdef RATS_SEARCH_FEATURES
        // Enable spider mode with announce callback. The new core delivers the
        // info hash already decoded and the peer as a structured Address, so no
        // string parsing is needed.
        bt->set_spider_mode(true);

        bt->set_spider_announce_callback(
            [this](const librats::InfoHash& info_hash, const librats::Address& peer) {
                std::array<uint8_t, 20> infoHash = info_hash;
                std::string ip = peer.ip.to_string();
                uint16_t port = peer.port;

                // Call onAnnounce on main thread
                QMetaObject::invokeMethod(this, [this, infoHash, ip, port]() {
                    onAnnounce(infoHash, ip, port);
                }, Qt::QueuedConnection);
            });

        qInfo() << "Spider mode enabled with announce callback";
#endif
        
        running_ = true;
        
        // Start timers - use walk timer for spider_walk
        walkTimer_->start(walkIntervalMs_);
        ignoreTimer_->start(ignoreIntervalMs_);
        metadataQueueTimer_->start(100);  // Process queue every 100ms
        
        emit started();
        emit statusChanged("Active");
        
        qInfo() << "Torrent spider started successfully";
        qInfo() << "DHT nodes:" << getDhtNodeCount();
        return true;
        
    } catch (const std::exception& e) {
        emit error(QString("Failed to start spider: %1").arg(e.what()));
        return false;
    }
}

void TorrentSpider::stop()
{
    if (!running_) {
        return;
    }
    
    qInfo() << "Stopping torrent spider...";
    
    walkTimer_->stop();
    ignoreTimer_->stop();
    metadataQueueTimer_->stop();
    
#ifdef RATS_SEARCH_FEATURES
    // Disable spider mode
    librats::Bittorrent* bt = bittorrent();
    if (bt) {
        bt->set_spider_mode(false);
    }
    activeFetches_ = 0;
#endif

    // Note: We don't stop the BitTorrent subsystem here - P2PNetwork owns it

    running_ = false;
    
    emit stopped();
    emit statusChanged("Stopped");
    
    qInfo() << "Torrent spider stopped. Total indexed:" << indexedCount_.load();
}

bool TorrentSpider::isRunning() const
{
    return running_;
}

int TorrentSpider::getIndexedCount() const
{
    return indexedCount_.load();
}

int TorrentSpider::getPendingCount() const
{
    return pendingCount_.load();
}

void TorrentSpider::setWalkInterval(int intervalMs)
{
    walkIntervalMs_ = intervalMs;
    if (running_ && walkTimer_) {
        walkTimer_->setInterval(intervalMs);
    }
}

int TorrentSpider::getWalkInterval() const
{
    return walkIntervalMs_;
}

void TorrentSpider::setIgnoreInterval(int intervalMs)
{
    ignoreIntervalMs_ = intervalMs;
    if (running_ && ignoreTimer_) {
        ignoreTimer_->setInterval(intervalMs);
    }
}

void TorrentSpider::setMetadataFetchEnabled(bool enabled)
{
    metadataFetchEnabled_ = enabled;
}

size_t TorrentSpider::getDhtNodeCount() const
{
    if (!p2pNetwork_) {
        return 0;
    }
    return p2pNetwork_->getDhtNodeCount();
}

size_t TorrentSpider::getSpiderPoolSize() const
{
#ifdef RATS_SEARCH_FEATURES
    librats::Bittorrent* bt = bittorrent();
    if (bt) {
        return bt->spider_pool_size();
    }
#endif
    return 0;
}

size_t TorrentSpider::getSpiderVisitedCount() const
{
#ifdef RATS_SEARCH_FEATURES
    librats::Bittorrent* bt = bittorrent();
    if (bt) {
        return bt->spider_visited_count();
    }
#endif
    return 0;
}

void TorrentSpider::onSpiderWalk()
{
    if (!running_) {
        return;
    }

#ifdef RATS_SEARCH_FEATURES
    // Trigger spider walk to expand DHT routing table
    librats::Bittorrent* bt = bittorrent();
    if (bt) {
        bt->spider_walk();
    }
#endif
}

void TorrentSpider::onIgnoreToggle()
{
#ifdef RATS_SEARCH_FEATURES
    // Rate limiting logic - toggle spider ignore mode
    // In legacy code, this toggled spider_ignore to manage incoming request rate
    librats::Bittorrent* bt = bittorrent();
    if (bt && bt->is_spider_mode()) {
        // Toggle ignore mode - this limits incoming DHT requests
        bool currentIgnore = bt->is_spider_ignoring();
        bt->set_spider_ignore(!currentIgnore);
    }
#endif
}

void TorrentSpider::processMetadataQueue()
{
    if (!running_ || !metadataFetchEnabled_) {
        return;
    }

    // Process metadata queue
    while (activeFetches_.load() < MAX_CONCURRENT_METADATA_FETCHES) {
        QString queueEntry;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (metadataQueue_.empty()) {
                break;
            }
            queueEntry = metadataQueue_.front();
            metadataQueue_.pop();
            pendingCount_ = static_cast<int>(metadataQueue_.size());
        }
        
        // Parse queue entry (format: "hash|ip|port")
        QStringList parts = queueEntry.split('|');
        if (parts.size() == 3) {
            QString hash = parts[0];
            QString peerIp = parts[1];
            uint16_t peerPort = static_cast<uint16_t>(parts[2].toUInt());
            fetchMetadata(hash, peerIp, peerPort);
        } else {
            // Fallback for old format (just hash) - use DHT search
            fetchMetadata(queueEntry, QString(), 0);
        }
    }
}

void TorrentSpider::onAnnounce(const std::array<uint8_t, 20>& infoHash,
                               const std::string& ip, uint16_t port)
{
    // Convert info hash to hex string
    QString hashHex;
    for (uint8_t byte : infoHash) {
        hashHex += QString("%1").arg(byte, 2, 16, QChar('0'));
    }
    
    emit torrentDiscovered(hashHex);
    
    // Check if we've seen this hash recently
    {
        std::lock_guard<std::mutex> lock(recentHashesMutex_);
        if (recentHashes_.count(hashHex) > 0) {
            qDebug() << "Already seen hash:" << hashHex << " - ignoring";
            return;  // Already seen
        }
        
        recentHashes_.insert(hashHex);
        
        // Limit size of recent hashes
        if (recentHashes_.size() > MAX_RECENT_HASHES) {
            auto it = recentHashes_.begin();
            std::advance(it, recentHashes_.size() / 2);
            recentHashes_.erase(recentHashes_.begin(), it);
        }
    }
    
    // Check if already in database
    if (database_ && database_->hasTorrent(hashHex)) {
        return;
    }

    qDebug() << "Discovered torrent:" << hashHex << "from peer" << QString::fromStdString(ip) << ":" << port;
    
    // Add to metadata queue with peer address (format: "hash|ip|port")
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        QString queueEntry = QString("%1|%2|%3").arg(hashHex).arg(QString::fromStdString(ip)).arg(port);
        metadataQueue_.push(queueEntry);
        pendingCount_ = static_cast<int>(metadataQueue_.size());
    }
}

void TorrentSpider::fetchMetadata(const QString& infoHash, const QString& peerIp, uint16_t peerPort)
{
#ifdef RATS_SEARCH_FEATURES
    librats::Bittorrent* bt = bittorrent();
    if (!bt) {
        return;
    }

    activeFetches_++;

    // librats manages the temporary torrent, the BEP 9 fetch and the timeout; it
    // invokes this callback exactly once (success or timeout) on a worker thread.
    auto onResult = [this, infoHash](const librats::bittorrent::TorrentInfo& torrentInfo, bool success,
                                     const std::string& error) {
        activeFetches_--;

        if (!success || !torrentInfo.is_valid()) {
            qInfo() << "Failed to get metadata for" << infoHash.left(8)
                    << ":" << QString::fromStdString(error);
            return;
        }

        // Extract file list
        QVector<QPair<QString, qint64>> filesList;
        for (const auto& file : torrentInfo.files().files()) {
            filesList.append(qMakePair(QString::fromStdString(file.path),
                                       static_cast<qint64>(file.size)));
        }

        // Call handler on main thread
        QMetaObject::invokeMethod(this, [this, infoHash,
                                         name = QString::fromStdString(torrentInfo.name()),
                                         totalSize = static_cast<qint64>(torrentInfo.total_size()),
                                         files = static_cast<int>(torrentInfo.files().files().size()),
                                         pieceLength = static_cast<int>(torrentInfo.piece_length()),
                                         filesList]() {
            onMetadataReceived(infoHash, name, totalSize, files, pieceLength, filesList);
        }, Qt::QueuedConnection);
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
    Q_UNUSED(infoHash);
    Q_UNUSED(peerIp);
    Q_UNUSED(peerPort);
    qWarning() << "BitTorrent features not enabled, cannot fetch metadata";
#endif
}

void TorrentSpider::onMetadataReceived(const QString& infoHash,
                                        const QString& name,
                                        qint64 size,
                                        int files,
                                        int pieceLength,
                                        const QVector<QPair<QString, qint64>>& filesList)
{
    if (!database_) {
        return;
    }
    
    qInfo() << "Indexed torrent:" << name << "(" << infoHash.left(8) << ")";
    
    // Create torrent info
    TorrentInfo torrent;
    torrent.hash = infoHash.toLower();
    torrent.name = name;
    torrent.size = size;
    torrent.files = files;
    torrent.piecelength = pieceLength;
    torrent.added = QDateTime::currentDateTime();
    
    // Add file list
    for (const auto& file : filesList) {
        TorrentFile tf;
        tf.path = file.first;
        tf.size = file.second;
        torrent.filesList.append(tf);
    }
    
    // Detect content type
    TorrentDatabase::detectContentType(torrent);
    
    // Add to database
    if (database_->addTorrent(torrent)) {
        indexedCount_++;
        emit indexedCountChanged(indexedCount_);
        emit torrentIndexed(infoHash, name);
    }
}
