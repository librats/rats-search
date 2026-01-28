#include "torrentdetailspanel.h"
#include "torrentitemdelegate.h"
#include "api/ratsapi.h"
#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QDateTime>
#include <QFont>
#include <QScrollArea>
#include <QFrame>
#include <QTimer>
#include <QStyle>

TorrentDetailsPanel::TorrentDetailsPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    clear();
}

TorrentDetailsPanel::~TorrentDetailsPanel()
{
}

void TorrentDetailsPanel::setupUi()
{
    setObjectName("detailsPanel");
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 16);
    mainLayout->setSpacing(12);
    
    // Header with close button
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);
    
    // Content type icon
    contentTypeIcon_ = new QWidget();
    contentTypeIcon_->setFixedSize(32, 32);
    contentTypeIcon_->setObjectName("contentTypeIcon");
    headerLayout->addWidget(contentTypeIcon_);
    
    // Title
    titleLabel_ = new QLabel(tr("Select a torrent"));
    titleLabel_->setWordWrap(true);
    titleLabel_->setObjectName("detailsTitleLabel");
    headerLayout->addWidget(titleLabel_, 1);
    
    // Close button
    closeButton_ = new QPushButton("×");
    closeButton_->setObjectName("closeButton");
    closeButton_->setFixedSize(28, 28);
    closeButton_->setCursor(Qt::PointingHandCursor);
    connect(closeButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::closeRequested);
    headerLayout->addWidget(closeButton_);
    
    mainLayout->addLayout(headerLayout);
    
    // Content type label
    contentTypeLabel_ = new QLabel();
    contentTypeLabel_->setObjectName("contentTypeLabel");
    mainLayout->addWidget(contentTypeLabel_);
    
    // Separator
    QFrame *sep1 = new QFrame();
    sep1->setFrameShape(QFrame::HLine);
    sep1->setObjectName("detailsSeparator");
    sep1->setFixedHeight(1);
    mainLayout->addWidget(sep1);
    
    // Stats section (seeders, leechers, completed)
    QLabel *statsTitle = new QLabel(tr("Statistics"));
    statsTitle->setObjectName("sectionTitle");
    mainLayout->addWidget(statsTitle);
    
    QHBoxLayout *statsLayout = new QHBoxLayout();
    statsLayout->setSpacing(16);
    
    // Seeders
    QVBoxLayout *seedersLayout = new QVBoxLayout();
    seedersLabel_ = new QLabel("0");
    seedersLabel_->setObjectName("seedersLabel");
    seedersLabel_->setAlignment(Qt::AlignCenter);
    QLabel *seedersText = new QLabel(tr("Seeders"));
    seedersText->setObjectName("statsSubLabel");
    seedersText->setAlignment(Qt::AlignCenter);
    seedersLayout->addWidget(seedersLabel_);
    seedersLayout->addWidget(seedersText);
    statsLayout->addLayout(seedersLayout);
    
    // Leechers
    QVBoxLayout *leechersLayout = new QVBoxLayout();
    leechersLabel_ = new QLabel("0");
    leechersLabel_->setObjectName("leechersLabel");
    leechersLabel_->setAlignment(Qt::AlignCenter);
    QLabel *leechersText = new QLabel(tr("Leechers"));
    leechersText->setObjectName("statsSubLabel");
    leechersText->setAlignment(Qt::AlignCenter);
    leechersLayout->addWidget(leechersLabel_);
    leechersLayout->addWidget(leechersText);
    statsLayout->addLayout(leechersLayout);
    
    // Completed
    QVBoxLayout *completedLayout = new QVBoxLayout();
    completedLabel_ = new QLabel("0");
    completedLabel_->setObjectName("completedLabel");
    completedLabel_->setAlignment(Qt::AlignCenter);
    QLabel *completedText = new QLabel(tr("Completed"));
    completedText->setObjectName("statsSubLabel");
    completedText->setAlignment(Qt::AlignCenter);
    completedLayout->addWidget(completedLabel_);
    completedLayout->addWidget(completedText);
    statsLayout->addLayout(completedLayout);
    
    mainLayout->addLayout(statsLayout);
    
    // Rating bar
    QHBoxLayout *ratingLayout = new QHBoxLayout();
    ratingBar_ = new QProgressBar();
    ratingBar_->setObjectName("ratingBar");
    ratingBar_->setRange(0, 100);
    ratingBar_->setValue(0);
    ratingBar_->setTextVisible(false);
    ratingBar_->setFixedHeight(6);
    ratingLayout->addWidget(ratingBar_, 1);
    ratingLabel_ = new QLabel("N/A");
    ratingLabel_->setObjectName("ratingLabel");
    ratingLayout->addWidget(ratingLabel_);
    mainLayout->addLayout(ratingLayout);
    
    // Voting buttons (migrated from legacy/app/torrent-page.js)
    QHBoxLayout *votingLayout = new QHBoxLayout();
    votingLayout->setSpacing(8);
    
    goodVoteButton_ = new QPushButton(tr("👍 Good"));
    goodVoteButton_->setObjectName("goodVoteButton");
    goodVoteButton_->setCursor(Qt::PointingHandCursor);
    connect(goodVoteButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::onGoodVoteClicked);
    votingLayout->addWidget(goodVoteButton_);
    
    badVoteButton_ = new QPushButton(tr("👎 Bad"));
    badVoteButton_->setObjectName("badVoteButton");
    badVoteButton_->setCursor(Qt::PointingHandCursor);
    connect(badVoteButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::onBadVoteClicked);
    votingLayout->addWidget(badVoteButton_);
    
    votingLayout->addStretch();
    
    votesLabel_ = new QLabel();
    votesLabel_->setObjectName("votesLabel");
    votingLayout->addWidget(votesLabel_);
    
    mainLayout->addLayout(votingLayout);
    
    // Download progress section (hidden by default)
    downloadProgressWidget_ = new QWidget();
    downloadProgressWidget_->setObjectName("downloadProgressWidget");
    QVBoxLayout *downloadLayout = new QVBoxLayout(downloadProgressWidget_);
    downloadLayout->setContentsMargins(12, 8, 12, 8);
    downloadLayout->setSpacing(6);
    
    QHBoxLayout *downloadHeaderLayout = new QHBoxLayout();
    QLabel *downloadTitle = new QLabel(tr("📥 Downloading..."));
    downloadTitle->setObjectName("downloadTitleLabel");
    downloadHeaderLayout->addWidget(downloadTitle);
    downloadHeaderLayout->addStretch();
    downloadSpeedLabel_ = new QLabel();
    downloadSpeedLabel_->setObjectName("downloadSpeedLabel");
    downloadHeaderLayout->addWidget(downloadSpeedLabel_);
    downloadLayout->addLayout(downloadHeaderLayout);
    
    downloadProgressBar_ = new QProgressBar();
    downloadProgressBar_->setObjectName("downloadProgressBarDetails");
    downloadProgressBar_->setRange(0, 100);
    downloadProgressBar_->setValue(0);
    downloadProgressBar_->setTextVisible(true);
    downloadProgressBar_->setFixedHeight(20);
    downloadLayout->addWidget(downloadProgressBar_);
    
    QHBoxLayout *downloadStatusLayout = new QHBoxLayout();
    downloadStatusLabel_ = new QLabel();
    downloadStatusLabel_->setObjectName("downloadStatusLabel");
    downloadStatusLayout->addWidget(downloadStatusLabel_);
    downloadStatusLayout->addStretch();
    cancelDownloadButton_ = new QPushButton(tr("Cancel"));
    cancelDownloadButton_->setObjectName("cancelDownloadButton");
    cancelDownloadButton_->setCursor(Qt::PointingHandCursor);
    connect(cancelDownloadButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::onCancelDownloadClicked);
    downloadStatusLayout->addWidget(cancelDownloadButton_);
    downloadLayout->addLayout(downloadStatusLayout);
    
    downloadProgressWidget_->hide();
    mainLayout->addWidget(downloadProgressWidget_);
    
    // Info section
    QLabel *infoTitle = new QLabel(tr("Information"));
    infoTitle->setObjectName("sectionTitle");
    mainLayout->addWidget(infoTitle);
    
    // Size
    QHBoxLayout *sizeRow = new QHBoxLayout();
    QLabel *sizeTitle = new QLabel(tr("Size:"));
    sizeTitle->setObjectName("infoLabel");
    sizeTitle->setFixedWidth(80);
    sizeLabel_ = new QLabel("-");
    sizeLabel_->setObjectName("infoValue");
    sizeRow->addWidget(sizeTitle);
    sizeRow->addWidget(sizeLabel_, 1);
    mainLayout->addLayout(sizeRow);
    
    // Files
    QHBoxLayout *filesRow = new QHBoxLayout();
    QLabel *filesTitle = new QLabel(tr("Files:"));
    filesTitle->setObjectName("infoLabel");
    filesTitle->setFixedWidth(80);
    filesLabel_ = new QLabel("-");
    filesLabel_->setObjectName("infoValue");
    filesRow->addWidget(filesTitle);
    filesRow->addWidget(filesLabel_, 1);
    mainLayout->addLayout(filesRow);
    
    // Date
    QHBoxLayout *dateRow = new QHBoxLayout();
    QLabel *dateTitle = new QLabel(tr("Added:"));
    dateTitle->setObjectName("infoLabel");
    dateTitle->setFixedWidth(80);
    dateLabel_ = new QLabel("-");
    dateLabel_->setObjectName("infoValue");
    dateRow->addWidget(dateTitle);
    dateRow->addWidget(dateLabel_, 1);
    mainLayout->addLayout(dateRow);
    
    // Category
    QHBoxLayout *categoryRow = new QHBoxLayout();
    QLabel *categoryTitle = new QLabel(tr("Category:"));
    categoryTitle->setObjectName("infoLabel");
    categoryTitle->setFixedWidth(80);
    categoryLabel_ = new QLabel("-");
    categoryLabel_->setObjectName("infoValue");
    categoryRow->addWidget(categoryTitle);
    categoryRow->addWidget(categoryLabel_, 1);
    mainLayout->addLayout(categoryRow);
    
    // Hash section
    QLabel *hashTitle = new QLabel(tr("Info Hash"));
    hashTitle->setObjectName("sectionTitle");
    mainLayout->addWidget(hashTitle);
    
    hashLabel_ = new QLabel("-");
    hashLabel_->setObjectName("hashLabel");
    hashLabel_->setWordWrap(true);
    hashLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(hashLabel_);
    
    // Spacer
    mainLayout->addStretch();
    
    // Action buttons
    QLabel *actionsTitle = new QLabel(tr("Actions"));
    actionsTitle->setObjectName("sectionTitle");
    mainLayout->addWidget(actionsTitle);
    
    // Magnet button
    magnetButton_ = new QPushButton(tr("Open Magnet Link"));
    magnetButton_->setObjectName("magnetButton");
    magnetButton_->setCursor(Qt::PointingHandCursor);
    connect(magnetButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::onMagnetClicked);
    mainLayout->addWidget(magnetButton_);
    
    // Download button
    downloadButton_ = new QPushButton(tr("Download"));
    downloadButton_->setObjectName("successButton");
    downloadButton_->setCursor(Qt::PointingHandCursor);
    connect(downloadButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::onDownloadClicked);
    mainLayout->addWidget(downloadButton_);
    
    // Copy hash button
    copyHashButton_ = new QPushButton(tr("Copy Info Hash"));
    copyHashButton_->setObjectName("secondaryButton");
    copyHashButton_->setCursor(Qt::PointingHandCursor);
    connect(copyHashButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::onCopyHashClicked);
    mainLayout->addWidget(copyHashButton_);
}

void TorrentDetailsPanel::setTorrent(const TorrentInfo &torrent)
{
    currentTorrent_ = torrent;
    currentHash_ = torrent.hash;
    hasVoted_ = false;
    
    // Update UI
    titleLabel_->setText(torrent.name);
    
    // Content type
    QString contentType = torrent.contentType.isEmpty() ? "unknown" : torrent.contentType;
    QColor typeColor = TorrentItemDelegate::getContentTypeColor(contentType);
    contentTypeIcon_->setProperty("typeColor", typeColor.name());
    contentTypeIcon_->style()->unpolish(contentTypeIcon_);
    contentTypeIcon_->style()->polish(contentTypeIcon_);
    contentTypeLabel_->setText(TorrentItemDelegate::getContentTypeName(contentType));
    
    // Stats
    seedersLabel_->setText(QString::number(torrent.seeders));
    leechersLabel_->setText(QString::number(torrent.leechers));
    completedLabel_->setText(QString::number(torrent.completed));
    
    // Info
    sizeLabel_->setText(formatBytes(torrent.size));
    filesLabel_->setText(tr("%1 files").arg(torrent.files));
    dateLabel_->setText(torrent.added.isValid() ? torrent.added.toString("MMMM d, yyyy") : "-");
    categoryLabel_->setText(torrent.contentCategory.isEmpty() ? tr("General") : torrent.contentCategory);
    
    // Hash
    hashLabel_->setText(torrent.hash);
    
    // Rating
    updateRatingDisplay();
    updateVotingButtons();
    
    setVisible(true);
}

void TorrentDetailsPanel::clear()
{
    currentHash_.clear();
    currentTorrent_ = TorrentInfo();
    hasVoted_ = false;
    
    titleLabel_->setText(tr("Select a torrent"));
    contentTypeIcon_->setProperty("typeColor", "#888888");
    contentTypeIcon_->style()->unpolish(contentTypeIcon_);
    contentTypeIcon_->style()->polish(contentTypeIcon_);
    contentTypeLabel_->clear();
    
    seedersLabel_->setText("0");
    leechersLabel_->setText("0");
    completedLabel_->setText("0");
    
    sizeLabel_->setText("-");
    filesLabel_->setText("-");
    dateLabel_->setText("-");
    categoryLabel_->setText("-");
    hashLabel_->setText("-");
    
    ratingBar_->setValue(0);
    ratingLabel_->setText("N/A");
    votesLabel_->setText(tr("No votes yet"));
}

void TorrentDetailsPanel::updateRatingDisplay()
{
    int good = currentTorrent_.good;
    int bad = currentTorrent_.bad;
    
    if (good == 0 && bad == 0) {
        ratingBar_->setValue(0);
        ratingBar_->setProperty("ratingType", "neutral");
        ratingLabel_->setText(tr("No ratings"));
        ratingLabel_->setProperty("ratingType", "neutral");
    } else {
        int rating = static_cast<int>((static_cast<double>(good) / (good + bad)) * 100);
        ratingBar_->setValue(rating);
        
        QString ratingType = rating >= 50 ? "good" : "bad";
        ratingBar_->setProperty("ratingType", ratingType);
        ratingLabel_->setText(QString("%1%").arg(rating));
        ratingLabel_->setProperty("ratingType", ratingType);
    }
    
    ratingBar_->style()->unpolish(ratingBar_);
    ratingBar_->style()->polish(ratingBar_);
    ratingLabel_->style()->unpolish(ratingLabel_);
    ratingLabel_->style()->polish(ratingLabel_);
}

QString TorrentDetailsPanel::formatBytes(qint64 bytes) const
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    const qint64 TB = GB * 1024;
    
    if (bytes >= TB) {
        return QString::number(bytes / static_cast<double>(TB), 'f', 2) + " TB";
    } else if (bytes >= GB) {
        return QString::number(bytes / static_cast<double>(GB), 'f', 2) + " GB";
    } else if (bytes >= MB) {
        return QString::number(bytes / static_cast<double>(MB), 'f', 2) + " MB";
    } else if (bytes >= KB) {
        return QString::number(bytes / static_cast<double>(KB), 'f', 2) + " KB";
    } else {
        return QString::number(bytes) + " B";
    }
}

QString TorrentDetailsPanel::formatDate(qint64 timestamp) const
{
    if (timestamp <= 0) return "-";
    QDateTime dt = QDateTime::fromSecsSinceEpoch(timestamp);
    return dt.toString("MMMM d, yyyy");
}

void TorrentDetailsPanel::onMagnetClicked()
{
    if (currentHash_.isEmpty()) return;
    
    QString magnetLink = QString("magnet:?xt=urn:btih:%1&dn=%2")
        .arg(currentHash_)
        .arg(QUrl::toPercentEncoding(currentTorrent_.name));
    
    QDesktopServices::openUrl(QUrl(magnetLink));
    emit magnetLinkRequested(currentHash_, currentTorrent_.name);
}

void TorrentDetailsPanel::onDownloadClicked()
{
    if (currentHash_.isEmpty()) return;
    emit downloadRequested(currentHash_);
}

void TorrentDetailsPanel::onCopyHashClicked()
{
    if (currentHash_.isEmpty()) return;
    
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(currentHash_);
    
    // Visual feedback
    copyHashButton_->setText(tr("Copied!"));
    QTimer::singleShot(2000, this, [this]() {
        copyHashButton_->setText(tr("Copy Info Hash"));
    });
}

void TorrentDetailsPanel::setApi(RatsAPI* api)
{
    api_ = api;
    if (api_) {
        connect(api_, &RatsAPI::votesUpdated, this, &TorrentDetailsPanel::onVotesUpdated);
    }
}

void TorrentDetailsPanel::onGoodVoteClicked()
{
    if (currentHash_.isEmpty()) return;
    
    hasVoted_ = true;
    updateVotingButtons();
    
    if (api_) {
        api_->vote(currentHash_, true, [](const ApiResponse&) {});
    }
    emit voteRequested(currentHash_, true);
}

void TorrentDetailsPanel::onBadVoteClicked()
{
    if (currentHash_.isEmpty()) return;
    
    hasVoted_ = true;
    updateVotingButtons();
    
    if (api_) {
        api_->vote(currentHash_, false, [](const ApiResponse&) {});
    }
    emit voteRequested(currentHash_, false);
}

void TorrentDetailsPanel::onVotesUpdated(const QString& hash, int good, int bad)
{
    if (hash != currentHash_) return;
    
    currentTorrent_.good = good;
    currentTorrent_.bad = bad;
    updateRatingDisplay();
    updateVotingButtons();
}

void TorrentDetailsPanel::updateVotingButtons()
{
    int total = currentTorrent_.good + currentTorrent_.bad;
    if (total > 0) {
        votesLabel_->setText(tr("%1 votes").arg(total));
    } else {
        votesLabel_->setText(tr("No votes yet"));
    }
    
    goodVoteButton_->setEnabled(!hasVoted_);
    badVoteButton_->setEnabled(!hasVoted_);
    
    if (hasVoted_) {
        goodVoteButton_->setText(tr("👍 Voted"));
        badVoteButton_->setText(tr("👎 Voted"));
    } else {
        goodVoteButton_->setText(tr("👍 Good"));
        badVoteButton_->setText(tr("👎 Bad"));
    }
}

void TorrentDetailsPanel::setDownloadProgress(double progress, qint64 downloaded, qint64 total, int speed)
{
    isDownloading_ = true;
    downloadProgressWidget_->show();
    downloadButton_->hide();
    
    int percent = static_cast<int>(progress * 100);
    downloadProgressBar_->setValue(percent);
    
    downloadStatusLabel_->setText(QString("%1 / %2").arg(formatBytes(downloaded), formatBytes(total)));
    downloadSpeedLabel_->setText(formatSpeed(speed));
}

void TorrentDetailsPanel::setDownloadCompleted()
{
    isDownloading_ = false;
    downloadProgressWidget_->hide();
    downloadButton_->show();
    downloadButton_->setText(tr("✓ Completed"));
    downloadButton_->setEnabled(false);
    downloadButton_->setObjectName("completedButton");
    downloadButton_->style()->unpolish(downloadButton_);
    downloadButton_->style()->polish(downloadButton_);
}

void TorrentDetailsPanel::resetDownloadState()
{
    isDownloading_ = false;
    downloadProgressWidget_->hide();
    downloadButton_->show();
    downloadButton_->setText(tr("Download"));
    downloadButton_->setEnabled(true);
    downloadButton_->setObjectName("successButton");
    downloadButton_->style()->unpolish(downloadButton_);
    downloadButton_->style()->polish(downloadButton_);
}

void TorrentDetailsPanel::onCancelDownloadClicked()
{
    if (!currentHash_.isEmpty()) {
        emit downloadCancelRequested(currentHash_);
    }
    resetDownloadState();
}

QString TorrentDetailsPanel::formatSpeed(int bytesPerSec) const
{
    if (bytesPerSec < 1024) {
        return QString::number(bytesPerSec) + " B/s";
    } else if (bytesPerSec < 1024 * 1024) {
        return QString::number(bytesPerSec / 1024.0, 'f', 1) + " KB/s";
    } else {
        return QString::number(bytesPerSec / (1024.0 * 1024.0), 'f', 1) + " MB/s";
    }
}
