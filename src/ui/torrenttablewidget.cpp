#include "torrenttablewidget.h"
#include "searchresultmodel.h"
#include "torrentitemdelegate.h"
#include "torrentmenu.h"

#include <QHeaderView>
#include <QMenu>
#include <QTableView>
#include <QVBoxLayout>

TorrentTableWidget::TorrentTableWidget(QWidget* parent) : QWidget(parent)
{
    mainLayout_ = new QVBoxLayout(this);
    // Top margin keeps the page's own content off the main tab bar — without it
    // the category tabs of the Top page butt straight up against the tabs above
    // them and the two rows read as one.
    mainLayout_->setContentsMargins(0, 8, 0, 0);
    mainLayout_->setSpacing(0);

    setupTable();
}

TorrentTableWidget::~TorrentTableWidget() = default;

void TorrentTableWidget::setupTable()
{
    tableView_ = new QTableView(this);
    model_ = new SearchResultModel(this);
    TorrentItemDelegate* delegate = new TorrentItemDelegate(this);

    tableView_->setModel(model_);
    tableView_->setItemDelegate(delegate);
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setAlternatingRowColors(true);
    // Start with no sort indicator, so enabling sorting does not immediately
    // re-sort by column 0 and destroy the order the page loaded with (top = by
    // seeders, feed = by date). SearchResultModel::sort() then takes over on the
    // first header click, and setResults() re-applies it across refreshes.
    tableView_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    tableView_->setSortingEnabled(true);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->verticalHeader()->setVisible(false);
    tableView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView_->setShowGrid(false);
    tableView_->setMouseTracking(true);
    tableView_->setContextMenuPolicy(Qt::CustomContextMenu);

    // Column widths (Name / Size / Seeders / Leechers / Date)
    tableView_->setColumnWidth(0, 500);
    tableView_->setColumnWidth(1, 100);
    tableView_->setColumnWidth(2, 80);
    tableView_->setColumnWidth(3, 80);
    tableView_->setColumnWidth(4, 120);

    connect(tableView_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
        [this](const QModelIndex& current, const QModelIndex&) { handleSelectionRow(current); });
    connect(tableView_, &QTableView::doubleClicked, this, &TorrentTableWidget::handleDoubleClicked);
    connect(tableView_, &QTableView::customContextMenuRequested, this, &TorrentTableWidget::handleContextMenu);

    mainLayout_->addWidget(tableView_, 1);
}

void TorrentTableWidget::setApplication(rats::app::Application* app)
{
    app_ = app;
    onApplicationSet();
}

void TorrentTableWidget::onApplicationSet()
{
    refresh();
}

void TorrentTableWidget::onSelectionChanged(const rats::domain::Torrent&, bool)
{
    // Default: no-op.
}

rats::domain::Torrent TorrentTableWidget::selectedTorrent() const
{
    QModelIndex index = tableView_->currentIndex();
    if (index.isValid()) {
        return model_->getTorrent(index.row());
    }
    return rats::domain::Torrent();
}

void TorrentTableWidget::setResults(const QVector<rats::domain::SearchHit>& hits)
{
    model_->setResults(hits);
}

void TorrentTableWidget::setTorrents(const QVector<rats::domain::Torrent>& torrents)
{
    QVector<rats::domain::SearchHit> hits;
    hits.reserve(torrents.size());
    for (const rats::domain::Torrent& t : torrents) {
        rats::domain::SearchHit hit;
        hit.torrent = t;
        hits.append(hit);
    }
    model_->setResults(hits);
}

void TorrentTableWidget::handleSelectionRow(const QModelIndex& current)
{
    if (current.isValid()) {
        rats::domain::Torrent torrent = model_->getTorrent(current.row());
        bool valid = torrent.isValid();
        onSelectionChanged(torrent, valid);
        if (valid) {
            emit torrentSelected(torrent);
        }
    } else {
        onSelectionChanged(rats::domain::Torrent(), false);
    }
}

void TorrentTableWidget::handleDoubleClicked(const QModelIndex& index)
{
    if (index.isValid()) {
        rats::domain::Torrent torrent = model_->getTorrent(index.row());
        if (torrent.isValid()) {
            emit torrentDoubleClicked(torrent);
        }
    }
}

void TorrentTableWidget::handleContextMenu(const QPoint& pos)
{
    QModelIndex index = tableView_->indexAt(pos);
    if (!index.isValid())
        return;

    rats::domain::Torrent torrent = model_->getTorrent(index.row());
    if (!torrent.isValid())
        return;

    QMenu* menu = buildContextMenu(torrent);
    if (!menu)
        return;
    menu->exec(tableView_->viewport()->mapToGlobal(pos));
    menu->deleteLater();
}

QMenu* TorrentTableWidget::buildContextMenu(const rats::domain::Torrent& torrent)
{
    QMenu* menu = new QMenu(this);
    rats::ui::addTorrentActions(menu, this, torrent);

    menu->addSeparator();

    QAction* exportAction = menu->addAction(tr("💾 Export to .torrent file..."));
    connect(exportAction, &QAction::triggered, this, [this, torrent]() { emit exportTorrentRequested(torrent); });

    return menu;
}
