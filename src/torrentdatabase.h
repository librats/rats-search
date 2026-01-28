#ifndef TORRENTDATABASE_H
#define TORRENTDATABASE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QJsonObject>
#include <memory>

class ManticoreManager;
class SphinxQL;

/**
 * @brief TorrentFile - file within a torrent
 */
struct TorrentFile {
    QString path;
    qint64 size;
};

/**
 * @brief TorrentInfo - full torrent information
 * Matches the legacy database schema
 */
struct TorrentInfo {
    qint64 id = 0;
    QString hash;               // info_hash (40 char hex)
    QString name;
    QString nameIndex;          // Full-text search index
    qint64 size = 0;
    int files = 0;              // Number of files
    int piecelength = 0;
    QDateTime added;
    QString ipv4;               // IP that announced it
    int port = 0;
    int contentTypeId = 0;      // Content type ID (numeric)
    int contentCategoryId = 0;  // Content category ID (numeric)
    QString contentType;        // Content type string (video, audio, etc)
    QString contentCategory;    // Content category string (movie, xxx, etc)
    int seeders = 0;
    int leechers = 0;
    int completed = 0;
    QDateTime trackersChecked;
    int good = 0;               // Good votes
    int bad = 0;                // Bad votes
    QJsonObject info;           // Additional info (tracker data, etc)
    
    QVector<TorrentFile> filesList;
    
    // File search results (highlighted snippets from searchFiles)
    QStringList matchingPaths;   // Highlighted file path snippets with <b> tags
    bool isFileMatch = false;    // True if this result came from file search
    
    // Helper methods
    bool isValid() const { return !hash.isEmpty() && hash.length() == 40; }
    QString contentTypeString() const;
    QString contentCategoryString() const;
    void setContentTypeFromString(const QString& type);
    void setContentCategoryFromString(const QString& category);
};

/**
 * @brief Content type enumeration
 */
enum class ContentType {
    Unknown = 0,
    Video = 1,
    Audio = 2,
    Books = 3,
    Pictures = 4,
    Software = 5,
    Games = 6,
    Archive = 7,
    Bad = 100
};

/**
 * @brief Content category enumeration
 */
enum class ContentCategory {
    Unknown = 0,
    Movie = 1,
    Series = 2,
    Documentary = 3,
    Anime = 4,
    Music = 5,
    Ebook = 6,
    Comics = 7,
    Software = 8,
    Game = 9,
    XXX = 100
};

/**
 * @brief Search options for torrent queries
 */
struct SearchOptions {
    QString query;
    int index = 0;              // Offset
    int limit = 10;
    QString orderBy;
    bool orderDesc = true;
    bool safeSearch = false;
    QString contentType;        // Filter by content type
    qint64 sizeMin = 0;
    qint64 sizeMax = 0;
    int filesMin = 0;
    int filesMax = 0;
};

/**
 * @brief TorrentDatabase - Manticore Search based database for torrent indexing
 */
class TorrentDatabase : public QObject
{
    Q_OBJECT

public:
    explicit TorrentDatabase(const QString& dataDirectory, QObject *parent = nullptr);
    ~TorrentDatabase();

    /**
     * @brief Initialize the database
     * Starts Manticore and prepares tables
     */
    bool initialize();
    
    /**
     * @brief Close the database
     */
    void close();
    
    /**
     * @brief Check if database is ready
     */
    bool isReady() const;

    // =========================================================================
    // Torrent CRUD Operations
    // =========================================================================
    
    /**
     * @brief Add a torrent to the database
     */
    bool addTorrent(const TorrentInfo& torrent);
    
    /**
     * @brief Update an existing torrent
     */
    bool updateTorrent(const TorrentInfo& torrent);
    
    /**
     * @brief Remove a torrent from the database
     */
    bool removeTorrent(const QString& hash);
    
    /**
     * @brief Get torrent by hash
     * @param hash Info hash
     * @param includeFiles Whether to load file list
     */
    TorrentInfo getTorrent(const QString& hash, bool includeFiles = false);
    
    /**
     * @brief Check if torrent exists
     */
    bool hasTorrent(const QString& hash);

    // =========================================================================
    // Search Operations
    // =========================================================================
    
    /**
     * @brief Search torrents
     */
    QVector<TorrentInfo> searchTorrents(const SearchOptions& options);
    
    /**
     * @brief Search files within torrents
     */
    QVector<TorrentInfo> searchFiles(const SearchOptions& options);
    
    /**
     * @brief Get recent torrents
     */
    QVector<TorrentInfo> getRecentTorrents(int limit = 10);
    
    /**
     * @brief Get top torrents by seeders
     */
    QVector<TorrentInfo> getTopTorrents(const QString& type = "", 
                                         const QString& time = "", 
                                         int index = 0, 
                                         int limit = 20);
    
    /**
     * @brief Get random torrents for P2P replication
     */
    QVector<TorrentInfo> getRandomTorrents(int limit = 5);
    
    /**
     * @brief Insert a torrent (alias for addTorrent)
     */
    bool insertTorrent(const TorrentInfo& torrent) { return addTorrent(torrent); }

    // =========================================================================
    // Tracker Operations
    // =========================================================================
    
    /**
     * @brief Update torrent tracker information
     */
    bool updateTrackerInfo(const QString& hash, int seeders, int leechers, int completed);

    // =========================================================================
    // Statistics
    // =========================================================================
    
    struct Statistics {
        qint64 totalTorrents = 0;
        qint64 totalFiles = 0;
        qint64 totalSize = 0;
    };
    
    /**
     * @brief Get database statistics (from cache, no DB query)
     */
    Statistics getStatistics() const;
    
    /**
     * @brief Get cached statistics (same as getStatistics, for clarity)
     */
    const Statistics& cachedStatistics() const { return currentStats_; }
    
    /**
     * @brief Get total torrent count
     */
    qint64 getTorrentCount() const;
    
    /**
     * @brief Get next available ID for torrents table
     */
    qint64 getNextTorrentId();
    
    /**
     * @brief Get next available ID for files table
     */
    qint64 getNextFilesId();

    // =========================================================================
    // Content Type Helpers
    // =========================================================================
    
    static int contentTypeId(const QString& type);
    static QString contentTypeFromId(int id);
    static int contentCategoryId(const QString& category);
    static QString contentCategoryFromId(int id);
    
    /**
     * @brief Detect content type and category from torrent name and files
     */
    static void detectContentType(TorrentInfo& torrent);
    
    /**
     * @brief Build search index from torrent name and info
     */
    static QString buildNameIndex(const TorrentInfo& torrent);

    // =========================================================================
    // Maintenance
    // =========================================================================
    
    /**
     * @brief Optimize database indexes
     */
    bool optimize();
    
    /**
     * @brief Access to SphinxQL interface
     */
    SphinxQL* sphinxQL() const { return sphinxQL_.get(); }
    
    /**
     * @brief Access to ManticoreManager
     */
    ManticoreManager* manager() const { return manager_.get(); }

signals:
    void torrentAdded(const QString& hash);
    void torrentUpdated(const QString& hash);
    void torrentRemoved(const QString& hash);
    void statisticsChanged(qint64 torrents, qint64 files, qint64 totalSize);
    void databaseError(const QString& error);
    void ready();

private:
    TorrentInfo rowToTorrent(const QVariantMap& row);
    QVector<TorrentFile> getFilesForTorrent(const QString& hash);
    bool addFilesToDatabase(const TorrentInfo& torrent);
    
    QString dataDirectory_;
    std::unique_ptr<ManticoreManager> manager_;
    std::unique_ptr<SphinxQL> sphinxQL_;
    qint64 nextTorrentId_ = 1;
    qint64 nextFilesId_ = 1;
    
    // Cached statistics (updated incrementally like legacy spider.js)
    Statistics currentStats_;
};

#endif // TORRENTDATABASE_H
