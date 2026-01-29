#ifndef TORRENTDETAILSPANEL_H
#define TORRENTDETAILSPANEL_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QProgressBar>
#include <QScrollArea>
#include <QJsonObject>
#include <QJsonArray>
#include "torrentdatabase.h"

// Forward declarations
class RatsAPI;
class TorrentClient;

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
class TorrentDetailsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TorrentDetailsPanel(QWidget *parent = nullptr);
    ~TorrentDetailsPanel();
    
    void setTorrent(const TorrentInfo &torrent);
    void setApi(RatsAPI* api);
    void setTorrentClient(TorrentClient* client);
    void clear();
    bool isEmpty() const { return currentHash_.isEmpty(); }
    QString currentHash() const { return currentHash_; }
    TorrentInfo currentTorrent() const { return currentTorrent_; }
    
    // Download progress
    void setDownloadProgress(double progress, qint64 downloaded, qint64 total, int speed);
    void setDownloadCompleted();
    void resetDownloadState();

signals:
    void magnetLinkRequested(const QString &hash, const QString &name);
    void downloadRequested(const QString &hash);
    void downloadCancelRequested(const QString &hash);
    void voteRequested(const QString &hash, bool isGood);
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

private:
    void setupUi();
    void updateRatingDisplay();
    void updateVotingButtons();
    QString formatBytes(qint64 bytes) const;
    QString formatDate(qint64 timestamp) const;
    QString formatSpeed(int bytesPerSec) const;
    
    RatsAPI* api_ = nullptr;
    TorrentClient* torrentClient_ = nullptr;
    
    // Header section
    QLabel *titleLabel_;
    QLabel *contentTypeLabel_;
    QWidget *contentTypeIcon_;
    
    // Info section
    QLabel *sizeLabel_;
    QLabel *filesLabel_;
    QLabel *dateLabel_;
    QLabel *hashLabel_;
    QLabel *categoryLabel_;
    
    // Stats section
    QLabel *seedersLabel_;
    QLabel *leechersLabel_;
    QLabel *completedLabel_;
    
    // Rating/Voting section
    QProgressBar *ratingBar_;
    QLabel *ratingLabel_;
    QPushButton *goodVoteButton_;
    QPushButton *badVoteButton_;
    QLabel *votesLabel_;
    
    // Download progress section
    QWidget *downloadProgressWidget_;
    QProgressBar *downloadProgressBar_;
    QLabel *downloadStatusLabel_;
    QLabel *downloadSpeedLabel_;
    QPushButton *cancelDownloadButton_;
    QPushButton *goToDownloadsButton_;
    
    // Actions
    QPushButton *magnetButton_;
    QPushButton *downloadButton_;
    QPushButton *copyHashButton_;
    QPushButton *closeButton_;
    
    // Current torrent data
    QString currentHash_;
    TorrentInfo currentTorrent_;
    bool isDownloading_ = false;
    bool hasVoted_ = false;
};

#endif // TORRENTDETAILSPANEL_H
