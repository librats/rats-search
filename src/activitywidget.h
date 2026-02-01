#ifndef ACTIVITYWIDGET_H
#define ACTIVITYWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QQueue>
#include <QHash>
#include <QDateTime>
#include "searchresultmodel.h"

class RatsAPI;

/**
 * @brief ActivityWidget - Real-time display of new torrents being indexed
 * 
 * Shows torrents as they are added to the database in real-time.
 * Implements a display queue with artificial delay (850ms) to prevent
 * overwhelming the user when torrents arrive too fast.
 * 
 * Features:
 * - Real-time torrent display with controlled speed
 * - Pause/Continue button to stop/resume the display
 * - Queue counter showing pending torrents
 * - Maximum queue size to prevent memory issues
 * - Maximum display size to keep UI responsive
 * 
 * Migrated from: legacy/app/recent-torrents.js
 */
class ActivityWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ActivityWidget(QWidget *parent = nullptr);
    ~ActivityWidget();

    void setApi(RatsAPI* api);
    
    /**
     * @brief Get currently selected torrent (if any)
     * @return Selected torrent or invalid TorrentInfo if none selected
     */
    TorrentInfo getSelectedTorrent() const;

signals:
    void torrentSelected(const TorrentInfo& torrent);
    void torrentDoubleClicked(const TorrentInfo& torrent);
    void navigateToTop();  // Emitted when Top button is clicked

public slots:
    /**
     * @brief Toggle pause state
     */
    void togglePause();
    
    /**
     * @brief Handle new torrent indexed event
     */
    void onNewTorrent(const QString& hash, const QString& name);

private slots:
    void displayNextTorrent();
    void updateQueueCounter();
    void onItemClicked(QListWidgetItem* item);
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    void setupUi();
    void loadRecentTorrents();
    void addTorrentToDisplay(const TorrentInfo& torrent);
    QListWidgetItem* createTorrentItem(const TorrentInfo& torrent);
    void updatePauseButton();
    QString formatSize(qint64 bytes) const;
    QString formatDate(const QDateTime& dateTime) const;
    QString getContentTypeIcon(const QString& contentType) const;

    RatsAPI* api_ = nullptr;
    
    // UI components
    QListWidget* torrentList_;
    QPushButton* pauseButton_;
    QPushButton* topButton_;
    QLabel* titleLabel_;
    QLabel* queueLabel_;
    QLabel* statusLabel_;
    
    // Display queue mechanism (like legacy recent-torrents.js)
    QQueue<TorrentInfo> displayQueue_;
    QHash<QString, TorrentInfo> displayQueueAssoc_;  // For fast lookup by hash
    QHash<QString, TorrentInfo> displayedTorrents_;  // Currently displayed torrents
    
    QTimer* displayTimer_;      // Timer for controlled display speed
    QTimer* counterTimer_;      // Timer for queue counter updates
    
    // Configuration
    static const int MAX_QUEUE_SIZE = 1000;
    static const int MAX_DISPLAY_SIZE = 15;
    static const int DISPLAY_SPEED_MS = 850;     // Delay between displaying torrents
    static const int COUNTER_UPDATE_MS = 40;     // How often to update queue counter
    
    bool isPaused_ = false;
    bool isInitialized_ = false;
};

#endif // ACTIVITYWIDGET_H
