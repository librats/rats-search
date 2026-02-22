#ifndef FAVORITESWIDGET_H
#define FAVORITESWIDGET_H

#include <QWidget>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include "searchresultmodel.h"

class FavoritesManager;

/**
 * @brief FavoritesWidget - Tab widget displaying user's favorite torrents
 * 
 * Shows all torrents that the user has added to their personal favorites.
 * Provides ability to remove torrents from favorites via context menu
 * or a remove button.
 */
class FavoritesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FavoritesWidget(QWidget *parent = nullptr);
    ~FavoritesWidget();

    void setFavoritesManager(FavoritesManager* manager);
    
    /**
     * @brief Get currently selected torrent (if any)
     * @return Selected torrent or invalid TorrentInfo if none selected
     */
    TorrentInfo getSelectedTorrent() const;

signals:
    void torrentSelected(const TorrentInfo& torrent);
    void torrentDoubleClicked(const TorrentInfo& torrent);
    void removeFromFavoritesRequested(const QString& hash);

public slots:
    void refresh();

private slots:
    void onTorrentSelected(const QModelIndex& index);
    void onTorrentDoubleClicked(const QModelIndex& index);
    void onContextMenu(const QPoint& pos);
    void onRemoveClicked();

private:
    void setupUi();

    FavoritesManager* favoritesManager_ = nullptr;
    
    // UI components
    QTableView* tableView_;
    SearchResultModel* model_;
    QLabel* statusLabel_;
    QLabel* emptyLabel_;
    QPushButton* removeButton_;
};

#endif // FAVORITESWIDGET_H
