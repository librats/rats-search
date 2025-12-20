#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVersionNumber>
#include <QTemporaryDir>
#include <memory>

/**
 * @brief Manages application updates by checking GitHub releases
 * 
 * This class handles:
 * - Checking for new versions from GitHub releases
 * - Downloading update packages
 * - Extracting and applying updates
 * - Restarting the application after update
 */
class UpdateManager : public QObject
{
    Q_OBJECT

public:
    struct UpdateInfo {
        QString version;
        QString downloadUrl;
        QString releaseNotes;
        qint64 downloadSize;
        QString publishedAt;
        bool isPrerelease;
        
        bool isValid() const { return !version.isEmpty() && !downloadUrl.isEmpty(); }
    };
    
    enum class UpdateState {
        Idle,
        CheckingForUpdates,
        UpdateAvailable,
        Downloading,
        Extracting,
        ReadyToInstall,
        Installing,
        Error
    };
    Q_ENUM(UpdateState)

    explicit UpdateManager(QObject *parent = nullptr);
    ~UpdateManager();

    // Current application version
    static QString currentVersion();
    static QVersionNumber currentVersionNumber();
    
    // GitHub repository info
    void setRepository(const QString& owner, const QString& repo);
    
    // Check for updates
    void checkForUpdates();
    
    // Download the update
    void downloadUpdate();
    
    // Apply the update (extract and prepare for restart)
    void applyUpdate();
    
    // Execute the update script (closes app and applies update)
    Q_INVOKABLE void executeUpdateScript();
    
    // Get current state
    UpdateState state() const { return state_; }
    QString stateString() const;
    
    // Get update info (valid after checkForUpdates succeeds)
    const UpdateInfo& updateInfo() const { return updateInfo_; }
    
    // Get download progress (0-100)
    int downloadProgress() const { return downloadProgress_; }
    
    // Get error message
    QString errorMessage() const { return errorMessage_; }
    
    // Check if update is available
    bool isUpdateAvailable() const;
    
    // Settings
    void setCheckOnStartup(bool check) { checkOnStartup_ = check; }
    bool checkOnStartup() const { return checkOnStartup_; }
    
    void setIncludePrerelease(bool include) { includePrerelease_ = include; }
    bool includePrerelease() const { return includePrerelease_; }

signals:
    void stateChanged(UpdateState state);
    void updateAvailable(const UpdateInfo& info);
    void noUpdateAvailable();
    void downloadProgressChanged(int percent);
    void downloadComplete();
    void extractionComplete();
    void updateReady();
    void errorOccurred(const QString& error);
    void checkComplete();

private slots:
    void onCheckReplyFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    void setState(UpdateState state);
    void setError(const QString& error);
    QString getPlatformAssetName() const;
    bool extractZipFile(const QString& zipPath, const QString& destPath);
    bool createUpdateScript(const QString& updateDir);
    QString getApplicationDir() const;
    
    QNetworkAccessManager* networkManager_;
    QNetworkReply* currentReply_;
    
    QString repoOwner_;
    QString repoName_;
    
    UpdateState state_;
    UpdateInfo updateInfo_;
    QString errorMessage_;
    
    int downloadProgress_;
    QString downloadedFilePath_;
    std::unique_ptr<QTemporaryDir> tempDir_;
    
    bool checkOnStartup_;
    bool includePrerelease_;
};

#endif // UPDATEMANAGER_H

