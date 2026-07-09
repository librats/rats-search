#include "toptorrentswidget.h"
#include "app/application.h"
#include "services/search_service.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabBar>
#include <QVBoxLayout>

TopTorrentsWidget::TopTorrentsWidget(QWidget* parent) : TorrentTableWidget(parent)
{
    categories_ = { "main", "video", "audio", "books", "pictures", "application", "archive" };
    categoryLabels_
        = { { "main", tr("All") }, { "video", tr("Video") }, { "audio", tr("Audio/Music") }, { "books", tr("Books") },
              { "pictures", tr("Pictures") }, { "application", tr("Apps/Games") }, { "archive", tr("Archives") } };

    timeFilters_ = { "overall", "hours", "week", "month" };
    timeLabels_ = { { "overall", tr("Overall") }, { "hours", tr("Last Hour") }, { "week", tr("Last Week") },
        { "month", tr("Last Month") } };

    setupControls();
}

void TopTorrentsWidget::setupControls()
{
    // Category tabs (above the table).
    categoryTabs_ = new QTabBar(this);
    categoryTabs_->setExpanding(false);
    categoryTabs_->setObjectName("categoryTabBar");
    for (const QString& cat : categories_) {
        categoryTabs_->addTab(categoryLabels_.value(cat, cat));
    }
    connect(categoryTabs_, &QTabBar::currentChanged, this, &TopTorrentsWidget::onCategoryChanged);
    mainLayout_->insertWidget(0, categoryTabs_);

    // Time filter row.
    QWidget* filterRow = new QWidget(this);
    filterRow->setObjectName("filterRow");
    QHBoxLayout* filterLayout = new QHBoxLayout(filterRow);
    filterLayout->setContentsMargins(12, 8, 12, 8);

    QLabel* filterLabel = new QLabel(tr("Time Period:"), this);
    filterLabel->setObjectName("filterLabel");
    filterLayout->addWidget(filterLabel);

    timeFilter_ = new QComboBox(this);
    for (const QString& time : timeFilters_) {
        timeFilter_->addItem(timeLabels_.value(time, time), time);
    }
    timeFilter_->setMinimumWidth(150);
    connect(timeFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &TopTorrentsWidget::onTimeFilterChanged);
    filterLayout->addWidget(timeFilter_);

    filterLayout->addStretch();

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("statusLabel");
    filterLayout->addWidget(statusLabel_);

    mainLayout_->insertWidget(1, filterRow);

    // "Load more" row (below the table).
    QWidget* bottomRow = new QWidget(this);
    bottomRow->setObjectName("bottomRow");
    QHBoxLayout* bottomLayout = new QHBoxLayout(bottomRow);
    bottomLayout->setContentsMargins(12, 8, 12, 8);
    bottomLayout->addStretch();

    moreButton_ = new QPushButton(tr("Load More Torrents"), this);
    moreButton_->setObjectName("secondaryButton");
    connect(moreButton_, &QPushButton::clicked, this, &TopTorrentsWidget::onMoreClicked);
    bottomLayout->addWidget(moreButton_);

    bottomLayout->addStretch();
    mainLayout_->addWidget(bottomRow);
}

QString TopTorrentsWidget::currentType() const
{
    return currentCategory_ == QLatin1String("main") ? QString() : currentCategory_;
}

void TopTorrentsWidget::refresh()
{
    loadedCount_ = 0;
    torrents_.clear();
    loadMore();
}

void TopTorrentsWidget::loadMore()
{
    if (!app_ || !app_->search())
        return;

    const QString time = currentTime_ == QLatin1String("overall") ? QString() : currentTime_;
    statusLabel_->setText(tr("Loading..."));

    QVector<rats::domain::Torrent> page = app_->search()->top(currentType(), time, loadedCount_, kPageSize);
    torrents_ += page;
    loadedCount_ = torrents_.size();

    setTorrents(torrents_);
    statusLabel_->setText(tr("%1 torrents").arg(torrents_.size()));
}

void TopTorrentsWidget::onCategoryChanged(int index)
{
    if (index >= 0 && index < categories_.size()) {
        currentCategory_ = categories_.at(index);
        refresh();
    }
}

void TopTorrentsWidget::onTimeFilterChanged(int index)
{
    if (index >= 0 && index < timeFilters_.size()) {
        currentTime_ = timeFilters_.at(index);
        refresh();
    }
}

void TopTorrentsWidget::onMoreClicked()
{
    loadMore();
}
