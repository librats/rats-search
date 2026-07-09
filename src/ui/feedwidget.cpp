#include "feedwidget.h"
#include "app/application.h"
#include "domain/content.h"
#include "services/feed_service.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QTableView>
#include <QVBoxLayout>

FeedWidget::FeedWidget(QWidget* parent) : TorrentTableWidget(parent)
{
    setupControls();
}

void FeedWidget::setupControls()
{
    // Header row (above the table).
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

    mainLayout_->insertWidget(0, headerRow);

    // Empty state label.
    emptyLabel_ = new QLabel(tr("No feed items yet. Vote on torrents to populate the feed!"), this);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setObjectName("emptyStateLabel");
    emptyLabel_->hide();
    mainLayout_->insertWidget(1, emptyLabel_);

    // Auto-load more on scroll.
    connect(tableView_->verticalScrollBar(), &QScrollBar::valueChanged, this, &FeedWidget::onScrollValueChanged);
}

void FeedWidget::onApplicationSet()
{
    if (app_ && app_->feed()) {
        connect(app_->feed(), &rats::service::FeedService::feedUpdated, this, &FeedWidget::refresh);
    }
    refresh();
}

void FeedWidget::refresh()
{
    torrents_.clear();
    loadedCount_ = 0;
    loading_ = false;
    loadMore();
}

void FeedWidget::loadMore()
{
    if (!app_ || !app_->feed() || loading_)
        return;

    loading_ = true;
    statusLabel_->setText(tr("Loading..."));

    const QVector<rats::service::FeedItem> items = app_->feed()->getFeed(loadedCount_, kPageSize);
    for (const rats::service::FeedItem& item : items) {
        const rats::domain::Torrent& t = item.torrent;
        // Skip adult / illegal content.
        if (t.contentCategory == rats::domain::ContentCategory::XXX
            || t.contentType == rats::domain::ContentType::Bad) {
            continue;
        }
        if (t.isValid()) {
            torrents_.append(t);
        }
    }

    loadedCount_ += items.size();
    setTorrents(torrents_);
    updateEmptyState();

    loading_ = false;
}

void FeedWidget::updateEmptyState()
{
    const bool isEmpty = torrents_.isEmpty();
    emptyLabel_->setVisible(isEmpty);
    tableView_->setVisible(!isEmpty);
    statusLabel_->setText(isEmpty ? tr("No items") : tr("%1 items").arg(torrents_.size()));
}

void FeedWidget::onScrollValueChanged(int value)
{
    QScrollBar* scrollBar = tableView_->verticalScrollBar();
    if (scrollBar && value >= scrollBar->maximum() - 50) {
        loadMore();
    }
}
