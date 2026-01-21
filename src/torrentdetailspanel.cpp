#include "torrentdetailspanel.h"
#include "torrentitemdelegate.h"
#include "api/ratsapi.h"
#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QDateTime>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QScrollArea>
#include <QFrame>
#include <QTimer>
#include <QJsonArray>
#include <QHeaderView>
#include <QSet>
#include <algorithm>

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
    setObjectName("TorrentDetailsPanel");
    setStyleSheet(R"(
        #TorrentDetailsPanel {
            background-color: #252526;
            border-left: 1px solid #3c3f41;
        }
        QLabel {
            color: #ffffff;
        }
        QLabel#sectionTitle {
            color: #4a9eff;
            font-weight: bold;
            font-size: 11px;
            padding: 8px 0 4px 0;
            border-bottom: 1px solid #3c3f41;
        }
        QLabel#infoLabel {
            color: #888888;
            font-size: 10px;
        }
        QLabel#infoValue {
            color: #ffffff;
            font-size: 11px;
        }
        QPushButton#actionButton {
            border: none;
            border-radius: 6px;
            padding: 10px 20px;
            font-weight: bold;
            font-size: 12px;
            color: white;
        }
        QPushButton#actionButton:hover {
            opacity: 0.9;
        }
        QPushButton#magnetButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #e91e63, stop:1 #9c27b0);
        }
        QPushButton#magnetButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #f06292, stop:1 #ba68c8);
        }
        QPushButton#downloadButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #00c853, stop:1 #00e676);
        }
        QPushButton#downloadButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #00e676, stop:1 #69f0ae);
        }
        QPushButton#secondaryButton {
            background-color: #3c3f41;
            border: 1px solid #555555;
        }
        QPushButton#secondaryButton:hover {
            background-color: #4c4f51;
        }
        QPushButton#closeButton {
            background-color: transparent;
            border: none;
            color: #888888;
            font-size: 18px;
            padding: 4px 8px;
        }
        QPushButton#closeButton:hover {
            color: #ffffff;
            background-color: #3c3f41;
            border-radius: 4px;
        }
        QProgressBar {
            border: none;
            border-radius: 3px;
            background-color: #444444;
            height: 6px;
            text-align: center;
        }
        QProgressBar::chunk {
            border-radius: 3px;
        }
    )");
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 16);
    mainLayout->setSpacing(12);
    
    // Header with close button
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);
    
    // Content type icon
    contentTypeIcon_ = new QWidget();
    contentTypeIcon_->setFixedSize(32, 32);
    contentTypeIcon_->setStyleSheet("background-color: #888888; border-radius: 6px;");
    headerLayout->addWidget(contentTypeIcon_);
    
    // Title
    titleLabel_ = new QLabel(tr("Select a torrent"));
    titleLabel_->setWordWrap(true);
    titleLabel_->setStyleSheet("font-size: 14px; font-weight: bold; color: #ffffff;");
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
    contentTypeLabel_->setStyleSheet("font-size: 10px; color: #888888; padding-left: 40px;");
    mainLayout->addWidget(contentTypeLabel_);
    
    // Separator
    QFrame *sep1 = new QFrame();
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet("background-color: #3c3f41;");
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
    seedersLabel_->setStyleSheet("font-size: 20px; font-weight: bold; color: #00C853;");
    seedersLabel_->setAlignment(Qt::AlignCenter);
    QLabel *seedersText = new QLabel(tr("Seeders"));
    seedersText->setStyleSheet("font-size: 10px; color: #888888;");
    seedersText->setAlignment(Qt::AlignCenter);
    seedersLayout->addWidget(seedersLabel_);
    seedersLayout->addWidget(seedersText);
    statsLayout->addLayout(seedersLayout);
    
    // Leechers
    QVBoxLayout *leechersLayout = new QVBoxLayout();
    leechersLabel_ = new QLabel("0");
    leechersLabel_->setStyleSheet("font-size: 20px; font-weight: bold; color: #AA00FF;");
    leechersLabel_->setAlignment(Qt::AlignCenter);
    QLabel *leechersText = new QLabel(tr("Leechers"));
    leechersText->setStyleSheet("font-size: 10px; color: #888888;");
    leechersText->setAlignment(Qt::AlignCenter);
    leechersLayout->addWidget(leechersLabel_);
    leechersLayout->addWidget(leechersText);
    statsLayout->addLayout(leechersLayout);
    
    // Completed
    QVBoxLayout *completedLayout = new QVBoxLayout();
    completedLabel_ = new QLabel("0");
    completedLabel_->setStyleSheet("font-size: 20px; font-weight: bold; color: #FF6D00;");
    completedLabel_->setAlignment(Qt::AlignCenter);
    QLabel *completedText = new QLabel(tr("Completed"));
    completedText->setStyleSheet("font-size: 10px; color: #888888;");
    completedText->setAlignment(Qt::AlignCenter);
    completedLayout->addWidget(completedLabel_);
    completedLayout->addWidget(completedText);
    statsLayout->addLayout(completedLayout);
    
    mainLayout->addLayout(statsLayout);
    
    // Rating bar
    QHBoxLayout *ratingLayout = new QHBoxLayout();
    ratingBar_ = new QProgressBar();
    ratingBar_->setRange(0, 100);
    ratingBar_->setValue(0);
    ratingBar_->setTextVisible(false);
    ratingBar_->setFixedHeight(6);
    ratingLayout->addWidget(ratingBar_, 1);
    ratingLabel_ = new QLabel("N/A");
    ratingLabel_->setStyleSheet("font-size: 10px; color: #888888; margin-left: 8px;");
    ratingLayout->addWidget(ratingLabel_);
    mainLayout->addLayout(ratingLayout);
    
    // Voting buttons (migrated from legacy/app/torrent-page.js)
    QHBoxLayout *votingLayout = new QHBoxLayout();
    votingLayout->setSpacing(8);
    
    goodVoteButton_ = new QPushButton(tr("👍 Good"));
    goodVoteButton_->setStyleSheet(R"(
        QPushButton {
            background-color: #2e7d32;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
            font-size: 11px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #388e3c;
        }
        QPushButton:pressed {
            background-color: #1b5e20;
        }
        QPushButton:disabled {
            background-color: #3c3f41;
            color: #666666;
        }
    )");
    goodVoteButton_->setCursor(Qt::PointingHandCursor);
    connect(goodVoteButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::onGoodVoteClicked);
    votingLayout->addWidget(goodVoteButton_);
    
    badVoteButton_ = new QPushButton(tr("👎 Bad"));
    badVoteButton_->setStyleSheet(R"(
        QPushButton {
            background-color: #c62828;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
            font-size: 11px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #d32f2f;
        }
        QPushButton:pressed {
            background-color: #b71c1c;
        }
        QPushButton:disabled {
            background-color: #3c3f41;
            color: #666666;
        }
    )");
    badVoteButton_->setCursor(Qt::PointingHandCursor);
    connect(badVoteButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::onBadVoteClicked);
    votingLayout->addWidget(badVoteButton_);
    
    votingLayout->addStretch();
    
    votesLabel_ = new QLabel();
    votesLabel_->setStyleSheet("font-size: 10px; color: #888888;");
    votingLayout->addWidget(votesLabel_);
    
    mainLayout->addLayout(votingLayout);
    
    // Download progress section (hidden by default)
    downloadProgressWidget_ = new QWidget();
    downloadProgressWidget_->setStyleSheet("background-color: #2d2d2d; border-radius: 6px; padding: 8px;");
    QVBoxLayout *downloadLayout = new QVBoxLayout(downloadProgressWidget_);
    downloadLayout->setContentsMargins(12, 8, 12, 8);
    downloadLayout->setSpacing(6);
    
    QHBoxLayout *downloadHeaderLayout = new QHBoxLayout();
    QLabel *downloadTitle = new QLabel(tr("📥 Downloading..."));
    downloadTitle->setStyleSheet("font-size: 12px; font-weight: bold; color: #4a9eff;");
    downloadHeaderLayout->addWidget(downloadTitle);
    downloadHeaderLayout->addStretch();
    downloadSpeedLabel_ = new QLabel();
    downloadSpeedLabel_->setStyleSheet("font-size: 11px; color: #00c853; font-weight: bold;");
    downloadHeaderLayout->addWidget(downloadSpeedLabel_);
    downloadLayout->addLayout(downloadHeaderLayout);
    
    downloadProgressBar_ = new QProgressBar();
    downloadProgressBar_->setRange(0, 100);
    downloadProgressBar_->setValue(0);
    downloadProgressBar_->setTextVisible(true);
    downloadProgressBar_->setFixedHeight(20);
    downloadProgressBar_->setStyleSheet(R"(
        QProgressBar {
            border: none;
            border-radius: 4px;
            background-color: #1e1e1e;
            text-align: center;
            font-size: 10px;
            color: #ffffff;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #4a9eff, stop:1 #66b2ff);
            border-radius: 4px;
        }
    )");
    downloadLayout->addWidget(downloadProgressBar_);
    
    QHBoxLayout *downloadStatusLayout = new QHBoxLayout();
    downloadStatusLabel_ = new QLabel();
    downloadStatusLabel_->setStyleSheet("font-size: 10px; color: #888888;");
    downloadStatusLayout->addWidget(downloadStatusLabel_);
    downloadStatusLayout->addStretch();
    cancelDownloadButton_ = new QPushButton(tr("Cancel"));
    cancelDownloadButton_->setStyleSheet(R"(
        QPushButton {
            background-color: #8b3a3a;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            padding: 4px 12px;
            font-size: 10px;
        }
        QPushButton:hover {
            background-color: #a04040;
        }
    )");
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
    hashLabel_->setStyleSheet("font-size: 9px; color: #666666; font-family: monospace;");
    hashLabel_->setWordWrap(true);
    hashLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(hashLabel_);
    
    // Files tree section (migrated from legacy/app/torrent-page.js)
    filesTreeTitle_ = new QLabel(tr("Files"));
    filesTreeTitle_->setObjectName("sectionTitle");
    filesTreeTitle_->hide();  // Hidden until files are loaded
    mainLayout->addWidget(filesTreeTitle_);
    
    filesTree_ = new QTreeWidget();
    filesTree_->setHeaderLabels({tr("Name"), tr("Size"), tr("Download")});
    filesTree_->setColumnCount(3);
    filesTree_->header()->setStretchLastSection(false);
    filesTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    filesTree_->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    filesTree_->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    filesTree_->setColumnWidth(1, 80);
    filesTree_->setColumnWidth(2, 60);
    filesTree_->setMaximumHeight(200);
    filesTree_->setStyleSheet(R"(
        QTreeWidget {
            background-color: #1e1e1e;
            border: 1px solid #3c3f41;
            border-radius: 4px;
            font-size: 11px;
        }
        QTreeWidget::item {
            padding: 4px;
        }
        QTreeWidget::item:selected {
            background-color: #3d6a99;
        }
        QTreeWidget::item:hover {
            background-color: #2d3748;
        }
        QHeaderView::section {
            background-color: #2d2d2d;
            color: #888888;
            padding: 6px;
            border: none;
            border-bottom: 1px solid #3c3f41;
            font-size: 10px;
        }
    )");
    filesTree_->hide();  // Hidden until files are loaded
    connect(filesTree_, &QTreeWidget::itemChanged, this, &TorrentDetailsPanel::onFileItemChanged);
    mainLayout->addWidget(filesTree_);
    
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
    magnetButton_->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #e91e63, stop:1 #9c27b0);
            border: none;
            border-radius: 6px;
            padding: 12px 20px;
            font-weight: bold;
            font-size: 12px;
            color: white;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #f06292, stop:1 #ba68c8);
        }
    )");
    connect(magnetButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::onMagnetClicked);
    mainLayout->addWidget(magnetButton_);
    
    // Download button
    downloadButton_ = new QPushButton(tr("Download"));
    downloadButton_->setObjectName("downloadButton");
    downloadButton_->setCursor(Qt::PointingHandCursor);
    downloadButton_->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #00c853, stop:1 #00e676);
            border: none;
            border-radius: 6px;
            padding: 12px 20px;
            font-weight: bold;
            font-size: 12px;
            color: white;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #00e676, stop:1 #69f0ae);
        }
    )");
    connect(downloadButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::onDownloadClicked);
    mainLayout->addWidget(downloadButton_);
    
    // Copy hash button
    copyHashButton_ = new QPushButton(tr("Copy Info Hash"));
    copyHashButton_->setObjectName("secondaryButton");
    copyHashButton_->setCursor(Qt::PointingHandCursor);
    copyHashButton_->setStyleSheet(R"(
        QPushButton {
            background-color: #3c3f41;
            border: 1px solid #555555;
            border-radius: 6px;
            padding: 10px 20px;
            font-size: 11px;
            color: #cccccc;
        }
        QPushButton:hover {
            background-color: #4c4f51;
            color: #ffffff;
        }
    )");
    connect(copyHashButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::onCopyHashClicked);
    mainLayout->addWidget(copyHashButton_);
}

void TorrentDetailsPanel::setTorrent(const TorrentInfo &torrent)
{
    currentTorrent_ = torrent;
    currentHash_ = torrent.hash;
    
    // Update UI
    titleLabel_->setText(torrent.name);
    
    // Content type
    QString contentType = torrent.contentType.isEmpty() ? "unknown" : torrent.contentType;
    QColor typeColor = TorrentItemDelegate::getContentTypeColor(contentType);
    contentTypeIcon_->setStyleSheet(QString("background-color: %1; border-radius: 6px;").arg(typeColor.name()));
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
    
    setVisible(true);
}

void TorrentDetailsPanel::clear()
{
    currentHash_.clear();
    currentTorrent_ = TorrentInfo();
    
    titleLabel_->setText(tr("Select a torrent"));
    contentTypeIcon_->setStyleSheet("background-color: #888888; border-radius: 6px;");
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
}

void TorrentDetailsPanel::updateRatingDisplay()
{
    int good = currentTorrent_.good;
    int bad = currentTorrent_.bad;
    
    if (good == 0 && bad == 0) {
        ratingBar_->setValue(0);
        ratingBar_->setStyleSheet("QProgressBar::chunk { background-color: #444444; }");
        ratingLabel_->setText(tr("No ratings"));
        return;
    }
    
    int rating = static_cast<int>((static_cast<double>(good) / (good + bad)) * 100);
    ratingBar_->setValue(rating);
    
    QColor ratingColor = rating >= 50 ? QColor("#00E676") : QColor("#FF3D00");
    ratingBar_->setStyleSheet(QString("QProgressBar::chunk { background-color: %1; border-radius: 3px; }").arg(ratingColor.name()));
    ratingLabel_->setText(QString("%1%").arg(rating));
    ratingLabel_->setStyleSheet(QString("font-size: 10px; color: %1; margin-left: 8px; font-weight: bold;").arg(ratingColor.name()));
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

void TorrentDetailsPanel::setFiles(const QJsonArray& files)
{
    filesTree_->clear();
    
    if (files.isEmpty()) {
        filesTreeTitle_->hide();
        filesTree_->hide();
        return;
    }
    
    filesTreeTitle_->show();
    filesTree_->show();
    
    // Block signals while populating
    filesTree_->blockSignals(true);
    
    // Build hierarchical tree structure (like legacy buildFilesTree)
    FileTreeNode* root = buildFileTree(files);
    
    // Add nodes to widget
    for (auto it = root->children.begin(); it != root->children.end(); ++it) {
        addTreeNodeToWidget(it.value(), nullptr);
    }
    
    delete root;
    
    // Expand first level for better UX
    for (int i = 0; i < filesTree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = filesTree_->topLevelItem(i);
        item->setExpanded(true);
    }
    
    filesTree_->blockSignals(false);
}

TorrentDetailsPanel::FileTreeNode* TorrentDetailsPanel::buildFileTree(const QJsonArray& files)
{
    FileTreeNode* root = new FileTreeNode();
    root->name = "";
    root->size = 0;
    
    for (int i = 0; i < files.size(); ++i) {
        QJsonObject fileObj = files.at(i).toObject();
        QString path = fileObj["path"].toString();
        qint64 size = fileObj["size"].toVariant().toLongLong();
        
        // Split path into parts (e.g., "folder1/folder2/file.txt" -> ["folder1", "folder2", "file.txt"])
        QStringList pathParts = path.split('/', Qt::SkipEmptyParts);
        
        FileTreeNode* current = root;
        for (int j = 0; j < pathParts.size(); ++j) {
            const QString& part = pathParts[j];
            bool isLastPart = (j == pathParts.size() - 1);
            
            if (!current->children.contains(part)) {
                FileTreeNode* newNode = new FileTreeNode();
                newNode->name = part;
                newNode->size = 0;
                newNode->isFile = isLastPart;
                newNode->fileIndex = isLastPart ? i : -1;
                current->children[part] = newNode;
            }
            
            current = current->children[part];
            current->size += size;  // Accumulate size for folders
        }
        
        root->size += size;
    }
    
    return root;
}

void TorrentDetailsPanel::addTreeNodeToWidget(FileTreeNode* node, QTreeWidgetItem* parent)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();
    
    // Set name with icon
    QString icon = node->isFile ? getFileTypeIcon(node->name) : "📁";
    item->setText(0, icon + " " + node->name);
    
    // Set size
    item->setText(1, formatBytes(node->size));
    
    // Set checkbox for files
    if (node->isFile) {
        item->setCheckState(2, Qt::Checked);
        item->setData(0, Qt::UserRole, node->fileIndex);
    } else {
        // For folders, use partially checked state if needed
        item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
    }
    
    // Add to tree
    if (parent) {
        parent->addChild(item);
    } else {
        filesTree_->addTopLevelItem(item);
    }
    
    // Recursively add children (sorted alphabetically, folders first)
    QList<FileTreeNode*> folders;
    QList<FileTreeNode*> fileNodes;
    
    for (auto it = node->children.begin(); it != node->children.end(); ++it) {
        if (it.value()->isFile) {
            fileNodes.append(it.value());
        } else {
            folders.append(it.value());
        }
    }
    
    // Sort by name
    std::sort(folders.begin(), folders.end(), [](FileTreeNode* a, FileTreeNode* b) {
        return a->name.toLower() < b->name.toLower();
    });
    std::sort(fileNodes.begin(), fileNodes.end(), [](FileTreeNode* a, FileTreeNode* b) {
        return a->name.toLower() < b->name.toLower();
    });
    
    // Add folders first, then files
    for (FileTreeNode* folder : folders) {
        addTreeNodeToWidget(folder, item);
    }
    for (FileTreeNode* file : fileNodes) {
        addTreeNodeToWidget(file, item);
    }
}

QString TorrentDetailsPanel::getFileTypeIcon(const QString& filename) const
{
    QString ext = filename.section('.', -1).toLower();
    
    // Video extensions
    static const QSet<QString> videoExts = {
        "mkv", "mp4", "avi", "mov", "wmv", "flv", "webm", "m4v", "mpg", "mpeg",
        "3gp", "ts", "m2ts", "vob", "divx", "rmvb", "asf"
    };
    
    // Audio extensions
    static const QSet<QString> audioExts = {
        "mp3", "flac", "wav", "aac", "ogg", "wma", "m4a", "opus", "ape", "ac3"
    };
    
    // Image extensions
    static const QSet<QString> imageExts = {
        "jpg", "jpeg", "png", "gif", "bmp", "webp", "tiff", "svg", "ico", "psd"
    };
    
    // Book/document extensions
    static const QSet<QString> bookExts = {
        "pdf", "epub", "mobi", "djvu", "fb2", "doc", "docx", "txt", "rtf", "chm"
    };
    
    // Archive extensions
    static const QSet<QString> archiveExts = {
        "zip", "rar", "7z", "tar", "gz", "bz2", "xz", "iso"
    };
    
    // Application extensions
    static const QSet<QString> appExts = {
        "exe", "msi", "dmg", "apk", "deb", "rpm", "app"
    };
    
    if (videoExts.contains(ext)) return "🎬";
    if (audioExts.contains(ext)) return "🎵";
    if (imageExts.contains(ext)) return "🖼️";
    if (bookExts.contains(ext)) return "📚";
    if (archiveExts.contains(ext)) return "📦";
    if (appExts.contains(ext)) return "💿";
    
    return "📄";  // Default file icon
}

QList<int> TorrentDetailsPanel::getSelectedFileIndices() const
{
    QList<int> indices;
    
    // Recursively collect from hierarchical tree
    for (int i = 0; i < filesTree_->topLevelItemCount(); ++i) {
        collectSelectedFiles(filesTree_->topLevelItem(i), indices);
    }
    
    return indices;
}

void TorrentDetailsPanel::collectSelectedFiles(QTreeWidgetItem* item, QList<int>& indices) const
{
    if (!item) return;
    
    // Check if this is a file (has valid file index)
    int fileIndex = item->data(0, Qt::UserRole).toInt();
    if (fileIndex >= 0 && item->checkState(2) == Qt::Checked) {
        indices.append(fileIndex);
    }
    
    // Recursively check children
    for (int i = 0; i < item->childCount(); ++i) {
        collectSelectedFiles(item->child(i), indices);
    }
}

void TorrentDetailsPanel::onFileItemChanged(QTreeWidgetItem* item, int column)
{
    if (column != 2) return;  // Only care about checkbox column
    
    Q_UNUSED(item);
    
    if (!currentHash_.isEmpty()) {
        emit fileSelectionChanged(currentHash_, getSelectedFileIndices());
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
    downloadButton_->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #388e3c, stop:1 #4caf50);
            border: none;
            border-radius: 6px;
            padding: 12px 20px;
            font-weight: bold;
            font-size: 12px;
            color: white;
        }
    )");
}

void TorrentDetailsPanel::resetDownloadState()
{
    isDownloading_ = false;
    downloadProgressWidget_->hide();
    downloadButton_->show();
    downloadButton_->setText(tr("Download"));
    downloadButton_->setEnabled(true);
    downloadButton_->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #00c853, stop:1 #00e676);
            border: none;
            border-radius: 6px;
            padding: 12px 20px;
            font-weight: bold;
            font-size: 12px;
            color: white;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #00e676, stop:1 #69f0ae);
        }
    )");
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
