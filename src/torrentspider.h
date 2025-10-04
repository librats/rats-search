#ifndef TORRENTSPIDER_H
#define TORRENTSPIDER_H

#include <QObject>
#include <QThread>
#include <memory>
#include "torrentdatabase.h"

namespace librats {
    class RatsClient;
}

/**
 * @brief TorrentSpider - BitTorrent DHT spider for automatic torrent discovery
 * 
 * Crawls the BitTorrent DHT network to discover and index torrents automatically
 */
class TorrentSpider : public QObject
{
    Q_OBJECT

public:
    explicit TorrentSpider(TorrentDatabase *database, int dhtPort, QObject *parent = nullptr);
    ~TorrentSpider();

    bool start();
    void stop();
    bool isRunning() const;
    
    int getIndexedCount() const;

signals:
    void started();
    void stopped();
    void statusChanged(const QString& status);
    void torrentDiscovered(const QString& infoHash);
    void torrentIndexed(const QString& infoHash, const QString& name);
    void error(const QString& errorMessage);

private:
    void setupDhtCallbacks();
    void handleTorrentDiscovery(const QString& infoHash);
    
    TorrentDatabase *database_;
    int dhtPort_;
    bool running_;
    int indexedCount_;
};

#endif // TORRENTSPIDER_H

