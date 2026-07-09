#include "activitywidget.h"
#include "app/application.h"
#include "domain/content.h"
#include "format.h"
#include "services/indexing_service.h"
#include "services/search_service.h"
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFont>
#include <QMenu>
#include <QScrollBar>
#include <QStyle>
#include <QUrl>

ActivityWidget::ActivityWidget(QWidget* parent)
    : QWidget(parent)
    , app_(nullptr)
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

    // Note: Hover/selection colors are handled by the application theme
    // (light.qss/dark.qss) We only set padding and height here

    connect(torrentList_, &QListWidget::itemClicked, this, &ActivityWidget::onItemClicked);
    connect(torrentList_, &QListWidget::itemDoubleClicked, this, &ActivityWidget::onItemDoubleClicked);
    torrentList_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(torrentList_, &QListWidget::customContextMenuRequested, this, &ActivityWidget::onContextMenu);

    mainLayout->addWidget(torrentList_, 1);
}

void ActivityWidget::setApplication(rats::app::Application* app)
{
    app_ = app;

    if (app_) {
        // Connect to new torrent signal from the indexing service
        if (auto* indexing = app_->indexing()) {
            connect(indexing, &rats::service::IndexingService::torrentIndexed, this, &ActivityWidget::onNewTorrent);
        }

        // Load initial recent torrents
        loadRecentTorrents();
    }
}

void ActivityWidget::loadRecentTorrents()
{
    if (!app_ || !app_->search())
        return;

    statusLabel_->setText(tr("Loading..."));

    const QVector<rats::domain::Torrent> torrents = app_->search()->recent(MAX_DISPLAY_SIZE);

    // Clear existing items
    torrentList_->clear();
    displayedTorrents_.clear();

    // Add torrents to display (in order - newest first)
    for (const rats::domain::Torrent& info : torrents) {
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
}

void ActivityWidget::onNewTorrent(const rats::domain::Torrent& torrent)
{
    const QString hash = torrent.hash;

    // Check if we already have this torrent in queue or display
    if (displayQueueAssoc_.contains(hash) || displayedTorrents_.contains(hash)) {
        return;
    }

    // Add to queue if not full
    if (displayQueue_.size() < MAX_QUEUE_SIZE) {
        displayQueue_.enqueue(torrent);
        displayQueueAssoc_[hash] = torrent;

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
    rats::domain::Torrent torrent = displayQueue_.dequeue();

    // Remove from queue association
    displayQueueAssoc_.remove(torrent.hash);

    // The indexing signal already carries the full torrent entity, so we can
    // display it directly without a second lookup.
    addTorrentToDisplay(torrent);

    // Schedule next display
    displayTimer_->start(DISPLAY_SPEED_MS);
}

void ActivityWidget::addTorrentToDisplay(const rats::domain::Torrent& torrent)
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

QListWidgetItem* ActivityWidget::createTorrentItem(const rats::domain::Torrent& torrent)
{
    // Create formatted text for the item
    QString icon = rats::ui::contentTypeIcon(torrent.contentType);
    QString sizeStr = rats::ui::formatSize(torrent.size);
    QString dateStr = rats::ui::formatDate(torrent.added);

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
            .arg(torrent.seeders)
            .arg(torrent.leechers));

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
    if (!item)
        return;

    QString hash = item->data(Qt::UserRole).toString();
    if (displayedTorrents_.contains(hash)) {
        emit torrentSelected(displayedTorrents_[hash]);
    }
}

void ActivityWidget::onItemDoubleClicked(QListWidgetItem* item)
{
    if (!item)
        return;

    QString hash = item->data(Qt::UserRole).toString();
    if (displayedTorrents_.contains(hash)) {
        emit torrentDoubleClicked(displayedTorrents_[hash]);
    }
}

void ActivityWidget::onContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = torrentList_->itemAt(pos);
    if (!item)
        return;

    QString hash = item->data(Qt::UserRole).toString();
    if (!displayedTorrents_.contains(hash))
        return;
    rats::domain::Torrent torrent = displayedTorrents_[hash];
    if (!torrent.isValid())
        return;

    QMenu contextMenu(this);

    QAction* magnetAction = contextMenu.addAction(tr("Open Magnet Link"));
    connect(magnetAction, &QAction::triggered, [torrent]() { QDesktopServices::openUrl(QUrl(torrent.magnetLink())); });

    QAction* copyHashAction = contextMenu.addAction(tr("Copy Info Hash"));
    connect(copyHashAction, &QAction::triggered, [torrent]() { QApplication::clipboard()->setText(torrent.hash); });

    QAction* copyMagnetAction = contextMenu.addAction(tr("Copy Magnet Link"));
    connect(copyMagnetAction, &QAction::triggered,
        [torrent]() { QApplication::clipboard()->setText(torrent.magnetLink()); });

    contextMenu.addSeparator();

    QAction* exportAction = contextMenu.addAction(tr("💾 Export to .torrent file..."));
    connect(exportAction, &QAction::triggered, [this, torrent]() { emit exportTorrentRequested(torrent); });

    contextMenu.exec(torrentList_->viewport()->mapToGlobal(pos));
}
