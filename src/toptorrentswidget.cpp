#include "toptorrentswidget.h"
#include "api/ratsapi.h"
#include "torrentitemdelegate.h"
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>

TopTorrentsWidget::TopTorrentsWidget(QWidget *parent)
    : QWidget(parent)
    , api_(nullptr)
    , categoryTabs_(nullptr)
    , timeFilter_(nullptr)
    , tableView_(nullptr)
    , model_(nullptr)
    , moreButton_(nullptr)
    , statusLabel_(nullptr)
    , currentCategory_("main")
    , currentTime_("overall")
{
    // Initialize categories (matches legacy top-page.js)
    categories_ = {"main", "video", "audio", "books", "pictures", "application", "archive"};
    categoryLabels_ = {
        {"main", tr("All")},
        {"video", tr("Video")},
        {"audio", tr("Audio/Music")},
        {"books", tr("Books")},
        {"pictures", tr("Pictures")},
        {"application", tr("Apps/Games")},
        {"archive", tr("Archives")}
    };
    
    // Initialize time filters
    timeFilters_ = {"overall", "hours", "week", "month"};
    timeLabels_ = {
        {"overall", tr("Overall")},
        {"hours", tr("Last Hour")},
        {"week", tr("Last Week")},
        {"month", tr("Last Month")}
    };
    
    setupUi();
}

TopTorrentsWidget::~TopTorrentsWidget()
{
}

void TopTorrentsWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Category tabs
    categoryTabs_ = new QTabBar(this);
    categoryTabs_->setExpanding(false);
    categoryTabs_->setStyleSheet(R"(
        QTabBar::tab {
            background-color: #2d2d2d;
            color: #888888;
            padding: 10px 20px;
            margin-right: 2px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            font-weight: bold;
        }
        QTabBar::tab:selected {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #4a9eff, stop:1 #3d8ce8);
            color: #ffffff;
        }
        QTabBar::tab:hover:!selected {
            background-color: #3c3f41;
            color: #ffffff;
        }
    )");
    
    for (const QString& cat : categories_) {
        categoryTabs_->addTab(categoryLabels_.value(cat, cat));
    }
    connect(categoryTabs_, &QTabBar::currentChanged, this, &TopTorrentsWidget::onCategoryChanged);
    mainLayout->addWidget(categoryTabs_);
    
    // Time filter row
    QWidget* filterRow = new QWidget(this);
    filterRow->setStyleSheet("background-color: #252526;");
    QHBoxLayout* filterLayout = new QHBoxLayout(filterRow);
    filterLayout->setContentsMargins(12, 8, 12, 8);
    
    QLabel* filterLabel = new QLabel(tr("Time Period:"), this);
    filterLabel->setStyleSheet("color: #888888; font-size: 12px;");
    filterLayout->addWidget(filterLabel);
    
    timeFilter_ = new QComboBox(this);
    for (const QString& time : timeFilters_) {
        timeFilter_->addItem(timeLabels_.value(time, time), time);
    }
    timeFilter_->setMinimumWidth(150);
    connect(timeFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &TopTorrentsWidget::onTimeFilterChanged);
    filterLayout->addWidget(timeFilter_);
    
    filterLayout->addStretch();
    
    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet("color: #666666; font-size: 11px;");
    filterLayout->addWidget(statusLabel_);
    
    mainLayout->addWidget(filterRow);
    
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
    connect(tableView_, &QTableView::doubleClicked, this, &TopTorrentsWidget::onTorrentDoubleClicked);
    
    mainLayout->addWidget(tableView_, 1);
    
    // More button
    QWidget* bottomRow = new QWidget(this);
    bottomRow->setStyleSheet("background-color: #252526;");
    QHBoxLayout* bottomLayout = new QHBoxLayout(bottomRow);
    bottomLayout->setContentsMargins(12, 8, 12, 8);
    
    bottomLayout->addStretch();
    
    moreButton_ = new QPushButton(tr("Load More Torrents"), this);
    moreButton_->setStyleSheet(R"(
        QPushButton {
            background-color: #3c3f41;
            color: #ffffff;
            border: 1px solid #555555;
            border-radius: 6px;
            padding: 8px 24px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #4c4f51;
            border-color: #4a9eff;
        }
    )");
    connect(moreButton_, &QPushButton::clicked, this, &TopTorrentsWidget::onMoreTorrentsClicked);
    bottomLayout->addWidget(moreButton_);
    
    bottomLayout->addStretch();
    
    mainLayout->addWidget(bottomRow);
}

void TopTorrentsWidget::setApi(RatsAPI* api)
{
    api_ = api;
    // Initial load for all categories
    for (const QString& cat : categories_) {
        loadTopTorrents(cat, "overall");
    }
}

void TopTorrentsWidget::refresh()
{
    cache_.clear();
    for (const QString& cat : categories_) {
        loadTopTorrents(cat, currentTime_);
    }
}

void TopTorrentsWidget::onCategoryChanged(int index)
{
    if (index >= 0 && index < categories_.size()) {
        currentCategory_ = categories_.at(index);
        
        QString key = getCacheKey(currentCategory_, currentTime_);
        if (cache_.contains(key)) {
            model_->setResults(cache_[key].torrents);
            statusLabel_->setText(tr("%1 torrents").arg(cache_[key].torrents.size()));
        } else {
            model_->clearResults();
            loadTopTorrents(currentCategory_, currentTime_);
        }
    }
}

void TopTorrentsWidget::onTimeFilterChanged(int index)
{
    if (index >= 0 && index < timeFilters_.size()) {
        currentTime_ = timeFilters_.at(index);
        
        QString key = getCacheKey(currentCategory_, currentTime_);
        if (cache_.contains(key)) {
            model_->setResults(cache_[key].torrents);
            statusLabel_->setText(tr("%1 torrents").arg(cache_[key].torrents.size()));
        } else {
            model_->clearResults();
            loadTopTorrents(currentCategory_, currentTime_);
        }
    }
}

void TopTorrentsWidget::onTorrentSelected(const QModelIndex& index)
{
    if (index.isValid()) {
        TorrentInfo torrent = model_->getTorrent(index.row());
        if (torrent.isValid()) {
            emit torrentSelected(torrent);
        }
    }
}

void TopTorrentsWidget::onTorrentDoubleClicked(const QModelIndex& index)
{
    if (index.isValid()) {
        TorrentInfo torrent = model_->getTorrent(index.row());
        if (torrent.isValid()) {
            emit torrentDoubleClicked(torrent);
        }
    }
}

void TopTorrentsWidget::onMoreTorrentsClicked()
{
    loadTopTorrents(currentCategory_, currentTime_);
}

void TopTorrentsWidget::loadTopTorrents(const QString& category, const QString& time)
{
    if (!api_) return;
    
    QString key = getCacheKey(category, time);
    int page = cache_.contains(key) ? cache_[key].page : 0;
    
    QJsonObject options;
    options["index"] = page * 20;
    options["limit"] = 20;
    if (time != "overall") {
        options["time"] = time;
    }
    
    QString typeParam = (category == "main") ? QString() : category;
    
    statusLabel_->setText(tr("Loading..."));
    
    api_->getTopTorrents(typeParam, options, [this, category, time](const ApiResponse& response) {
        if (!response.success) {
            statusLabel_->setText(tr("Error: %1").arg(response.error));
            return;
        }
        
        QVector<TorrentInfo> torrents;
        QJsonArray arr = response.data.toArray();
        for (const QJsonValue& val : arr) {
            QJsonObject obj = val.toObject();
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
            info.contentCategory = obj["contentCategory"].toString();
            
            if (info.isValid()) {
                torrents.append(info);
            }
        }
        
        mergeTorrents(torrents, category, time);
    });
}

void TopTorrentsWidget::mergeTorrents(const QVector<TorrentInfo>& torrents, const QString& category, const QString& time)
{
    QString key = getCacheKey(category, time);
    
    if (!cache_.contains(key)) {
        cache_[key] = CacheEntry();
    }
    
    CacheEntry& entry = cache_[key];
    
    // Merge and deduplicate
    QHash<QString, bool> existing;
    for (const TorrentInfo& t : entry.torrents) {
        existing[t.hash] = true;
    }
    
    for (const TorrentInfo& t : torrents) {
        if (!existing.contains(t.hash)) {
            entry.torrents.append(t);
            existing[t.hash] = true;
        }
    }
    
    // Sort by seeders descending
    std::sort(entry.torrents.begin(), entry.torrents.end(),
              [](const TorrentInfo& a, const TorrentInfo& b) {
                  return a.seeders > b.seeders;
              });
    
    entry.page++;
    
    // Update UI if this is the current view
    if (category == currentCategory_ && time == currentTime_) {
        model_->setResults(entry.torrents);
        statusLabel_->setText(tr("%1 torrents").arg(entry.torrents.size()));
    }
}

void TopTorrentsWidget::handleRemoteTopTorrents(const QJsonArray& torrents, const QString& type, const QString& time)
{
    QString category = type.isEmpty() ? "main" : type;
    QString timeFilter = time.isEmpty() ? "overall" : time;
    
    QVector<TorrentInfo> torrentList;
    for (const QJsonValue& val : torrents) {
        QJsonObject obj = val.toObject();
        TorrentInfo info;
        info.hash = obj["hash"].toString();
        if (info.hash.isEmpty()) {
            info.hash = obj["info_hash"].toString();
        }
        info.name = obj["name"].toString();
        info.size = obj["size"].toVariant().toLongLong();
        info.seeders = obj["seeders"].toInt();
        info.leechers = obj["leechers"].toInt();
        info.completed = obj["completed"].toInt();
        info.contentType = obj["contentType"].toString();
        
        if (info.isValid()) {
            torrentList.append(info);
        }
    }
    
    if (!torrentList.isEmpty()) {
        mergeTorrents(torrentList, category, timeFilter);
    }
}

QString TopTorrentsWidget::getCacheKey(const QString& category, const QString& time) const
{
    return category + ":" + time;
}
