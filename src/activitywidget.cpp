#include "activitywidget.h"
#include "api/ratsapi.h"
#include <QScrollBar>
#include <QJsonArray>
#include <QJsonObject>
#include <QApplication>
#include <QFont>

ActivityWidget::ActivityWidget(QWidget *parent)
    : QWidget(parent)
    , api_(nullptr)
    , torrentList_(nullptr)
    , pauseButton_(nullptr)
    , topButton_(nullptr)
    , titleLabel_(nullptr)
    , queueLabel_(nullptr)
    , statusLabel_(nullptr)
    , displayTimer_(nullptr)
    , counterTimer_(nullptr)
    , isPaused_(false)
    , isInitialized_(false)
{
    setupUi();
    
    // Setup display timer
    displayTimer_ = new QTimer(this);
    displayTimer_->setSingleShot(true);
    connect(displayTimer_, &QTimer::timeout, this, &ActivityWidget::displayNextTorrent);
    
    // Setup counter update timer
    counterTimer_ = new QTimer(this);
    connect(counterTimer_, &QTimer::timeout, this, &ActivityWidget::updateQueueCounter);
    counterTimer_->start(COUNTER_UPDATE_MS);
}

ActivityWidget::~ActivityWidget()
{
    if (displayTimer_) {
        displayTimer_->stop();
    }
    if (counterTimer_) {
        counterTimer_->stop();
    }
}

void ActivityWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Header row
    QWidget* headerRow = new QWidget(this);
    headerRow->setObjectName("filterRow");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(12, 10, 12, 10);
    headerLayout->setSpacing(12);
    
    // Top button (navigate to Top tab)
    topButton_ = new QPushButton(tr("🔥 Top"), this);
    topButton_->setObjectName("secondaryButton");
    topButton_->setCursor(Qt::PointingHandCursor);
    topButton_->setToolTip(tr("Go to Top Torrents"));
    connect(topButton_, &QPushButton::clicked, this, &ActivityWidget::navigateToTop);
    headerLayout->addWidget(topButton_);
    
    // Pause/Continue button
    pauseButton_ = new QPushButton(this);
    pauseButton_->setObjectName("primaryButton");
    pauseButton_->setCursor(Qt::PointingHandCursor);
    updatePauseButton();
    connect(pauseButton_, &QPushButton::clicked, this, &ActivityWidget::togglePause);
    headerLayout->addWidget(pauseButton_);
    
    // Title
    titleLabel_ = new QLabel(tr("Most Recent Torrents"), this);
    titleLabel_->setObjectName("subtitleLabel");
    headerLayout->addWidget(titleLabel_);
    
    // Queue counter
    queueLabel_ = new QLabel(this);
    queueLabel_->setObjectName("hintLabel");
    headerLayout->addWidget(queueLabel_);
    
    headerLayout->addStretch();
    
    // Status label
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("statusLabel");
    headerLayout->addWidget(statusLabel_);
    
    mainLayout->addWidget(headerRow);
    
    // Torrent list
    torrentList_ = new QListWidget(this);
    torrentList_->setObjectName("activityTorrentList");
    torrentList_->setAlternatingRowColors(true);
    torrentList_->setSelectionMode(QAbstractItemView::SingleSelection);
    torrentList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    torrentList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    torrentList_->setSpacing(2);
    
    // Set custom item height
    torrentList_->setStyleSheet(
        "QListWidget::item { padding: 12px 16px; min-height: 50px; }"
        "QListWidget::item:selected { background-color: palette(highlight); }"
        "QListWidget::item:hover { background-color: palette(midlight); }"
    );
    
    connect(torrentList_, &QListWidget::itemClicked, this, &ActivityWidget::onItemClicked);
    connect(torrentList_, &QListWidget::itemDoubleClicked, this, &ActivityWidget::onItemDoubleClicked);
    
    mainLayout->addWidget(torrentList_, 1);
}

void ActivityWidget::setApi(RatsAPI* api)
{
    api_ = api;
    
    if (api_) {
        // Connect to new torrent signal
        connect(api_, &RatsAPI::torrentIndexed, this, &ActivityWidget::onNewTorrent);
        
        // Load initial recent torrents
        loadRecentTorrents();
    }
}

void ActivityWidget::loadRecentTorrents()
{
    if (!api_) return;
    
    statusLabel_->setText(tr("Loading..."));
    
    api_->getRecentTorrents(MAX_DISPLAY_SIZE, [this](const ApiResponse& response) {
        if (!response.success) {
            statusLabel_->setText(tr("Error: %1").arg(response.error));
            return;
        }
        
        QJsonArray torrents = response.data.toArray();
        
        // Clear existing items
        torrentList_->clear();
        displayedTorrents_.clear();
        
        // Add torrents to display (in order - newest first)
        for (const QJsonValue& val : torrents) {
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
            info.good = obj["good"].toInt();
            info.bad = obj["bad"].toInt();
            
            if (info.isValid()) {
                QListWidgetItem* item = createTorrentItem(info);
                torrentList_->addItem(item);
                displayedTorrents_[info.hash] = info;
            }
        }
        
        statusLabel_->setText(tr("%1 torrents").arg(torrentList_->count()));
        isInitialized_ = true;
        
        // Start the display timer to process any queued torrents
        if (!displayTimer_->isActive()) {
            displayTimer_->start(DISPLAY_SPEED_MS);
        }
    });
}

void ActivityWidget::onNewTorrent(const QString& hash, const QString& name)
{
    // Check if we already have this torrent in queue or display
    if (displayQueueAssoc_.contains(hash) || displayedTorrents_.contains(hash)) {
        return;
    }
    
    // Add to queue if not full
    if (displayQueue_.size() < MAX_QUEUE_SIZE) {
        TorrentInfo info;
        info.hash = hash;
        info.name = name;
        info.added = QDateTime::currentDateTime();
        // Other fields will be filled when we fetch full info
        
        displayQueue_.enqueue(info);
        displayQueueAssoc_[hash] = info;
        
        // Start display timer if not running
        if (!displayTimer_->isActive() && isInitialized_) {
            displayTimer_->start(DISPLAY_SPEED_MS);
        }
    }
}

void ActivityWidget::displayNextTorrent()
{
    if (displayQueue_.isEmpty()) {
        // Queue is empty, check again in 1 second
        displayTimer_->start(1000);
        return;
    }
    
    if (isPaused_) {
        // Paused, check again after delay
        displayTimer_->start(DISPLAY_SPEED_MS);
        return;
    }
    
    // Take next torrent from queue
    TorrentInfo torrent = displayQueue_.dequeue();
    
    // Remove from queue association
    displayQueueAssoc_.remove(torrent.hash);
    
    // Fetch full torrent info if we only have hash/name
    if (api_ && torrent.size == 0) {
        api_->getTorrent(torrent.hash, false, QString(), [this, torrent](const ApiResponse& response) mutable {
            if (response.success) {
                QJsonObject obj = response.data.toObject();
                torrent.size = obj["size"].toVariant().toLongLong();
                torrent.files = obj["files"].toInt();
                torrent.seeders = obj["seeders"].toInt();
                torrent.leechers = obj["leechers"].toInt();
                torrent.completed = obj["completed"].toInt();
                torrent.added = QDateTime::fromMSecsSinceEpoch(obj["added"].toVariant().toLongLong());
                torrent.contentType = obj["contentType"].toString();
                torrent.contentCategory = obj["contentCategory"].toString();
                torrent.good = obj["good"].toInt();
                torrent.bad = obj["bad"].toInt();
            }
            
            // Add to display
            addTorrentToDisplay(torrent);
        });
    } else {
        addTorrentToDisplay(torrent);
    }
    
    // Schedule next display
    displayTimer_->start(DISPLAY_SPEED_MS);
}

void ActivityWidget::addTorrentToDisplay(const TorrentInfo& torrent)
{
    // Skip if already displayed
    if (displayedTorrents_.contains(torrent.hash)) {
        return;
    }
    
    // Create item and insert at top
    QListWidgetItem* item = createTorrentItem(torrent);
    torrentList_->insertItem(0, item);
    displayedTorrents_[torrent.hash] = torrent;
    
    // Remove old items if over limit
    while (torrentList_->count() > MAX_DISPLAY_SIZE) {
        QListWidgetItem* lastItem = torrentList_->takeItem(torrentList_->count() - 1);
        if (lastItem) {
            QString hash = lastItem->data(Qt::UserRole).toString();
            displayedTorrents_.remove(hash);
            displayQueueAssoc_.remove(hash);
            delete lastItem;
        }
    }
    
    // Update status
    statusLabel_->setText(tr("%1 torrents").arg(torrentList_->count()));
}

void ActivityWidget::updateQueueCounter()
{
    int queueSize = displayQueue_.size();
    if (queueSize > 0) {
        queueLabel_->setText(tr("(and %1 more)").arg(queueSize));
        queueLabel_->show();
    } else {
        queueLabel_->hide();
    }
}

QListWidgetItem* ActivityWidget::createTorrentItem(const TorrentInfo& torrent)
{
    // Create formatted text for the item
    QString icon = getContentTypeIcon(torrent.contentType);
    QString sizeStr = formatSize(torrent.size);
    QString dateStr = formatDate(torrent.added);
    
    // Primary text: icon + name
    QString primaryText = QString("%1 %2").arg(icon, torrent.name);
    
    // Secondary text: size, seeders, date
    QString secondaryText;
    if (torrent.size > 0) {
        secondaryText = QString("%1  •  ").arg(sizeStr);
    }
    if (torrent.seeders > 0 || torrent.leechers > 0) {
        secondaryText += QString("🌱 %1 / %2  •  ").arg(torrent.seeders).arg(torrent.leechers);
    }
    if (torrent.added.isValid()) {
        secondaryText += dateStr;
    }
    
    // Combined display with line break
    QString displayText = primaryText;
    if (!secondaryText.isEmpty()) {
        displayText += "\n" + secondaryText;
    }
    
    QListWidgetItem* item = new QListWidgetItem(displayText);
    item->setData(Qt::UserRole, torrent.hash);
    
    // Store full torrent info
    item->setData(Qt::UserRole + 1, QVariant::fromValue(torrent));
    
    // Set tooltip with full name
    item->setToolTip(QString("%1\nHash: %2\nSize: %3\nSeeders: %4 | Leechers: %5")
        .arg(torrent.name, torrent.hash, sizeStr)
        .arg(torrent.seeders).arg(torrent.leechers));
    
    return item;
}

void ActivityWidget::togglePause()
{
    isPaused_ = !isPaused_;
    updatePauseButton();
    
    // If resuming, start the display timer
    if (!isPaused_ && !displayTimer_->isActive()) {
        displayTimer_->start(DISPLAY_SPEED_MS);
    }
}

void ActivityWidget::updatePauseButton()
{
    if (isPaused_) {
        pauseButton_->setText(tr("▶ Continue"));
        pauseButton_->setObjectName("successButton");
    } else {
        pauseButton_->setText(tr("⏸ Running"));
        pauseButton_->setObjectName("primaryButton");
    }
    // Force style update
    pauseButton_->style()->unpolish(pauseButton_);
    pauseButton_->style()->polish(pauseButton_);
}

void ActivityWidget::onItemClicked(QListWidgetItem* item)
{
    if (!item) return;
    
    QString hash = item->data(Qt::UserRole).toString();
    if (displayedTorrents_.contains(hash)) {
        emit torrentSelected(displayedTorrents_[hash]);
    }
}

void ActivityWidget::onItemDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;
    
    QString hash = item->data(Qt::UserRole).toString();
    if (displayedTorrents_.contains(hash)) {
        emit torrentDoubleClicked(displayedTorrents_[hash]);
    }
}

QString ActivityWidget::formatSize(qint64 bytes) const
{
    if (bytes <= 0) return QString();
    
    const char* suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int suffixIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && suffixIndex < 5) {
        size /= 1024.0;
        suffixIndex++;
    }
    
    if (suffixIndex == 0) {
        return QString("%1 %2").arg(static_cast<int>(size)).arg(suffixes[suffixIndex]);
    }
    return QString("%1 %2").arg(size, 0, 'f', 1).arg(suffixes[suffixIndex]);
}

QString ActivityWidget::formatDate(const QDateTime& dateTime) const
{
    if (!dateTime.isValid()) return QString();
    
    QDateTime now = QDateTime::currentDateTime();
    qint64 secsAgo = dateTime.secsTo(now);
    
    if (secsAgo < 60) {
        return tr("Just now");
    } else if (secsAgo < 3600) {
        int mins = static_cast<int>(secsAgo / 60);
        return tr("%1 min ago").arg(mins);
    } else if (secsAgo < 86400) {
        int hours = static_cast<int>(secsAgo / 3600);
        return tr("%1 hr ago").arg(hours);
    } else if (secsAgo < 604800) {
        int days = static_cast<int>(secsAgo / 86400);
        return tr("%1 days ago").arg(days);
    } else {
        return dateTime.toString("MMM d, yyyy");
    }
}

QString ActivityWidget::getContentTypeIcon(const QString& contentType) const
{
    if (contentType.isEmpty()) return "📦";
    
    QString type = contentType.toLower();
    
    if (type.contains("video") || type == "movie" || type == "tv") {
        return "🎬";
    } else if (type.contains("audio") || type == "music") {
        return "🎵";
    } else if (type.contains("book") || type.contains("document") || type == "ebook") {
        return "📚";
    } else if (type.contains("image") || type.contains("picture") || type == "photo") {
        return "🖼️";
    } else if (type.contains("app") || type.contains("software") || type == "game") {
        return "🎮";
    } else if (type.contains("archive") || type.contains("zip") || type.contains("rar")) {
        return "📁";
    }
    
    return "📦";
}
