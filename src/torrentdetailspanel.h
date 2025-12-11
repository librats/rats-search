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
#include "torrentdatabase.h"

/**
 * @brief Panel for displaying detailed torrent information
 * Similar to TorrentPage in legacy React app
 */
class TorrentDetailsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TorrentDetailsPanel(QWidget *parent = nullptr);
    ~TorrentDetailsPanel();
    
    void setTorrent(const TorrentInfo &torrent);
    void clear();
    bool isEmpty() const { return currentHash_.isEmpty(); }
    QString currentHash() const { return currentHash_; }

signals:
    void magnetLinkRequested(const QString &hash, const QString &name);
    void downloadRequested(const QString &hash);
    void closeRequested();

private slots:
    void onMagnetClicked();
    void onDownloadClicked();
    void onCopyHashClicked();

private:
    void setupUi();
    void updateRatingDisplay();
    QString formatBytes(qint64 bytes) const;
    QString formatDate(qint64 timestamp) const;
    QWidget* createInfoRow(const QString &label, const QString &value, 
                           const QColor &valueColor = QColor());
    QWidget* createActionButton(const QString &text, const QString &iconPath,
                                const QColor &bgColor);
    
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
    
    // Rating section
    QProgressBar *ratingBar_;
    QLabel *ratingLabel_;
    
    // Actions
    QPushButton *magnetButton_;
    QPushButton *downloadButton_;
    QPushButton *copyHashButton_;
    QPushButton *closeButton_;
    
    // Current torrent data
    QString currentHash_;
    TorrentInfo currentTorrent_;
};

#endif // TORRENTDETAILSPANEL_H

