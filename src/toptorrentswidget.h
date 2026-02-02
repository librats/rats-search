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
#include <QDateTime>
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
    
    /**
     * @brief Get currently selected torrent (if any)
     * @return Selected torrent or invalid TorrentInfo if none selected
     */
    TorrentInfo getSelectedTorrent() const;
    
    /**
     * @brief Invalidate cache entries, forcing refresh on next view
     * @param category If specified, only invalidate this category (empty = all)
     */
    void invalidateCache(const QString& category = QString());
    
    /**
     * @brief Called when widget becomes visible (tab switched to)
     * Refreshes data if cache is stale
     */
    void onTabBecameVisible();
    
    /**
     * @brief Set cache TTL (time to live) in seconds
     * @param seconds Cache lifetime, default 300 (5 minutes)
     */
    void setCacheTTL(int seconds) { cacheTTL_ = seconds; }
    
    /**
     * @brief Get current cache TTL
     */
    int cacheTTL() const { return cacheTTL_; }

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
    
    // Cache: key = "category:time", value = {torrents, page, timestamp}
    struct CacheEntry {
        QVector<TorrentInfo> torrents;
        int page = 0;
        QDateTime lastUpdated;
        bool invalidated = false;  // Force refresh even if not expired
    };
    QHash<QString, CacheEntry> cache_;
    
    /**
     * @brief Check if cache entry is stale (expired or invalidated)
     */
    bool isCacheStale(const QString& key) const;
    
    QString currentCategory_;
    QString currentTime_;
    int cacheTTL_ = 300;  // Cache TTL in seconds (default 5 minutes)
};

#endif // TOPTORRENTSWIDGET_H
