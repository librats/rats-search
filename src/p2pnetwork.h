#ifndef P2PNETWORK_H
#define P2PNETWORK_H

#include <QObject>
#include <QString>
#include <QThread>
#include <memory>

namespace librats {
    class RatsClient;
}

/**
 * @brief P2PNetwork - Wrapper around librats RatsClient for Qt integration
 * 
 * Handles P2P networking using librats library, including:
 * - Peer discovery (DHT, mDNS)
 * - Torrent search in P2P network
 * - Torrent data exchange
 * - GossipSub pub-sub messaging
 */
class P2PNetwork : public QObject
{
    Q_OBJECT

public:
    explicit P2PNetwork(int port, const QString& dataDirectory, QObject *parent = nullptr);
    ~P2PNetwork();

    bool start();
    void stop();
    bool isRunning() const;
    bool isConnected() const;
    
    int getPeerCount() const;
    QString getOurPeerId() const;
    
    // Search torrents in P2P network
    void searchTorrents(const QString& query);
    
    // Announce torrent
    void announceTorrent(const QString& infoHash, const QString& name);

signals:
    void started();
    void stopped();
    void statusChanged(const QString& status);
    void peerCountChanged(int count);
    void peerConnected(const QString& peerId);
    void peerDisconnected(const QString& peerId);
    
    // Search results from other peers
    void searchResultReceived(const QString& infoHash, const QString& name, 
                             qint64 size, int seeders, int leechers);
    
    void error(const QString& errorMessage);

private slots:
    void updatePeerCount();

private:
    void setupCallbacks();
    void setupGossipSub();
    
    std::unique_ptr<librats::RatsClient> ratsClient_;
    int port_;
    QString dataDirectory_;
    bool running_;
    
    QTimer *updateTimer_;
};

#endif // P2PNETWORK_H

