#include "torrentspider.h"
#include <QDebug>
#include <QTimer>

TorrentSpider::TorrentSpider(TorrentDatabase *database, int dhtPort, QObject *parent)
    : QObject(parent)
    , database_(database)
    , dhtPort_(dhtPort)
    , running_(false)
    , indexedCount_(0)
{
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
    
    // TODO: Implement DHT spider using librats
    // For now, just mark as running
    running_ = true;
    
    emit started();
    emit statusChanged("Active");
    
    qInfo() << "Torrent spider started";
    
    return true;
}

void TorrentSpider::stop()
{
    if (!running_) {
        return;
    }
    
    qInfo() << "Stopping torrent spider...";
    
    running_ = false;
    
    emit stopped();
    emit statusChanged("Idle");
    
    qInfo() << "Torrent spider stopped. Total indexed:" << indexedCount_;
}

bool TorrentSpider::isRunning() const
{
    return running_;
}

int TorrentSpider::getIndexedCount() const
{
    return indexedCount_;
}

void TorrentSpider::handleTorrentDiscovery(const QString& infoHash)
{
    emit torrentDiscovered(infoHash);
    
    // TODO: Fetch torrent metadata and add to database
    
    indexedCount_++;
}

