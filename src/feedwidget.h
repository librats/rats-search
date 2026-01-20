#ifndef FEEDWIDGET_H
#define FEEDWIDGET_H

#include <QWidget>
#include <QTableView>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include "searchresultmodel.h"

class RatsAPI;

/**
 * @brief FeedWidget - Widget displaying feed of voted/popular torrents
 * 
 * Shows torrents that have been voted on by the community.
 * Supports auto-loading more content on scroll (like legacy/app/feed.js)
 */
class FeedWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FeedWidget(QWidget *parent = nullptr);
    ~FeedWidget();

    void setApi(RatsAPI* api);

signals:
    void torrentSelected(const TorrentInfo& torrent);
    void torrentDoubleClicked(const TorrentInfo& torrent);

public slots:
    void refresh();
    void handleFeedUpdate(const QJsonArray& feed);

private slots:
    void onTorrentSelected(const QModelIndex& index);
    void onTorrentDoubleClicked(const QModelIndex& index);
    void onScrollValueChanged(int value);
    void loadMoreFeed();

private:
    void setupUi();

    RatsAPI* api_ = nullptr;
    
    // UI components
    QTableView* tableView_;
    SearchResultModel* model_;
    QLabel* statusLabel_;
    QLabel* emptyLabel_;
    
    // State
    QVector<TorrentInfo> feedTorrents_;
    bool loading_ = false;
    int loadedCount_ = 0;
};

#endif // FEEDWIDGET_H
