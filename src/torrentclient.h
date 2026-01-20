#ifndef TORRENTCLIENT_H
#define TORRENTCLIENT_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QVector>
#include <QTimer>
#include <QMutex>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>

// Forward declarations
class P2PNetwork;
class TorrentDatabase;

namespace librats {
    class RatsClient;
    class TorrentDownload;
}

/**
 * @brief TorrentFileInfo - Information about a file in a torrent
 */
struct TorrentFileInfo {
    QString path;
    qint64 size;
    int index;
    bool selected = true;
    double progress = 0.0;
    
    QJsonObject toJson() const;
};

/**
 * @brief ActiveTorrent - State for an actively downloading torrent
 */
struct ActiveTorrent {
    QString hash;
    QString name;
    QString savePath;
    qint64 totalSize = 0;
    qint64 downloadedBytes = 0;
    double progress = 0.0;
    double downloadSpeed = 0.0;
    int peersConnected = 0;
    bool paused = false;
    bool removeOnDone = false;
    bool ready = false;           // Metadata received
    bool completed = false;
    QVector<TorrentFileInfo> files;
    
    // librats torrent reference
    std::shared_ptr<librats::TorrentDownload> download;
    
    QJsonObject toJson() const;
    QJsonObject toProgressJson() const;
};

/**
 * @brief TorrentClient - Torrent download client using librats::BitTorrentClient
 * 
 * This class provides torrent downloading functionality by integrating with
 * librats BitTorrent implementation. It handles:
 * - Adding torrents by magnet link or info hash
 * - Progress tracking and reporting
 * - Pause/resume
 * - File selection
 * - Session persistence (save/restore active downloads)
 */
class TorrentClient : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     */
    explicit TorrentClient(QObject *parent = nullptr);
    ~TorrentClient();

    /**
     * @brief Initialize the torrent client with P2P network
     * @param p2pNetwork P2P network instance (provides access to RatsClient)
     * @param database Optional torrent database for metadata lookup
     * @return true if initialization succeeded
     */
    bool initialize(P2PNetwork* p2pNetwork, TorrentDatabase* database = nullptr);

    /**
     * @brief Check if the client is ready (BitTorrent enabled)
     */
    bool isReady() const;

    /**
     * @brief Set the torrent database for metadata lookup
     */
    void setDatabase(TorrentDatabase* database);

    // =========================================================================
    // Download Management
    // =========================================================================

    /**
     * @brief Add a torrent for download
     * @param magnetLink Magnet link or info hash (40 char hex)
     * @param savePath Download directory (empty = default)
     * @return true if torrent was added successfully
     */
    bool downloadTorrent(const QString& magnetLink, const QString& savePath = QString());

    /**
     * @brief Add a torrent with known metadata
     * @param hash Info hash (40 char hex)
     * @param name Torrent name
     * @param size Total size in bytes
     * @param savePath Download directory (empty = default)
     * @return true if torrent was added successfully
     */
    bool downloadWithInfo(const QString& hash, const QString& name, qint64 size, 
                          const QString& savePath = QString());

    /**
     * @brief Add a torrent from a .torrent file
     * @param torrentFile Path to .torrent file
     * @param savePath Download directory
     * @return true if torrent was added successfully
     */
    bool downloadTorrentFile(const QString& torrentFile, const QString& savePath = QString());

    /**
     * @brief Stop and remove a torrent
     * @param infoHash Info hash of torrent to stop
     */
    void stopTorrent(const QString& infoHash);

    /**
     * @brief Pause a torrent download
     * @param infoHash Info hash of torrent to pause
     * @return true if torrent was paused
     */
    bool pauseTorrent(const QString& infoHash);

    /**
     * @brief Resume a paused torrent
     * @param infoHash Info hash of torrent to resume
     * @return true if torrent was resumed
     */
    bool resumeTorrent(const QString& infoHash);

    /**
     * @brief Toggle pause state
     * @param infoHash Info hash of torrent
     * @return true if state was toggled
     */
    bool togglePause(const QString& infoHash);

    /**
     * @brief Select which files to download
     * @param infoHash Info hash of torrent
     * @param fileSelection Array of booleans (true=download, false=skip)
     * @return true if selection was applied
     */
    bool selectFiles(const QString& infoHash, const QVector<bool>& fileSelection);

    /**
     * @brief Set remove-on-done flag
     * @param infoHash Info hash of torrent
     * @param removeOnDone Whether to remove after completion
     */
    void setRemoveOnDone(const QString& infoHash, bool removeOnDone);

    // =========================================================================
    // Query Methods
    // =========================================================================

    /**
     * @brief Check if a torrent is currently downloading
     */
    bool isDownloading(const QString& infoHash) const;

    /**
     * @brief Get active torrent information
     */
    ActiveTorrent getTorrent(const QString& infoHash) const;

    /**
     * @brief Get all active torrents
     */
    QVector<ActiveTorrent> getAllTorrents() const;

    /**
     * @brief Get count of active torrents
     */
    int count() const;

    // =========================================================================
    // JSON Serialization (for API compatibility)
    // =========================================================================

    /**
     * @brief Get all torrents as JSON array
     */
    QJsonArray toJsonArray() const;

    /**
     * @brief Select files for download using JSON
     * @param hash Info hash
     * @param selection JSON array of booleans or object with {index: selected}
     * @return true if selection was applied
     */
    bool selectFilesJson(const QString& hash, const QJsonValue& selection);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set default download path
     */
    void setDefaultDownloadPath(const QString& path);

    /**
     * @brief Get default download path
     */
    QString defaultDownloadPath() const;

    // =========================================================================
    // Session Persistence
    // =========================================================================

    /**
     * @brief Save current download session to file
     * @param filePath Path to save session file
     * @return true if saved successfully
     */
    bool saveSession(const QString& filePath);

    /**
     * @brief Restore downloads from session file
     * @param filePath Path to session file
     * @return Number of downloads restored
     */
    int loadSession(const QString& filePath);

signals:
    /**
     * @brief Emitted when a torrent starts downloading (metadata received)
     */
    void downloadStarted(const QString& infoHash);

    /**
     * @brief Emitted periodically with download progress
     */
    void downloadProgress(const QString& infoHash, int progressPercent);

    /**
     * @brief Emitted with detailed progress info
     */
    void progressUpdate(const QString& infoHash, qint64 downloaded, qint64 total, 
                        double speed, int peers);

    /**
     * @brief Emitted with progress as JSON (API compatible)
     */
    void progressUpdated(const QString& infoHash, const QJsonObject& progress);

    /**
     * @brief Emitted when file list is available (after metadata received)
     */
    void filesReady(const QString& infoHash, const QVector<TorrentFileInfo>& files);

    /**
     * @brief Emitted when file list is available as JSON (API compatible)
     */
    void filesReadyJson(const QString& infoHash, const QJsonArray& files);

    /**
     * @brief Emitted when a torrent completes downloading
     */
    void downloadCompleted(const QString& infoHash);

    /**
     * @brief Emitted when a torrent download fails
     */
    void downloadFailed(const QString& infoHash, const QString& error);

    /**
     * @brief Emitted when a torrent is removed/cancelled
     */
    void torrentRemoved(const QString& infoHash);

    /**
     * @brief Emitted when pause state changes
     */
    void pauseStateChanged(const QString& infoHash, bool paused);

    /**
     * @brief Emitted when download state changes (API compatible)
     */
    void stateChanged(const QString& infoHash, const QJsonObject& state);

private slots:
    void onUpdateTimer();

private:
    QString parseInfoHash(const QString& magnetLink) const;
    void setupTorrentCallbacks(const QString& hash, std::shared_ptr<librats::TorrentDownload> download);
    void updateTorrentStatus(const QString& hash);
    void emitProgressJson(const QString& hash, const ActiveTorrent& torrent);

    P2PNetwork* p2pNetwork_ = nullptr;
    TorrentDatabase* database_ = nullptr;
    QString defaultDownloadPath_;
    
    // Active torrents by info hash
    QHash<QString, ActiveTorrent> torrents_;
    mutable QMutex torrentsMutex_;
    
    // Update timer for progress polling
    QTimer* updateTimer_ = nullptr;
    
    bool initialized_ = false;
};

#endif // TORRENTCLIENT_H
