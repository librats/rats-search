#include "favoriteswidget.h"
#include "favoritesmanager.h"
#include "torrentitemdelegate.h"
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>

FavoritesWidget::FavoritesWidget(QWidget *parent)
    : QWidget(parent)
    , favoritesManager_(nullptr)
    , tableView_(nullptr)
    , model_(nullptr)
    , statusLabel_(nullptr)
    , emptyLabel_(nullptr)
    , removeButton_(nullptr)
{
    setupUi();
}

FavoritesWidget::~FavoritesWidget()
{
}

void FavoritesWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Header row
    QWidget* headerRow = new QWidget(this);
    headerRow->setObjectName("filterRow");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(12, 8, 12, 8);
    
    QLabel* titleLabel = new QLabel(tr("⭐ My Favorites"), this);
    titleLabel->setObjectName("sectionTitle");
    headerLayout->addWidget(titleLabel);
    
    headerLayout->addStretch();
    
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("statusLabel");
    headerLayout->addWidget(statusLabel_);
    
    // Remove button (enabled when a torrent is selected)
    removeButton_ = new QPushButton(tr("Remove from Favorites"), this);
    removeButton_->setObjectName("dangerButton");
    removeButton_->setCursor(Qt::PointingHandCursor);
    removeButton_->setEnabled(false);
    connect(removeButton_, &QPushButton::clicked, this, &FavoritesWidget::onRemoveClicked);
    headerLayout->addWidget(removeButton_);
    
    mainLayout->addWidget(headerRow);
    
    // Empty state label
    emptyLabel_ = new QLabel(tr("No favorites yet.\n\nAdd torrents to favorites from the details panel\nor they will be added automatically when you create or import a torrent."), this);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setObjectName("emptyStateLabel");
    emptyLabel_->setWordWrap(true);
    mainLayout->addWidget(emptyLabel_);
    
    // Table view
    tableView_ = new QTableView(this);
    model_ = new SearchResultModel(this);
    TorrentItemDelegate* delegate = new TorrentItemDelegate(this);
    
    tableView_->setModel(model_);
    tableView_->setItemDelegate(delegate);
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setAlternatingRowColors(true);
    tableView_->setSortingEnabled(false);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->verticalHeader()->setVisible(false);
    tableView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView_->setShowGrid(false);
    tableView_->setMouseTracking(true);
    tableView_->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Column widths
    tableView_->setColumnWidth(0, 500);  // Name
    tableView_->setColumnWidth(1, 100);  // Size
    tableView_->setColumnWidth(2, 80);   // Seeders
    tableView_->setColumnWidth(3, 80);   // Leechers
    tableView_->setColumnWidth(4, 120);  // Date
    
    connect(tableView_->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex& current, const QModelIndex&) {
                onTorrentSelected(current);
            });
    connect(tableView_, &QTableView::doubleClicked, this, &FavoritesWidget::onTorrentDoubleClicked);
    connect(tableView_, &QTableView::customContextMenuRequested, this, &FavoritesWidget::onContextMenu);
    
    mainLayout->addWidget(tableView_, 1);
}

void FavoritesWidget::setFavoritesManager(FavoritesManager* manager)
{
    favoritesManager_ = manager;
    
    if (favoritesManager_) {
        connect(favoritesManager_, &FavoritesManager::favoritesChanged,
                this, &FavoritesWidget::refresh);
        refresh();
    }
}

TorrentInfo FavoritesWidget::getSelectedTorrent() const
{
    QModelIndex index = tableView_->currentIndex();
    if (index.isValid()) {
        return model_->getTorrent(index.row());
    }
    return TorrentInfo();
}

void FavoritesWidget::refresh()
{
    if (!favoritesManager_) return;
    
    QVector<FavoriteEntry> favorites = favoritesManager_->favorites();
    
    // Convert FavoriteEntry to TorrentInfo for the model
    QVector<TorrentInfo> torrents;
    torrents.reserve(favorites.size());
    
    for (const FavoriteEntry& fav : favorites) {
        TorrentInfo info;
        info.hash = fav.hash;
        info.name = fav.name;
        info.size = fav.size;
        info.files = fav.files;
        info.seeders = fav.seeders;
        info.leechers = fav.leechers;
        info.completed = fav.completed;
        info.contentType = fav.contentType;
        info.contentCategory = fav.contentCategory;
        info.added = fav.added;
        
        if (info.isValid()) {
            torrents.append(info);
        }
    }
    
    model_->setResults(torrents);
    
    // Update UI state
    bool isEmpty = torrents.isEmpty();
    emptyLabel_->setVisible(isEmpty);
    tableView_->setVisible(!isEmpty);
    
    statusLabel_->setText(isEmpty ? QString() : tr("%1 favorites").arg(torrents.size()));
    removeButton_->setEnabled(false);
}

void FavoritesWidget::onTorrentSelected(const QModelIndex& index)
{
    if (index.isValid()) {
        TorrentInfo torrent = model_->getTorrent(index.row());
        removeButton_->setEnabled(torrent.isValid());
        if (torrent.isValid()) {
            emit torrentSelected(torrent);
        }
    } else {
        removeButton_->setEnabled(false);
    }
}

void FavoritesWidget::onTorrentDoubleClicked(const QModelIndex& index)
{
    if (index.isValid()) {
        TorrentInfo torrent = model_->getTorrent(index.row());
        if (torrent.isValid()) {
            emit torrentDoubleClicked(torrent);
        }
    }
}

void FavoritesWidget::onContextMenu(const QPoint& pos)
{
    QModelIndex index = tableView_->indexAt(pos);
    if (!index.isValid()) return;
    
    TorrentInfo torrent = model_->getTorrent(index.row());
    if (!torrent.isValid()) return;
    
    QMenu contextMenu(this);
    
    QAction* removeAction = contextMenu.addAction(tr("❌ Remove from Favorites"));
    connect(removeAction, &QAction::triggered, [this, torrent]() {
        if (favoritesManager_) {
            favoritesManager_->removeFavorite(torrent.hash);
        }
    });
    
    contextMenu.exec(tableView_->viewport()->mapToGlobal(pos));
}

void FavoritesWidget::onRemoveClicked()
{
    QModelIndex index = tableView_->currentIndex();
    if (!index.isValid()) return;
    
    TorrentInfo torrent = model_->getTorrent(index.row());
    if (!torrent.isValid()) return;
    
    if (favoritesManager_) {
        favoritesManager_->removeFavorite(torrent.hash);
    }
}
