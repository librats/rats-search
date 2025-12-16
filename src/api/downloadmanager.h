#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QHash>
#include <memory>

// Forward declarations
class TorrentClient;
class TorrentDatabase;

/**
 * @brief DownloadFile - A file within a download
 */
struct DownloadFile {
    QString path;
    qint64 size = 0;
    int index = 0;
    bool selected = true;
    
    QJsonObject toJson() const;
};

/**
 * @brief DownloadInfo - Information about an active download
 */
struct DownloadInfo {
    QString hash;
    QString name;
    qint64 size = 0;
    QString savePath;
    
    // Progress
    qint64 received = 0;
    qint64 downloaded = 0;
    double progress = 0.0;
    int downloadSpeed = 0;
    qint64 timeRemaining = 0;
    
    // State
    bool paused = false;
    bool removeOnDone = false;
    bool ready = false;  // Metadata received
    bool completed = false;
    
    // Files
    QVector<DownloadFile> files;
    
    QJsonObject toJson() const;
    QJsonObject toProgressJson() const;
};

/**
 * @brief DownloadManager - Manages torrent downloads
 * 
 * Provides high-level download management:
 * - Add/cancel downloads
 * - Pause/resume
 * - File selection
 * - Progress tracking
 * - Integration with TorrentClient
 * 
 * This class bridges the gap between the API layer and the
 * underlying torrent client implementation.
 */
class DownloadManager : public QObject
{
    Q_OBJECT

public:
    explicit DownloadManager(TorrentClient* client, 
                            TorrentDatabase* database,
                            QObject *parent = nullptr);
    ~DownloadManager();
    
    /**
     * @brief Set default download path
     */
    void setDownloadPath(const QString& path);
    
    /**
     * @brief Get default download path
     */
    QString downloadPath() const;
    
    // =========================================================================
    // Download Operations
    // =========================================================================
    
    /**
     * @brief Add a download by hash
     * @param hash Torrent info hash
     * @param savePath Optional save path (uses default if empty)
     * @return true if download was started
     */
    bool add(const QString& hash, const QString& savePath = QString());
    
    /**
     * @brief Add a download with torrent data
     * @param hash Torrent info hash
     * @param name Torrent name
     * @param size Torrent size
     * @param savePath Optional save path
     * @return true if download was started
     */
    bool addWithInfo(const QString& hash, 
                     const QString& name,
                     qint64 size,
                     const QString& savePath = QString());
    
    /**
     * @brief Cancel and remove a download
     * @param hash Torrent info hash
     * @return true if download was cancelled
     */
    bool cancel(const QString& hash);
    
    /**
     * @brief Pause a download
     */
    bool pause(const QString& hash);
    
    /**
     * @brief Resume a download
     */
    bool resume(const QString& hash);
    
    /**
     * @brief Toggle pause state
     */
    bool togglePause(const QString& hash);
    
    /**
     * @brief Set remove-on-done flag
     */
    bool setRemoveOnDone(const QString& hash, bool remove);
    
    /**
     * @brief Toggle remove-on-done flag
     */
    bool toggleRemoveOnDone(const QString& hash);
    
    /**
     * @brief Select files for download
     * @param hash Torrent info hash
     * @param selection Array of booleans or map of {index: selected}
     */
    bool selectFiles(const QString& hash, const QJsonValue& selection);
    
    // =========================================================================
    // Query Operations
    // =========================================================================
    
    /**
     * @brief Check if a hash is being downloaded
     */
    bool isDownloading(const QString& hash) const;
    
    /**
     * @brief Get download info
     */
    DownloadInfo getDownload(const QString& hash) const;
    
    /**
     * @brief Get all downloads
     */
    QVector<DownloadInfo> getAllDownloads() const;
    
    /**
     * @brief Get all downloads as JSON
     */
    QJsonArray toJsonArray() const;
    
    /**
     * @brief Get count of active downloads
     */
    int count() const;
    
signals:
    /**
     * @brief Emitted when download starts (ready with metadata)
     */
    void downloadStarted(const QString& hash);
    
    /**
     * @brief Emitted on progress updates
     */
    void progressUpdated(const QString& hash, const QJsonObject& progress);
    
    /**
     * @brief Emitted when files are ready for selection
     */
    void filesReady(const QString& hash, const QJsonArray& files);
    
    /**
     * @brief Emitted when download completes
     */
    void downloadCompleted(const QString& hash);
    
    /**
     * @brief Emitted when download is cancelled
     */
    void downloadCancelled(const QString& hash);
    
    /**
     * @brief Emitted when download state changes (pause/resume)
     */
    void stateChanged(const QString& hash, const QJsonObject& state);
    
    /**
     * @brief Emitted on error
     */
    void downloadError(const QString& hash, const QString& error);

private slots:
    void onTorrentReady(const QString& hash);
    void onTorrentProgress(const QString& hash, qint64 downloaded, qint64 total, int speed);
    void onTorrentCompleted(const QString& hash);
    void onTorrentError(const QString& hash, const QString& error);

private:
    void connectClientSignals();
    
    TorrentClient* client_;
    TorrentDatabase* database_;
    QString downloadPath_;
    QHash<QString, DownloadInfo> downloads_;
};

#endif // DOWNLOADMANAGER_H

