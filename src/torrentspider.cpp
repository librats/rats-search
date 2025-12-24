#include "torrentspider.h"
#include "torrentdatabase.h"
#include "p2pnetwork.h"
#include "librats/src/librats.h"
#include <QDebug>
#include <QDateTime>
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

librats::RatsClient* TorrentSpider::getRatsClient() const
{
    if (!p2pNetwork_) {
        return nullptr;
    }
    return p2pNetwork_->getRatsClient();
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
    
    librats::RatsClient* client = getRatsClient();
    if (!client) {
        emit error("RatsClient not available from P2PNetwork");
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
        // Enable BitTorrent for metadata fetching if not already enabled
        if (!p2pNetwork_->isBitTorrentEnabled()) {
            if (!p2pNetwork_->enableBitTorrent()) {
                qWarning() << "Failed to enable BitTorrent for metadata fetching";
                // Continue anyway, we can still discover hashes
            }
        }
        
        // Enable spider mode with announce callback
        client->set_spider_mode(true);
        
        // Set up spider announce callback to handle announce_peer messages
        client->set_spider_announce_callback(
            [this](const std::string& info_hash_hex, const std::string& peer_address) {
                // Parse peer address (ip:port)
                std::string ip;
                uint16_t port = 0;
                size_t colon_pos = peer_address.find(':');
                if (colon_pos != std::string::npos) {
                    ip = peer_address.substr(0, colon_pos);
                    port = static_cast<uint16_t>(std::stoi(peer_address.substr(colon_pos + 1)));
                }
                
                // Convert hex string to array
                std::array<uint8_t, 20> infoHash;
                for (size_t i = 0; i < 20 && i * 2 + 1 < info_hash_hex.size(); ++i) {
                    infoHash[i] = static_cast<uint8_t>(std::stoi(info_hash_hex.substr(i * 2, 2), nullptr, 16));
                }
                
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
    librats::RatsClient* client = getRatsClient();
    if (client) {
        client->set_spider_mode(false);
    }
#endif
    
    // Note: We don't stop the RatsClient here - P2PNetwork owns it
    
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

void TorrentSpider::onSpiderWalk()
{
    if (!running_) {
        return;
    }
    
#ifdef RATS_SEARCH_FEATURES
    // Trigger spider walk to expand DHT routing table
    librats::RatsClient* client = getRatsClient();
    if (client) {
        client->spider_walk();
    }
#endif
}

void TorrentSpider::onIgnoreToggle()
{
#ifdef RATS_SEARCH_FEATURES
    // Rate limiting logic - toggle spider ignore mode
    // In legacy code, this toggled spider_ignore to manage incoming request rate
    librats::RatsClient* client = getRatsClient();
    if (client && client->is_spider_mode()) {
        // Toggle ignore mode - this limits incoming DHT requests
        bool currentIgnore = client->is_spider_ignoring();
        client->set_spider_ignore(!currentIgnore);
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
        QString hash;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (metadataQueue_.empty()) {
                break;
            }
            hash = metadataQueue_.front();
            metadataQueue_.pop();
            pendingCount_ = static_cast<int>(metadataQueue_.size());
        }
        
        fetchMetadata(hash);
    }
}

void TorrentSpider::onAnnounce(const std::array<uint8_t, 20>& infoHash,
                               const std::string& ip, uint16_t port)
{
    Q_UNUSED(ip);
    Q_UNUSED(port);
    
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

    qDebug() << "Discovered torrent:" << hashHex;
    
    // Add to metadata queue
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        metadataQueue_.push(hashHex);
        pendingCount_ = static_cast<int>(metadataQueue_.size());
    }
}

void TorrentSpider::fetchMetadata(const QString& infoHash)
{
    librats::RatsClient* client = getRatsClient();
    if (!client) {
        return;
    }
    
#ifdef RATS_SEARCH_FEATURES
    activeFetches_++;
    
    client->get_torrent_metadata(infoHash.toStdString(),
        [this, infoHash](const librats::TorrentInfo& torrentInfo, bool success, const std::string& error) {
            activeFetches_--;
            
            if (!success) {
                qDebug() << "Failed to get metadata for" << infoHash.left(8) << ":" << QString::fromStdString(error);
                return;
            }
            
            // Extract file list
            QVector<QPair<QString, qint64>> filesList;
            for (const auto& file : torrentInfo.get_files()) {
                filesList.append(qMakePair(QString::fromStdString(file.path), static_cast<qint64>(file.length)));
            }
            
            // Call handler on main thread
            QMetaObject::invokeMethod(this, [this, infoHash, 
                                             name = QString::fromStdString(torrentInfo.get_name()),
                                             totalSize = static_cast<qint64>(torrentInfo.get_total_length()),
                                             files = static_cast<int>(torrentInfo.get_files().size()),
                                             pieceLength = static_cast<int>(torrentInfo.get_piece_length()),
                                             filesList]() {
                onMetadataReceived(infoHash, name, totalSize, files, pieceLength, filesList);
            }, Qt::QueuedConnection);
        });
#else
    Q_UNUSED(infoHash);
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
