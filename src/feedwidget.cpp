#include "feedwidget.h"
#include "api/ratsapi.h"
#include "torrentitemdelegate.h"
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>

FeedWidget::FeedWidget(QWidget *parent)
    : QWidget(parent)
    , api_(nullptr)
    , tableView_(nullptr)
    , model_(nullptr)
    , statusLabel_(nullptr)
    , emptyLabel_(nullptr)
    , loading_(false)
    , loadedCount_(0)
{
    setupUi();
}

FeedWidget::~FeedWidget()
{
}

void FeedWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Header
    QWidget* headerRow = new QWidget(this);
    headerRow->setObjectName("headerRow");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(16, 12, 16, 12);
    
    QLabel* titleLabel = new QLabel(tr("📰 Feed - Popular & Voted Torrents"), this);
    titleLabel->setObjectName("headerLabel");
    headerLayout->addWidget(titleLabel);
    
    headerLayout->addStretch();
    
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("statusLabel");
    headerLayout->addWidget(statusLabel_);
    
    mainLayout->addWidget(headerRow);
    
    // Empty state label
    emptyLabel_ = new QLabel(tr("No feed items yet. Vote on torrents to populate the feed!"), this);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setObjectName("emptyStateLabel");
    emptyLabel_->hide();
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
    connect(tableView_, &QTableView::doubleClicked, this, &FeedWidget::onTorrentDoubleClicked);
    
    // Auto-load on scroll
    connect(tableView_->verticalScrollBar(), &QScrollBar::valueChanged, 
            this, &FeedWidget::onScrollValueChanged);
    
    mainLayout->addWidget(tableView_, 1);
}

void FeedWidget::setApi(RatsAPI* api)
{
    api_ = api;
    if (api_) {
        connect(api_, &RatsAPI::feedUpdated, this, &FeedWidget::handleFeedUpdate);
    }
    loadMoreFeed();
}

void FeedWidget::refresh()
{
    feedTorrents_.clear();
    loadedCount_ = 0;
    model_->clearResults();
    loadMoreFeed();
}

void FeedWidget::onTorrentSelected(const QModelIndex& index)
{
    if (index.isValid()) {
        TorrentInfo torrent = model_->getTorrent(index.row());
        if (torrent.isValid()) {
            emit torrentSelected(torrent);
        }
    }
}

void FeedWidget::onTorrentDoubleClicked(const QModelIndex& index)
{
    if (index.isValid()) {
        TorrentInfo torrent = model_->getTorrent(index.row());
        if (torrent.isValid()) {
            emit torrentDoubleClicked(torrent);
        }
    }
}

void FeedWidget::onScrollValueChanged(int value)
{
    QScrollBar* scrollBar = tableView_->verticalScrollBar();
    if (scrollBar && value >= scrollBar->maximum() - 50) {
        loadMoreFeed();
    }
}

void FeedWidget::loadMoreFeed()
{
    if (!api_ || loading_) return;
    
    loading_ = true;
    statusLabel_->setText(tr("Loading..."));
    
    api_->getFeed(loadedCount_, 20, [this](const ApiResponse& response) {
        loading_ = false;
        
        if (!response.success) {
            statusLabel_->setText(tr("Error: %1").arg(response.error));
            return;
        }
        
        QJsonArray arr = response.data.toArray();
        if (arr.isEmpty() && feedTorrents_.isEmpty()) {
            emptyLabel_->show();
            tableView_->hide();
            statusLabel_->setText(tr("No items"));
            return;
        }
        
        emptyLabel_->hide();
        tableView_->show();
        
        for (const QJsonValue& val : arr) {
            QJsonObject obj = val.toObject();
            
            // Skip adult content like legacy does
            QString category = obj["contentCategory"].toString();
            if (category == "xxx") continue;
            
            TorrentInfo info;
            info.hash = obj["hash"].toString();
            info.name = obj["name"].toString();
            info.size = obj["size"].toVariant().toLongLong();
            info.files = obj["files"].toInt();
            info.seeders = obj["seeders"].toInt();
            info.leechers = obj["leechers"].toInt();
            info.completed = obj["completed"].toInt();
            info.added = QDateTime::fromMSecsSinceEpoch(obj["added"].toVariant().toLongLong());
            info.contentType = obj["contentType"].toString();
            info.contentCategory = category;
            info.good = obj["good"].toInt();
            info.bad = obj["bad"].toInt();
            
            if (info.isValid()) {
                feedTorrents_.append(info);
            }
        }
        
        loadedCount_ = feedTorrents_.size();
        model_->setResults(feedTorrents_);
        statusLabel_->setText(tr("%1 items").arg(feedTorrents_.size()));
    });
}

void FeedWidget::handleFeedUpdate(const QJsonArray& feed)
{
    // Replace current feed with updated one (limited to what we've loaded)
    feedTorrents_.clear();
    
    int limit = qMin(feed.size(), loadedCount_ > 0 ? loadedCount_ : 20);
    for (int i = 0; i < limit; ++i) {
        QJsonObject obj = feed.at(i).toObject();
        
        QString category = obj["contentCategory"].toString();
        if (category == "xxx") continue;
        
        TorrentInfo info;
        info.hash = obj["hash"].toString();
        info.name = obj["name"].toString();
        info.size = obj["size"].toVariant().toLongLong();
        info.files = obj["files"].toInt();
        info.seeders = obj["seeders"].toInt();
        info.leechers = obj["leechers"].toInt();
        info.completed = obj["completed"].toInt();
        info.contentType = obj["contentType"].toString();
        info.contentCategory = category;
        info.good = obj["good"].toInt();
        info.bad = obj["bad"].toInt();
        
        if (info.isValid()) {
            feedTorrents_.append(info);
        }
    }
    
    if (feedTorrents_.isEmpty()) {
        emptyLabel_->show();
        tableView_->hide();
    } else {
        emptyLabel_->hide();
        tableView_->show();
        model_->setResults(feedTorrents_);
    }
    
    statusLabel_->setText(tr("%1 items").arg(feedTorrents_.size()));
}
