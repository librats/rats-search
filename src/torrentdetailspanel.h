#ifndef TORRENTDETAILSPANEL_H
#define TORRENTDETAILSPANEL_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QProgressBar>
#include <QTreeWidget>
#include <QScrollArea>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include "torrentdatabase.h"

// Forward declarations
class RatsAPI;

/**
 * @brief Panel for displaying detailed torrent information
 * Similar to TorrentPage in legacy React app
 * 
 * Features migrated from legacy:
 * - Voting (Good/Bad buttons)
 * - Files tree with selection
 * - Download progress display
 */
class TorrentDetailsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TorrentDetailsPanel(QWidget *parent = nullptr);
    ~TorrentDetailsPanel();
    
    void setTorrent(const TorrentInfo &torrent);
    void setApi(RatsAPI* api);
    void clear();
    bool isEmpty() const { return currentHash_.isEmpty(); }
    QString currentHash() const { return currentHash_; }
    
    // File tree methods
    void setFiles(const QJsonArray& files);
    QList<int> getSelectedFileIndices() const;
    
    // Download progress
    void setDownloadProgress(double progress, qint64 downloaded, qint64 total, int speed);
    void setDownloadCompleted();
    void resetDownloadState();

signals:
    void magnetLinkRequested(const QString &hash, const QString &name);
    void downloadRequested(const QString &hash);
    void downloadCancelRequested(const QString &hash);
    void fileSelectionChanged(const QString &hash, const QList<int> &selectedIndices);
    void voteRequested(const QString &hash, bool isGood);
    void closeRequested();

public slots:
    void onVotesUpdated(const QString& hash, int good, int bad);

private slots:
    void onMagnetClicked();
    void onDownloadClicked();
    void onCopyHashClicked();
    void onGoodVoteClicked();
    void onBadVoteClicked();
    void onCancelDownloadClicked();
    void onFileItemChanged(QTreeWidgetItem* item, int column);

private:
    void setupUi();
    void updateRatingDisplay();
    void updateVotingButtons();
    QString formatBytes(qint64 bytes) const;
    QString formatDate(qint64 timestamp) const;
    QString formatSpeed(int bytesPerSec) const;
    QWidget* createInfoRow(const QString &label, const QString &value, 
                           const QColor &valueColor = QColor());
    QWidget* createActionButton(const QString &text, const QString &iconPath,
                                const QColor &bgColor);
    
    // File tree helpers (migrated from legacy/app/torrent-page.js buildFilesTree)
    struct FileTreeNode {
        QString name;
        qint64 size = 0;
        bool isFile = false;
        int fileIndex = -1;
        QMap<QString, FileTreeNode*> children;
        ~FileTreeNode() { qDeleteAll(children); }
    };
    FileTreeNode* buildFileTree(const QJsonArray& files);
    void addTreeNodeToWidget(FileTreeNode* node, QTreeWidgetItem* parent);
    QString getFileTypeIcon(const QString& filename) const;
    void collectSelectedFiles(QTreeWidgetItem* item, QList<int>& indices) const;
    
    RatsAPI* api_ = nullptr;
    
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
    
    // Files tree
    QTreeWidget *filesTree_;
    QLabel *filesTreeTitle_;
    
    // Download progress section
    QWidget *downloadProgressWidget_;
    QProgressBar *downloadProgressBar_;
    QLabel *downloadStatusLabel_;
    QLabel *downloadSpeedLabel_;
    QPushButton *cancelDownloadButton_;
    
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

