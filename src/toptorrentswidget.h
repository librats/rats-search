#ifndef TOPTORRENTSWIDGET_H
#define TOPTORRENTSWIDGET_H

#include <QWidget>
#include <QTabBar>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QHash>
#include <QVector>
#include "searchresultmodel.h"

class RatsAPI;

/**
 * @brief TopTorrentsWidget - Widget displaying top torrents by category and time
 * 
 * Provides tabbed view of top torrents similar to legacy/app/top-page.js
 * Categories: All, Video, Audio, Books, Pictures, Apps, Archives
 * Time filters: Overall, Last hour, Last week, Last month
 */
class TopTorrentsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TopTorrentsWidget(QWidget *parent = nullptr);
    ~TopTorrentsWidget();

    void setApi(RatsAPI* api);

signals:
    void torrentSelected(const TorrentInfo& torrent);
    void torrentDoubleClicked(const TorrentInfo& torrent);

public slots:
    void refresh();
    void handleRemoteTopTorrents(const QJsonArray& torrents, const QString& type, const QString& time);

private slots:
    void onCategoryChanged(int index);
    void onTimeFilterChanged(int index);
    void onTorrentSelected(const QModelIndex& index);
    void onTorrentDoubleClicked(const QModelIndex& index);
    void onMoreTorrentsClicked();

private:
    void setupUi();
    void loadTopTorrents(const QString& category, const QString& time);
    void mergeTorrents(const QVector<TorrentInfo>& torrents, const QString& category, const QString& time);
    QString getCacheKey(const QString& category, const QString& time) const;

    RatsAPI* api_ = nullptr;
    
    // UI components
    QTabBar* categoryTabs_;
    QComboBox* timeFilter_;
    QTableView* tableView_;
    SearchResultModel* model_;
    QPushButton* moreButton_;
    QLabel* statusLabel_;
    
    // Categories
    QStringList categories_;
    QHash<QString, QString> categoryLabels_;
    
    // Time filters
    QStringList timeFilters_;
    QHash<QString, QString> timeLabels_;
    
    // Cache: key = "category:time", value = {torrents, page}
    struct CacheEntry {
        QVector<TorrentInfo> torrents;
        int page = 0;
    };
    QHash<QString, CacheEntry> cache_;
    
    QString currentCategory_;
    QString currentTime_;
};

#endif // TOPTORRENTSWIDGET_H
