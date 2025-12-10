#include "torrentspider.h"
#include "torrentdatabase.h"
#include "librats/src/librats.h"
#include <QDebug>
#include <QDateTime>
#include <set>

TorrentSpider::TorrentSpider(TorrentDatabase *database, int dhtPort, QObject *parent)
    : QObject(parent)
    , database_(database)
    , dhtPort_(dhtPort)
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

bool TorrentSpider::start()
{
    if (running_) {
        return true;
    }
    
    qInfo() << "Starting torrent spider on DHT port" << dhtPort_;
    
    try {
        // Create librats client for DHT
        librats::NatTraversalConfig natConfig;
        ratsClient_ = std::make_unique<librats::RatsClient>(dhtPort_ + 1000, 0, natConfig);
        
        // Set protocol name
        ratsClient_->set_protocol_name("rats-spider");
        ratsClient_->set_protocol_version("2.0");
        
        // Start the client
        if (!ratsClient_->start()) {
            emit error("Failed to start librats client for spider");
            return false;
        }
        
        // Start DHT
        if (!ratsClient_->start_dht_discovery(dhtPort_)) {
            emit error("Failed to start DHT for spider");
            return false;
        }
        
#ifdef RATS_SEARCH_FEATURES
        // Enable BitTorrent for metadata fetching
        if (!ratsClient_->enable_bittorrent(dhtPort_)) {
            qWarning() << "Failed to enable BitTorrent for metadata fetching";
            // Continue anyway, we can still discover hashes
        }
        
        // Get DHT client and enable spider mode
        // Note: We need to access DHT through the client
        // For now, we'll use periodic walk instead of spider mode
        // TODO: Add spider mode API to RatsClient
#endif
        
        running_ = true;
        
        // Start timers
        walkTimer_->start(walkIntervalMs_);
        ignoreTimer_->start(ignoreIntervalMs_);
        metadataQueueTimer_->start(100);  // Process queue every 100ms
        
        emit started();
        emit statusChanged("Active");
        
        qInfo() << "Torrent spider started successfully";
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
    
    if (ratsClient_) {
#ifdef RATS_SEARCH_FEATURES
        ratsClient_->disable_bittorrent();
#endif
        ratsClient_->stop_dht_discovery();
        ratsClient_->stop();
        ratsClient_.reset();
    }
    
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
    if (!ratsClient_) {
        return 0;
    }
    return ratsClient_->get_dht_routing_table_size();
}

void TorrentSpider::onSpiderWalk()
{
    if (!running_ || !ratsClient_) {
        return;
    }
    
    // The DHT discovery in librats already performs periodic walks
    // Here we can add additional logic if needed
}

void TorrentSpider::onIgnoreToggle()
{
    // Rate limiting logic
    // In legacy code, this toggled spider_ignore to manage incoming request rate
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
    
    // Add to metadata queue
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        metadataQueue_.push(hashHex);
        pendingCount_ = static_cast<int>(metadataQueue_.size());
    }
}

void TorrentSpider::fetchMetadata(const QString& infoHash)
{
    if (!ratsClient_) {
        return;
    }
    
#ifdef RATS_SEARCH_FEATURES
    activeFetches_++;
    
    ratsClient_->get_torrent_metadata(infoHash.toStdString(),
        [this, infoHash](const librats::TorrentInfo& torrentInfo, bool success, const std::string& error) {
            activeFetches_--;
            
            if (!success) {
                qDebug() << "Failed to get metadata for" << infoHash.left(8) << ":" << QString::fromStdString(error);
                return;
            }
            
            // Extract file list
            QVector<QPair<QString, qint64>> filesList;
            for (const auto& file : torrentInfo.get_files()) {
                filesList.append({QString::fromStdString(file.path), static_cast<qint64>(file.size)});
            }
            
            // Call handler on main thread
            QMetaObject::invokeMethod(this, [this, infoHash, 
                                             name = QString::fromStdString(torrentInfo.get_name()),
                                             size = static_cast<qint64>(torrentInfo.get_total_size()),
                                             files = static_cast<int>(torrentInfo.get_files().size()),
                                             pieceLength = static_cast<int>(torrentInfo.get_piece_length()),
                                             filesList]() {
                onMetadataReceived(infoHash, name, size, files, pieceLength, filesList);
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
