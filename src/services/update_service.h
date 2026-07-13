#ifndef RATS_SERVICE_UPDATE_SERVICE_H
#define RATS_SERVICE_UPDATE_SERVICE_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <QVersionNumber>
#include <memory>

namespace rats::service {

// Manages application updates by checking GitHub releases.
//
// Drives the full update state machine: checking for a new version, downloading
// the release asset, extracting it, generating the platform updater script and
// relaunching. The batch/shell scripts live in Qt resources
// (":/update/windows_updater.bat.in", ":/update/unix_updater.sh.in").
class UpdateService : public QObject {
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

    explicit UpdateService(QObject* parent = nullptr);
    ~UpdateService();

    // Current application version
    static QString currentVersion();
    static QVersionNumber currentVersionNumber();

    // GitHub repository info
    void setRepository(const QString& owner, const QString& repo);

    // Check for updates
    void checkForUpdates();

    // Download the update
    void downloadUpdate();

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

    // Settings. Whether to check on startup is ConfigStore's
    // `checkUpdatesOnStartup`, not a copy held here.
    void setIncludePrerelease(bool include) { includePrerelease_ = include; }
    bool includePrerelease() const { return includePrerelease_; }

signals:
    void stateChanged(UpdateState state);
    void updateAvailable(const UpdateInfo& info);
    void noUpdateAvailable();
    void downloadProgressChanged(int percent);
    void updateReady();
    void errorOccurred(const QString& error);

private slots:
    void onCheckReplyFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    // Extract the downloaded archive and prepare the updater script.
    void applyUpdate();

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

    bool includePrerelease_;
};

} // namespace rats::service

#endif // RATS_SERVICE_UPDATE_SERVICE_H
