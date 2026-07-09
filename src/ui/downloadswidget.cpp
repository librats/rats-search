#include "downloadswidget.h"
#include "app/application.h"
#include "format.h"
#include "services/download_service.h"
#include <QDesktopServices>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollArea>
#include <QStyle>
#include <QUrl>

// DownloadItemWidget implementation

DownloadItemWidget::DownloadItemWidget(const QString& hash, const QString& name, qint64 size, QWidget* parent)
    : QWidget(parent)
    , hash_(hash)
    , nameLabel_(nullptr)
    , statusLabel_(nullptr)
    , speedLabel_(nullptr)
    , progressBar_(nullptr)
    , pauseButton_(nullptr)
    , cancelButton_(nullptr)
{
    setupUi(name, size);
}

void DownloadItemWidget::setupUi(const QString& name, qint64 size)
{
    setObjectName("downloadItemWidget");
    setMinimumHeight(100);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 12);
    mainLayout->setSpacing(8);

    // Top row: name and size
    QHBoxLayout* topRow = new QHBoxLayout();

    nameLabel_ = new QLabel(name, this);
    nameLabel_->setObjectName("downloadItemName");
    nameLabel_->setWordWrap(true);
    topRow->addWidget(nameLabel_, 1);

    sizeLabel_ = new QLabel(rats::ui::formatSize(size), this);
    sizeLabel_->setObjectName("downloadItemSize");
    topRow->addWidget(sizeLabel_);

    mainLayout->addLayout(topRow);

    // Middle row: progress bar
    progressBar_ = new QProgressBar(this);
    progressBar_->setMinimum(0);
    progressBar_->setMaximum(100);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(true);
    progressBar_->setObjectName("downloadProgress");
    mainLayout->addWidget(progressBar_);

    // Bottom row: status, speed, and buttons
    QHBoxLayout* bottomRow = new QHBoxLayout();

    statusLabel_ = new QLabel(tr("Waiting..."), this);
    statusLabel_->setObjectName("downloadItemStatus");
    bottomRow->addWidget(statusLabel_);

    speedLabel_ = new QLabel(this);
    speedLabel_->setObjectName("downloadItemSpeed");
    bottomRow->addWidget(speedLabel_);

    bottomRow->addStretch();

    pauseButton_ = new QPushButton(tr("Pause"), this);
    pauseButton_->setObjectName("secondaryButton");
    connect(pauseButton_, &QPushButton::clicked, this, [this]() { emit pauseToggled(hash_); });
    bottomRow->addWidget(pauseButton_);

    cancelButton_ = new QPushButton(tr("Cancel"), this);
    cancelButton_->setObjectName("dangerButton");
    connect(cancelButton_, &QPushButton::clicked, this, [this]() { emit cancelRequested(hash_); });
    bottomRow->addWidget(cancelButton_);

    mainLayout->addLayout(bottomRow);
}

void DownloadItemWidget::updateProgress(qint64 downloaded, qint64 total, int speed, double progress)
{
    progressBar_->setValue(static_cast<int>(progress * 100));

    QString status = QString("%1 / %2").arg(rats::ui::formatSize(downloaded), rats::ui::formatSize(total));
    if (total > 0 && speed > 0) {
        qint64 remaining = (total - downloaded) / speed;
        status += QString(" - %1 remaining").arg(formatTime(remaining));
    }
    statusLabel_->setText(status);

    if (speed > 0) {
        speedLabel_->setText(rats::ui::formatSpeed(speed));
    } else {
        speedLabel_->clear();
    }
}

void DownloadItemWidget::updateInfo(const QString& name, qint64 size)
{
    if (!name.isEmpty())
        nameLabel_->setText(name);
    if (size > 0)
        sizeLabel_->setText(rats::ui::formatSize(size));
}

void DownloadItemWidget::setCompleted()
{
    progressBar_->setValue(100);
    statusLabel_->setText(tr("Completed"));
    speedLabel_->clear();
    pauseButton_->setText(tr("Open"));
    pauseButton_->setObjectName("successButton");
    pauseButton_->style()->unpolish(pauseButton_);
    pauseButton_->style()->polish(pauseButton_);
    disconnect(pauseButton_, &QPushButton::clicked, nullptr, nullptr);
    connect(pauseButton_, &QPushButton::clicked, this, [this]() { emit openRequested(hash_); });
    cancelButton_->hide();
}

void DownloadItemWidget::setPaused(bool paused)
{
    if (paused) {
        pauseButton_->setText(tr("Resume"));
        statusLabel_->setText(tr("Paused"));
        progressBar_->setObjectName("downloadProgressPaused");
    } else {
        pauseButton_->setText(tr("Pause"));
        progressBar_->setObjectName("downloadProgress");
    }
    progressBar_->style()->unpolish(progressBar_);
    progressBar_->style()->polish(progressBar_);
}

QString DownloadItemWidget::formatTime(qint64 seconds) const
{
    if (seconds < 60)
        return QString::number(seconds) + "s";
    if (seconds < 3600)
        return QString("%1m %2s").arg(seconds / 60).arg(seconds % 60);
    return QString("%1h %2m").arg(seconds / 3600).arg((seconds % 3600) / 60);
}

// DownloadsWidget implementation

DownloadsWidget::DownloadsWidget(QWidget* parent)
    : QWidget(parent)
    , app_(nullptr)
    , listLayout_(nullptr)
    , emptyLabel_(nullptr)
    , statusLabel_(nullptr)
    , listContainer_(nullptr)
{
    setupUi();
}

DownloadsWidget::~DownloadsWidget() { }

void DownloadsWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header
    QWidget* headerRow = new QWidget(this);
    headerRow->setObjectName("headerRow");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(16, 12, 16, 12);

    QLabel* titleLabel = new QLabel(tr("📥 Downloads"), this);
    titleLabel->setObjectName("headerLabel");
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("statusLabel");
    headerLayout->addWidget(statusLabel_);

    mainLayout->addWidget(headerRow);

    // Empty state
    emptyLabel_ = new QLabel(tr("No active downloads.\nStart downloading "
                                "torrents from the search results!"),
        this);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setObjectName("emptyStateLabel");
    mainLayout->addWidget(emptyLabel_);

    // Scroll area for downloads list
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setObjectName("downloadsScrollArea");

    listContainer_ = new QWidget(this);
    listContainer_->setObjectName("downloadsListContainer");
    listLayout_ = new QVBoxLayout(listContainer_);
    listLayout_->setContentsMargins(12, 12, 12, 12);
    listLayout_->setSpacing(8);
    listLayout_->addStretch();

    scrollArea->setWidget(listContainer_);
    listContainer_->hide();
    mainLayout->addWidget(scrollArea, 1);
}

void DownloadsWidget::setApplication(rats::app::Application* app)
{
    app_ = app;

    if (app_ && app_->downloads()) {
        rats::service::DownloadService* downloads = app_->downloads();
        connect(downloads, &rats::service::DownloadService::downloadStarted, this, &DownloadsWidget::onDownloadStarted);
        connect(downloads, &rats::service::DownloadService::progressUpdated, this, &DownloadsWidget::onProgressUpdated);
        connect(
            downloads, &rats::service::DownloadService::downloadCompleted, this, &DownloadsWidget::onDownloadCompleted);
        connect(
            downloads, &rats::service::DownloadService::torrentRemoved, this, &DownloadsWidget::onDownloadCancelled);
        loadDownloads();
    }
}

void DownloadsWidget::refresh()
{
    loadDownloads();
}

void DownloadsWidget::loadDownloads()
{
    if (!app_ || !app_->downloads())
        return;

    // Clear existing items
    for (auto it = downloadItems_.begin(); it != downloadItems_.end(); ++it) {
        (*it)->deleteLater();
    }
    downloadItems_.clear();

    // Load current downloads
    const QVector<rats::service::Download> downloads = app_->downloads()->allDownloads();
    for (const rats::service::Download& dl : downloads) {
        addDownloadItem(dl.hash, dl.name, dl.totalSize);

        // Update progress if available
        if (dl.progress > 0) {
            downloadItems_[dl.hash]->updateProgress(
                dl.downloadedBytes, dl.totalSize, static_cast<int>(dl.downloadSpeed), dl.progress);
        }

        if (dl.completed) {
            downloadItems_[dl.hash]->setCompleted();
        } else if (dl.paused) {
            downloadItems_[dl.hash]->setPaused(true);
        }
    }

    if (downloadItems_.isEmpty()) {
        emptyLabel_->show();
        listContainer_->hide();
        statusLabel_->clear();
    } else {
        emptyLabel_->hide();
        listContainer_->show();
        statusLabel_->setText(tr("%1 download(s)").arg(downloadItems_.size()));
    }
}

void DownloadsWidget::addDownloadItem(const QString& hash, const QString& name, qint64 size)
{
    if (downloadItems_.contains(hash))
        return;

    DownloadItemWidget* item = new DownloadItemWidget(hash, name, size, this);

    connect(item, &DownloadItemWidget::pauseToggled, this, &DownloadsWidget::onPauseToggled);
    connect(item, &DownloadItemWidget::cancelRequested, this, &DownloadsWidget::onCancelRequested);
    connect(item, &DownloadItemWidget::openRequested, this, &DownloadsWidget::onOpenRequested);

    // Insert before the stretch
    listLayout_->insertWidget(listLayout_->count() - 1, item);
    downloadItems_[hash] = item;

    emptyLabel_->hide();
    listContainer_->show();
    statusLabel_->setText(tr("%1 download(s)").arg(downloadItems_.size()));
}

void DownloadsWidget::removeDownloadItem(const QString& hash)
{
    if (!downloadItems_.contains(hash))
        return;

    DownloadItemWidget* item = downloadItems_.take(hash);
    listLayout_->removeWidget(item);
    item->deleteLater();

    if (downloadItems_.isEmpty()) {
        emptyLabel_->show();
        listContainer_->hide();
        statusLabel_->clear();
    } else {
        statusLabel_->setText(tr("%1 download(s)").arg(downloadItems_.size()));
    }
}

void DownloadsWidget::onDownloadStarted(const QString& hash)
{
    if (!app_ || !app_->downloads())
        return;

    const rats::service::Download info = app_->downloads()->getDownload(hash);
    if (info.hash.isEmpty())
        return;

    // downloadStarted fires twice: once on add() (name is the info-hash
    // placeholder, size 0) and again when a magnet's metadata arrives (real name
    // and size known). Refresh the existing item on the second call instead of
    // returning early, otherwise it keeps showing the hash as its name.
    if (downloadItems_.contains(hash))
        downloadItems_[hash]->updateInfo(info.name, info.totalSize);
    else
        addDownloadItem(hash, info.name, info.totalSize);
}

void DownloadsWidget::onProgressUpdated(const QString& hash, const QJsonObject& progress)
{
    if (downloadItems_.contains(hash)) {
        qint64 downloaded = progress["downloaded"].toVariant().toLongLong();
        qint64 total = progress["total"].toVariant().toLongLong();
        int speed = progress["speed"].toInt();
        double progressPercent = progress["progress"].toDouble();
        downloadItems_[hash]->updateProgress(downloaded, total, speed, progressPercent);

        // Reflect paused state carried in the progress payload.
        if (progress.contains("paused")) {
            downloadItems_[hash]->setPaused(progress["paused"].toBool());
        }
    }
}

void DownloadsWidget::onDownloadCompleted(const QString& hash)
{
    if (downloadItems_.contains(hash)) {
        downloadItems_[hash]->setCompleted();
    }
}

void DownloadsWidget::onDownloadCancelled(const QString& hash)
{
    removeDownloadItem(hash);
}

void DownloadsWidget::onPauseToggled(const QString& hash)
{
    if (!app_ || !app_->downloads())
        return;

    rats::service::DownloadService* downloads = app_->downloads();
    downloads->togglePause(hash);

    const rats::service::Download info = downloads->getDownload(hash);
    if (downloadItems_.contains(hash)) {
        downloadItems_[hash]->setPaused(info.paused);
    }
}

void DownloadsWidget::onCancelRequested(const QString& hash)
{
    if (app_ && app_->downloads()) {
        app_->downloads()->remove(hash);
    }
}

void DownloadsWidget::onOpenRequested(const QString& hash)
{
    if (!app_ || !app_->downloads())
        return;

    const QString savePath = app_->downloads()->getDownload(hash).savePath;
    if (savePath.isEmpty() || !QDir(savePath).exists()) {
        statusLabel_->setText(tr("Download folder not found"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(savePath));
}
