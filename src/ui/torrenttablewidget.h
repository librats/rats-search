#ifndef TORRENTTABLEWIDGET_H
#define TORRENTTABLEWIDGET_H

#include "domain/torrent.h"
#include <QVector>
#include <QWidget>

class QTableView;
class QVBoxLayout;
class QMenu;
class QModelIndex;
class SearchResultModel;

namespace rats::app {
class Application;
}

/**
 * @brief TorrentTableWidget - shared base for the torrent-table tabs.
 *
 * Factors out the QTableView + SearchResultModel + TorrentItemDelegate setup
 * (5 columns: Name/Size/Seeders/Leechers/Date), the selection / double-click
 * handlers and the standard context menu (Open Magnet / Copy Info Hash /
 * Copy Magnet / Export) shared by TopTorrentsWidget, FeedWidget and
 * FavoritesWidget.
 *
 * Subclasses only implement refresh() to load their data (and may add their own
 * chrome above/below the table via layout()).
 */
class TorrentTableWidget : public QWidget {
    Q_OBJECT

public:
    explicit TorrentTableWidget(QWidget* parent = nullptr);
    ~TorrentTableWidget() override;

    /**
     * @brief Provide the running application. Stores it, then calls
     *        onApplicationSet() (which by default triggers refresh()).
     */
    void setApplication(rats::app::Application* app);

    /**
     * @brief Currently selected torrent (invalid Torrent if none selected).
     */
    rats::domain::Torrent selectedTorrent() const;

signals:
    void torrentSelected(const rats::domain::Torrent& torrent);
    void torrentDoubleClicked(const rats::domain::Torrent& torrent);
    void exportTorrentRequested(const rats::domain::Torrent& torrent);

protected:
    /**
     * @brief Load/reload this tab's data. Implemented by subclasses.
     */
    virtual void refresh() = 0;

    /**
     * @brief Called by setApplication() after app_ is stored. Default just
     *        calls refresh(); subclasses override to also wire up service
     *        signals before the first refresh.
     */
    virtual void onApplicationSet();

    /**
     * @brief Selection hook. Default no-op; subclasses override (e.g. to toggle
     *        a remove button). Called on every current-row change.
     */
    virtual void onSelectionChanged(const rats::domain::Torrent& torrent, bool valid);

    /**
     * @brief Build the context menu for a torrent. Base provides the standard
     *        actions (Open Magnet / Copy Info Hash / Copy Magnet / Export).
     *        Subclasses may override, call the base, and add their own actions.
     *        Returned menu is parented and deleted after exec.
     */
    virtual QMenu* buildContextMenu(const rats::domain::Torrent& torrent);

    /**
     * @brief Push search hits into the model.
     */
    void setResults(const QVector<rats::domain::SearchHit>& hits);

    /**
     * @brief Convenience: wrap plain torrents in SearchHits and display them.
     */
    void setTorrents(const QVector<rats::domain::Torrent>& torrents);

    rats::app::Application* app_ = nullptr;
    QVBoxLayout* mainLayout_ = nullptr;
    QTableView* tableView_ = nullptr;
    SearchResultModel* model_ = nullptr;

private slots:
    void handleSelectionRow(const QModelIndex& current);
    void handleDoubleClicked(const QModelIndex& index);
    void handleContextMenu(const QPoint& pos);

private:
    void setupTable();
};

#endif // TORRENTTABLEWIDGET_H
