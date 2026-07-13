#ifndef TORRENTDETAILSPANEL_H
#define TORRENTDETAILSPANEL_H

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "domain/torrent.h"

namespace rats::app {
class Application;
}

/**
 * @brief Panel for displaying detailed torrent information
 * Similar to TorrentPage in legacy React app
 *
 * Features migrated from legacy:
 * - Voting (Good/Bad buttons)
 * - Download progress display
 *
 * Note: Files tree has been moved to TorrentFilesWidget (bottom panel)
 */
class TorrentDetailsPanel : public QWidget {
    Q_OBJECT

public:
    explicit TorrentDetailsPanel(QWidget* parent = nullptr);
    ~TorrentDetailsPanel();

    void setApplication(rats::app::Application* app);
    void setTorrent(const rats::domain::Torrent& torrent);
    void clear();
    QString currentHash() const { return currentHash_; }

    // Download progress
    void setDownloadProgress(double progress, qint64 downloaded, qint64 total, int speed);
    void setDownloadCompleted();
    void resetDownloadState();

signals:
    void downloadRequested(const QString& hash);
    void downloadCancelRequested(const QString& hash);
    void closeRequested();
    void goToDownloadsRequested();

public slots:
    void onVotesUpdated(const QString& hash, int good, int bad);

private slots:
    void onMagnetClicked();
    void onDownloadClicked();
    void onCopyHashClicked();
    void onGoodVoteClicked();
    void onBadVoteClicked();
    void onCancelDownloadClicked();
    void onFavoriteClicked();
    // Repository signalled that this torrent's row changed (tracker counts/info).
    void onTorrentUpdated(const QString& hash);

private:
    void setupUi();
    // Repaint the seeders/leechers/completed row after a tracker count scrape.
    void updateTrackerStats(int seeders, int leechers, int completed);
    void updateRatingDisplay();
    void updateVotingButtons();
    QString makeBreakable(const QString& text) const;

    void updateFavoriteButton();

    rats::app::Application* app_ = nullptr;

    // Header section
    QLabel* titleLabel_;
    QLabel* contentTypeLabel_;
    QWidget* contentTypeIcon_;

    // Info section
    QLabel* sizeLabel_;
    QLabel* filesLabel_;
    QLabel* dateLabel_;
    QLabel* hashLabel_;
    QLabel* categoryLabel_;

    // Stats section
    QLabel* seedersLabel_;
    QLabel* leechersLabel_;
    QLabel* completedLabel_;

    // Rating/Voting section
    QProgressBar* ratingBar_;
    QLabel* ratingLabel_;
    QPushButton* goodVoteButton_;
    QPushButton* badVoteButton_;
    QLabel* votesLabel_;

    // Download progress section
    QWidget* downloadProgressWidget_;
    QProgressBar* downloadProgressBar_;
    QLabel* downloadStatusLabel_;
    QLabel* downloadSpeedLabel_;
    QPushButton* cancelDownloadButton_;
    QPushButton* goToDownloadsButton_;

    // Actions
    QPushButton* magnetButton_;
    QPushButton* downloadButton_;
    QPushButton* favoriteButton_;
    QPushButton* copyHashButton_;
    QPushButton* closeButton_;

    // Tracker info scraping (descriptions/posters from tracker websites)
    void requestTrackerRefresh();
    void updateTrackerInfoDisplay(const QJsonObject& info);
    void loadPosterImage(const QString& url);

    // Tracker info UI elements
    QWidget* trackerInfoWidget_; // Container for all tracker info
    QLabel* trackerInfoLoadingLabel_; // "Loading tracker info..." indicator
    QLabel* posterLabel_; // Poster/cover image
    QLabel* descriptionLabel_; // Description text (expandable)
    QPushButton* descriptionToggle_; // "Show more / Show less" button
    QWidget* trackerLinksWidget_; // Container for tracker link buttons
    QHBoxLayout* trackerLinksLayout_; // Layout for tracker link buttons
    bool descriptionExpanded_ = false;
    QString fullDescription_; // Full description text
    QNetworkAccessManager* posterNetworkManager_;

    // Current torrent data
    QString currentHash_;
    rats::domain::Torrent currentTorrent_;
    bool isDownloading_ = false;
    bool hasVoted_ = false;
};

#endif // TORRENTDETAILSPANEL_H
