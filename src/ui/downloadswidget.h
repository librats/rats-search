#ifndef DOWNLOADSWIDGET_H
#define DOWNLOADSWIDGET_H

#include <QHBoxLayout>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace rats::app {
class Application;
}

/**
 * @brief DownloadItemWidget - Individual download item with progress
 */
class DownloadItemWidget : public QWidget {
    Q_OBJECT

public:
    explicit DownloadItemWidget(const QString& hash, const QString& name, qint64 size, QWidget* parent = nullptr);

    QString hash() const { return hash_; }

    void updateProgress(qint64 downloaded, qint64 total, int speed, double progress);
    // Refresh the display name and size once a magnet's metadata arrives (the
    // item is first created with the info-hash as a placeholder name and size 0).
    void updateInfo(const QString& name, qint64 size);
    void setCompleted();
    void setPaused(bool paused);

signals:
    void pauseToggled(const QString& hash);
    void cancelRequested(const QString& hash);
    void openRequested(const QString& hash);

private:
    void setupUi(const QString& name, qint64 size);
    QString formatTime(qint64 seconds) const;

    QString hash_;
    QLabel* nameLabel_;
    QLabel* sizeLabel_;
    QLabel* statusLabel_;
    QLabel* speedLabel_;
    QProgressBar* progressBar_;
    QPushButton* pauseButton_;
    QPushButton* cancelButton_;
};

/**
 * @brief DownloadsWidget - Widget showing all active downloads
 *
 * Similar to legacy/app/download-page.js
 * Shows list of downloads with progress, speed, pause/cancel controls
 */
class DownloadsWidget : public QWidget {
    Q_OBJECT

public:
    explicit DownloadsWidget(QWidget* parent = nullptr);
    ~DownloadsWidget();

    void setApplication(rats::app::Application* app);

public slots:
    void refresh();

private slots:
    void onDownloadStarted(const QString& hash);
    void onProgressUpdated(const QString& hash, const QJsonObject& progress);
    void onDownloadCompleted(const QString& hash);
    void onDownloadCancelled(const QString& hash);
    void onPauseToggled(const QString& hash);
    void onCancelRequested(const QString& hash);
    void onOpenRequested(const QString& hash);

private:
    void setupUi();
    void loadDownloads();
    void addDownloadItem(const QString& hash, const QString& name, qint64 size);
    void removeDownloadItem(const QString& hash);

    rats::app::Application* app_ = nullptr;

    // UI components
    QVBoxLayout* listLayout_;
    QLabel* emptyLabel_;
    QLabel* statusLabel_;
    QWidget* listContainer_;

    // Download items by hash
    QHash<QString, DownloadItemWidget*> downloadItems_;
};

#endif // DOWNLOADSWIDGET_H
