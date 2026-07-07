#ifndef FAVORITESWIDGET_H
#define FAVORITESWIDGET_H

#include "torrenttablewidget.h"

class QLabel;
class QPushButton;
class QMenu;

/**
 * @brief FavoritesWidget - user's bookmarked torrents.
 *
 * Thin subclass of TorrentTableWidget: loads via app_->favorites()->favorites(),
 * refreshes on the FavoritesStore favoritesChanged signal and adds a
 * "Remove from Favorites" action (context menu + toolbar button).
 */
class FavoritesWidget : public TorrentTableWidget {
    Q_OBJECT

public:
    explicit FavoritesWidget(QWidget* parent = nullptr);

protected:
    void refresh() override;
    void onApplicationSet() override;
    void onSelectionChanged(const rats::domain::Torrent& torrent, bool valid) override;
    QMenu* buildContextMenu(const rats::domain::Torrent& torrent) override;

private slots:
    void onRemoveClicked();

private:
    void setupControls();
    void removeFavorite(const QString& hash);

    QLabel* statusLabel_ = nullptr;
    QLabel* emptyLabel_ = nullptr;
    QPushButton* removeButton_ = nullptr;
};

#endif // FAVORITESWIDGET_H
