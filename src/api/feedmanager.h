#ifndef FEEDMANAGER_H
#define FEEDMANAGER_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QVector>

class TorrentDatabase;

/**
 * @brief TorrentFeedFile - A file in a feed torrent
 */
struct TorrentFeedFile {
    QString path;
    qint64 size = 0;
    
    QJsonObject toJson() const;
    static TorrentFeedFile fromJson(const QJsonObject& obj);
};

/**
 * @brief TorrentFeedItem - A torrent in the feed
 */
struct TorrentFeedItem {
    QString hash;
    QString name;
    qint64 size = 0;
    int files = 0;
    QString contentType;
    QString contentCategory;
    int seeders = 0;
    int good = 0;
    int bad = 0;
    qint64 feedDate = 0;  // When added to feed
    QVector<TorrentFeedFile> filesList;  // Files for P2P replication
    
    QJsonObject toJson() const;
    static TorrentFeedItem fromJson(const QJsonObject& obj);
};

/**
 * @brief FeedManager - Manages the voted torrents feed
 * 
 * The feed is a ranked list of torrents that have been voted on.
 * Ranking is based on:
 * - Recency (newer items rank higher)
 * - Good votes (increase rank)
 * - Bad votes (decrease rank)
 * 
 * Features:
 * - Automatic ranking and ordering
 * - Persistence to database
 * - Maximum feed size limit
 * - P2P feed synchronization support
 */
class FeedManager : public QObject
{
    Q_OBJECT

public:
    explicit FeedManager(TorrentDatabase* database, QObject *parent = nullptr);
    ~FeedManager();
    
    /**
     * @brief Load feed from database
     */
    bool load();
    
    /**
     * @brief Save feed to database
     */
    bool save();
    
    /**
     * @brief Clear the feed
     */
    void clear();
    
    /**
     * @brief Get feed size
     */
    int size() const;
    
    /**
     * @brief Get maximum feed size
     */
    int maxSize() const;
    
    /**
     * @brief Set maximum feed size
     */
    void setMaxSize(int max);
    
    /**
     * @brief Get feed date (last update timestamp)
     */
    qint64 feedDate() const;
    
    /**
     * @brief Add or update a torrent in the feed
     */
    void add(const TorrentFeedItem& item);
    
    /**
     * @brief Add a torrent by hash (will be looked up in database)
     */
    void addByHash(const QString& hash);
    
    /**
     * @brief Get feed items
     * @param index Offset
     * @param limit Number of items (default: 20)
     */
    QVector<TorrentFeedItem> getFeed(int index = 0, int limit = 20) const;
    
    /**
     * @brief Get all feed items
     */
    QVector<TorrentFeedItem> allItems() const;
    
    /**
     * @brief Get feed as JSON array
     */
    QJsonArray toJsonArray(int index = 0, int limit = 20) const;
    
    /**
     * @brief Replace feed from JSON (for P2P sync)
     */
    void fromJsonArray(const QJsonArray& array, qint64 remoteFeedDate = 0);
    
    /**
     * @brief Check if feed contains a hash
     */
    bool contains(const QString& hash) const;
    
    /**
     * @brief Get item by hash
     */
    TorrentFeedItem getItem(const QString& hash) const;
    
signals:
    /**
     * @brief Emitted when feed is updated
     */
    void feedUpdated();
    
    /**
     * @brief Emitted when an item is added
     */
    void itemAdded(const QString& hash);

private:
    /**
     * @brief Sort feed by ranking
     */
    void reorder();
    
    /**
     * @brief Calculate ranking score for an item
     */
    double calculateScore(const TorrentFeedItem& item) const;
    
    TorrentDatabase* database_;
    QVector<TorrentFeedItem> feed_;
    int maxSize_ = 1000;
    qint64 feedDate_ = 0;
    bool loaded_ = false;
};

#endif // FEEDMANAGER_H

