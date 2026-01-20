#include "downloadswidget.h"
#include "api/ratsapi.h"
#include "api/downloadmanager.h"
#include <QScrollArea>
#include <QJsonArray>
#include <QJsonObject>

// DownloadItemWidget implementation

DownloadItemWidget::DownloadItemWidget(const QString& hash, const QString& name, 
                                       qint64 size, QWidget* parent)
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
    setStyleSheet(R"(
        DownloadItemWidget {
            background-color: #2d2d2d;
            border: 1px solid #3c3f41;
            border-radius: 8px;
            margin: 4px;
        }
        DownloadItemWidget:hover {
            border-color: #4a9eff;
        }
    )");
    setMinimumHeight(100);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 12);
    mainLayout->setSpacing(8);
    
    // Top row: name and size
    QHBoxLayout* topRow = new QHBoxLayout();
    
    nameLabel_ = new QLabel(name, this);
    nameLabel_->setStyleSheet("font-size: 14px; font-weight: bold; color: #ffffff;");
    nameLabel_->setWordWrap(true);
    topRow->addWidget(nameLabel_, 1);
    
    QLabel* sizeLabel = new QLabel(formatBytes(size), this);
    sizeLabel->setStyleSheet("font-size: 12px; color: #888888;");
    topRow->addWidget(sizeLabel);
    
    mainLayout->addLayout(topRow);
    
    // Middle row: progress bar
    progressBar_ = new QProgressBar(this);
    progressBar_->setMinimum(0);
    progressBar_->setMaximum(100);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(true);
    progressBar_->setStyleSheet(R"(
        QProgressBar {
            border: none;
            border-radius: 4px;
            background-color: #1e1e1e;
            height: 16px;
            text-align: center;
            font-size: 11px;
            color: #ffffff;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #4a9eff, stop:1 #66b2ff);
            border-radius: 4px;
        }
    )");
    mainLayout->addWidget(progressBar_);
    
    // Bottom row: status, speed, and buttons
    QHBoxLayout* bottomRow = new QHBoxLayout();
    
    statusLabel_ = new QLabel(tr("Waiting..."), this);
    statusLabel_->setStyleSheet("font-size: 12px; color: #888888;");
    bottomRow->addWidget(statusLabel_);
    
    speedLabel_ = new QLabel(this);
    speedLabel_->setStyleSheet("font-size: 12px; color: #4a9eff; font-weight: bold;");
    bottomRow->addWidget(speedLabel_);
    
    bottomRow->addStretch();
    
    pauseButton_ = new QPushButton(tr("Pause"), this);
    pauseButton_->setStyleSheet(R"(
        QPushButton {
            background-color: #3c3f41;
            color: #ffffff;
            border: 1px solid #555555;
            border-radius: 4px;
            padding: 6px 16px;
            font-size: 11px;
        }
        QPushButton:hover {
            background-color: #4c4f51;
            border-color: #4a9eff;
        }
    )");
    connect(pauseButton_, &QPushButton::clicked, this, [this]() {
        emit pauseToggled(hash_);
    });
    bottomRow->addWidget(pauseButton_);
    
    cancelButton_ = new QPushButton(tr("Cancel"), this);
    cancelButton_->setStyleSheet(R"(
        QPushButton {
            background-color: #8b3a3a;
            color: #ffffff;
            border: 1px solid #aa5555;
            border-radius: 4px;
            padding: 6px 16px;
            font-size: 11px;
        }
        QPushButton:hover {
            background-color: #a04040;
            border-color: #cc6666;
        }
    )");
    connect(cancelButton_, &QPushButton::clicked, this, [this]() {
        emit cancelRequested(hash_);
    });
    bottomRow->addWidget(cancelButton_);
    
    mainLayout->addLayout(bottomRow);
}

void DownloadItemWidget::updateProgress(qint64 downloaded, qint64 total, int speed, double progress)
{
    progressBar_->setValue(static_cast<int>(progress * 100));
    
    QString status = QString("%1 / %2").arg(formatBytes(downloaded), formatBytes(total));
    if (total > 0 && speed > 0) {
        qint64 remaining = (total - downloaded) / speed;
        status += QString(" - %1 remaining").arg(formatTime(remaining));
    }
    statusLabel_->setText(status);
    
    if (speed > 0) {
        speedLabel_->setText(formatSpeed(speed));
    } else {
        speedLabel_->clear();
    }
}

void DownloadItemWidget::setCompleted()
{
    completed_ = true;
    progressBar_->setValue(100);
    statusLabel_->setText(tr("Completed"));
    speedLabel_->clear();
    pauseButton_->setText(tr("Open"));
    pauseButton_->setStyleSheet(R"(
        QPushButton {
            background-color: #3a8b4a;
            color: #ffffff;
            border: 1px solid #55aa55;
            border-radius: 4px;
            padding: 6px 16px;
            font-size: 11px;
        }
        QPushButton:hover {
            background-color: #40a050;
            border-color: #66cc66;
        }
    )");
    disconnect(pauseButton_, &QPushButton::clicked, nullptr, nullptr);
    connect(pauseButton_, &QPushButton::clicked, this, [this]() {
        emit openRequested(hash_);
    });
    cancelButton_->hide();
}

void DownloadItemWidget::setPaused(bool paused)
{
    paused_ = paused;
    if (paused) {
        pauseButton_->setText(tr("Resume"));
        statusLabel_->setText(tr("Paused"));
        progressBar_->setStyleSheet(R"(
            QProgressBar {
                border: none;
                border-radius: 4px;
                background-color: #1e1e1e;
                height: 16px;
                text-align: center;
                font-size: 11px;
                color: #ffffff;
            }
            QProgressBar::chunk {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 #888888, stop:1 #aaaaaa);
                border-radius: 4px;
            }
        )");
    } else {
        pauseButton_->setText(tr("Pause"));
        progressBar_->setStyleSheet(R"(
            QProgressBar {
                border: none;
                border-radius: 4px;
                background-color: #1e1e1e;
                height: 16px;
                text-align: center;
                font-size: 11px;
                color: #ffffff;
            }
            QProgressBar::chunk {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 #4a9eff, stop:1 #66b2ff);
                border-radius: 4px;
            }
        )");
    }
}

QString DownloadItemWidget::formatBytes(qint64 bytes) const
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024 * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}

QString DownloadItemWidget::formatSpeed(int bytesPerSec) const
{
    if (bytesPerSec < 1024) return QString::number(bytesPerSec) + " B/s";
    if (bytesPerSec < 1024 * 1024) return QString::number(bytesPerSec / 1024.0, 'f', 1) + " KB/s";
    return QString::number(bytesPerSec / (1024.0 * 1024.0), 'f', 1) + " MB/s";
}

QString DownloadItemWidget::formatTime(qint64 seconds) const
{
    if (seconds < 60) return QString::number(seconds) + "s";
    if (seconds < 3600) return QString("%1m %2s").arg(seconds / 60).arg(seconds % 60);
    return QString("%1h %2m").arg(seconds / 3600).arg((seconds % 3600) / 60);
}

// DownloadsWidget implementation

DownloadsWidget::DownloadsWidget(QWidget *parent)
    : QWidget(parent)
    , api_(nullptr)
    , downloadManager_(nullptr)
    , listLayout_(nullptr)
    , emptyLabel_(nullptr)
    , statusLabel_(nullptr)
    , listContainer_(nullptr)
{
    setupUi();
}

DownloadsWidget::~DownloadsWidget()
{
}

void DownloadsWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Header
    QWidget* headerRow = new QWidget(this);
    headerRow->setStyleSheet("background-color: #252526;");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(16, 12, 16, 12);
    
    QLabel* titleLabel = new QLabel(tr("📥 Downloads"), this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #4a9eff;");
    headerLayout->addWidget(titleLabel);
    
    headerLayout->addStretch();
    
    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet("color: #666666; font-size: 11px;");
    headerLayout->addWidget(statusLabel_);
    
    mainLayout->addWidget(headerRow);
    
    // Empty state
    emptyLabel_ = new QLabel(tr("No active downloads.\nStart downloading torrents from the search results!"), this);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setStyleSheet("font-size: 14px; color: #666666; padding: 40px;");
    mainLayout->addWidget(emptyLabel_);
    
    // Scroll area for downloads list
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet("QScrollArea { border: none; background-color: #1e1e1e; }");
    
    listContainer_ = new QWidget(this);
    listContainer_->setStyleSheet("background-color: #1e1e1e;");
    listLayout_ = new QVBoxLayout(listContainer_);
    listLayout_->setContentsMargins(12, 12, 12, 12);
    listLayout_->setSpacing(8);
    listLayout_->addStretch();
    
    scrollArea->setWidget(listContainer_);
    listContainer_->hide();
    mainLayout->addWidget(scrollArea, 1);
}

void DownloadsWidget::setApi(RatsAPI* api)
{
    api_ = api;
}

void DownloadsWidget::setDownloadManager(DownloadManager* manager)
{
    downloadManager_ = manager;
    if (manager) {
        connect(manager, &DownloadManager::downloadStarted, this, &DownloadsWidget::onDownloadStarted);
        connect(manager, &DownloadManager::progressUpdated, this, &DownloadsWidget::onProgressUpdated);
        connect(manager, &DownloadManager::downloadCompleted, this, &DownloadsWidget::onDownloadCompleted);
        connect(manager, &DownloadManager::downloadCancelled, this, &DownloadsWidget::onDownloadCancelled);
        loadDownloads();
    }
}

void DownloadsWidget::refresh()
{
    loadDownloads();
}

void DownloadsWidget::loadDownloads()
{
    if (!downloadManager_) return;
    
    // Clear existing items
    for (auto it = downloadItems_.begin(); it != downloadItems_.end(); ++it) {
        (*it)->deleteLater();
    }
    downloadItems_.clear();
    
    // Load current downloads
    QVector<DownloadInfo> downloads = downloadManager_->getAllDownloads();
    for (const DownloadInfo& dl : downloads) {
        addDownloadItem(dl.hash, dl.name, dl.size);
        
        // Update progress if available
        if (dl.progress > 0) {
            downloadItems_[dl.hash]->updateProgress(dl.downloaded, dl.size, dl.downloadSpeed, dl.progress);
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
    if (downloadItems_.contains(hash)) return;
    
    DownloadItemWidget* item = new DownloadItemWidget(hash, name, size, this);
    
    connect(item, &DownloadItemWidget::pauseToggled, this, &DownloadsWidget::onPauseToggled);
    connect(item, &DownloadItemWidget::cancelRequested, this, &DownloadsWidget::onCancelRequested);
    
    // Insert before the stretch
    listLayout_->insertWidget(listLayout_->count() - 1, item);
    downloadItems_[hash] = item;
    
    emptyLabel_->hide();
    listContainer_->show();
    statusLabel_->setText(tr("%1 download(s)").arg(downloadItems_.size()));
}

void DownloadsWidget::removeDownloadItem(const QString& hash)
{
    if (!downloadItems_.contains(hash)) return;
    
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
    if (downloadManager_) {
        DownloadInfo info = downloadManager_->getDownload(hash);
        if (!info.hash.isEmpty()) {
            addDownloadItem(hash, info.name, info.size);
        }
    }
}

void DownloadsWidget::onProgressUpdated(const QString& hash, const QJsonObject& progress)
{
    if (downloadItems_.contains(hash)) {
        qint64 downloaded = progress["downloaded"].toVariant().toLongLong();
        qint64 total = progress["total"].toVariant().toLongLong();
        int speed = progress["speed"].toInt();
        double progressPercent = progress["progress"].toDouble();
        downloadItems_[hash]->updateProgress(downloaded, total, speed, progressPercent);
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
    if (downloadManager_) {
        downloadManager_->togglePause(hash);
        DownloadInfo info = downloadManager_->getDownload(hash);
        if (downloadItems_.contains(hash)) {
            downloadItems_[hash]->setPaused(info.paused);
        }
    }
}

void DownloadsWidget::onCancelRequested(const QString& hash)
{
    if (downloadManager_) {
        downloadManager_->cancel(hash);
    }
}
