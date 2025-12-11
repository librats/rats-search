#include "torrentdetailspanel.h"
#include "torrentitemdelegate.h"
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
    titleLabel_ = new QLabel("Select a torrent");
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
    QLabel *statsTitle = new QLabel("Statistics");
    statsTitle->setObjectName("sectionTitle");
    mainLayout->addWidget(statsTitle);
    
    QHBoxLayout *statsLayout = new QHBoxLayout();
    statsLayout->setSpacing(16);
    
    // Seeders
    QVBoxLayout *seedersLayout = new QVBoxLayout();
    seedersLabel_ = new QLabel("0");
    seedersLabel_->setStyleSheet("font-size: 20px; font-weight: bold; color: #00C853;");
    seedersLabel_->setAlignment(Qt::AlignCenter);
    QLabel *seedersText = new QLabel("Seeders");
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
    QLabel *leechersText = new QLabel("Leechers");
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
    QLabel *completedText = new QLabel("Completed");
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
    
    // Info section
    QLabel *infoTitle = new QLabel("Information");
    infoTitle->setObjectName("sectionTitle");
    mainLayout->addWidget(infoTitle);
    
    // Size
    QHBoxLayout *sizeRow = new QHBoxLayout();
    QLabel *sizeTitle = new QLabel("Size:");
    sizeTitle->setObjectName("infoLabel");
    sizeTitle->setFixedWidth(80);
    sizeLabel_ = new QLabel("-");
    sizeLabel_->setObjectName("infoValue");
    sizeRow->addWidget(sizeTitle);
    sizeRow->addWidget(sizeLabel_, 1);
    mainLayout->addLayout(sizeRow);
    
    // Files
    QHBoxLayout *filesRow = new QHBoxLayout();
    QLabel *filesTitle = new QLabel("Files:");
    filesTitle->setObjectName("infoLabel");
    filesTitle->setFixedWidth(80);
    filesLabel_ = new QLabel("-");
    filesLabel_->setObjectName("infoValue");
    filesRow->addWidget(filesTitle);
    filesRow->addWidget(filesLabel_, 1);
    mainLayout->addLayout(filesRow);
    
    // Date
    QHBoxLayout *dateRow = new QHBoxLayout();
    QLabel *dateTitle = new QLabel("Added:");
    dateTitle->setObjectName("infoLabel");
    dateTitle->setFixedWidth(80);
    dateLabel_ = new QLabel("-");
    dateLabel_->setObjectName("infoValue");
    dateRow->addWidget(dateTitle);
    dateRow->addWidget(dateLabel_, 1);
    mainLayout->addLayout(dateRow);
    
    // Category
    QHBoxLayout *categoryRow = new QHBoxLayout();
    QLabel *categoryTitle = new QLabel("Category:");
    categoryTitle->setObjectName("infoLabel");
    categoryTitle->setFixedWidth(80);
    categoryLabel_ = new QLabel("-");
    categoryLabel_->setObjectName("infoValue");
    categoryRow->addWidget(categoryTitle);
    categoryRow->addWidget(categoryLabel_, 1);
    mainLayout->addLayout(categoryRow);
    
    // Hash section
    QLabel *hashTitle = new QLabel("Info Hash");
    hashTitle->setObjectName("sectionTitle");
    mainLayout->addWidget(hashTitle);
    
    hashLabel_ = new QLabel("-");
    hashLabel_->setStyleSheet("font-size: 9px; color: #666666; font-family: monospace;");
    hashLabel_->setWordWrap(true);
    hashLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(hashLabel_);
    
    // Spacer
    mainLayout->addStretch();
    
    // Action buttons
    QLabel *actionsTitle = new QLabel("Actions");
    actionsTitle->setObjectName("sectionTitle");
    mainLayout->addWidget(actionsTitle);
    
    // Magnet button
    magnetButton_ = new QPushButton("🧲  Open Magnet Link");
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
    downloadButton_ = new QPushButton("⬇  Download");
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
    copyHashButton_ = new QPushButton("📋  Copy Info Hash");
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
    filesLabel_->setText(QString::number(torrent.files) + " files");
    dateLabel_->setText(torrent.added.isValid() ? torrent.added.toString("MMMM d, yyyy") : "-");
    categoryLabel_->setText(torrent.contentCategory.isEmpty() ? "General" : torrent.contentCategory);
    
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
    
    titleLabel_->setText("Select a torrent");
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
        ratingLabel_->setText("No ratings");
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
    copyHashButton_->setText("✓  Copied!");
    QTimer::singleShot(2000, this, [this]() {
        copyHashButton_->setText("📋  Copy Info Hash");
    });
}

