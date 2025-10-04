#ifndef TORRENTDATABASE_H
#define TORRENTDATABASE_H

#include <QObject>
#include <QString>
#include <QSqlDatabase>
#include <QVector>
#include <QDateTime>

struct TorrentInfo {
    QString infoHash;
    QString name;
    qint64 size;
    int seeders;
    int leechers;
    QDateTime indexedDate;
    QString category;
    QString description;
    QStringList files;
};

/**
 * @brief TorrentDatabase - SQLite database for torrent indexing
 */
class TorrentDatabase : public QObject
{
    Q_OBJECT

public:
    explicit TorrentDatabase(const QString& dataDirectory, QObject *parent = nullptr);
    ~TorrentDatabase();

    bool initialize();
    void close();
    
    // Torrent operations
    bool addTorrent(const TorrentInfo& torrent);
    bool updateTorrent(const TorrentInfo& torrent);
    bool removeTorrent(const QString& infoHash);
    
    TorrentInfo getTorrent(const QString& infoHash);
    QVector<TorrentInfo> searchTorrents(const QString& query, int limit = 100);
    QVector<TorrentInfo> getRecentTorrents(int limit = 50);
    QVector<TorrentInfo> getTopTorrents(int limit = 50);
    
    int getTorrentCount() const;
    qint64 getDatabaseSize() const;
    
    // Statistics
    struct Statistics {
        int totalTorrents;
        qint64 totalSize;
        int torrentsToday;
        int torrentsWeek;
        QMap<QString, int> categoryStats;
    };
    
    Statistics getStatistics() const;

signals:
    void torrentAdded(const QString& infoHash);
    void torrentUpdated(const QString& infoHash);
    void torrentRemoved(const QString& infoHash);
    void databaseError(const QString& error);

private:
    bool createTables();
    bool upgradeSchema();
    QString getDatabasePath() const;
    
    QSqlDatabase db_;
    QString dataDirectory_;
};

#endif // TORRENTDATABASE_H

