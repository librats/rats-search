#include "torrentdetailspanel.h"
#include "format.h"

#include "app/application.h"
#include "app/favorites_store.h"
#include "data/torrent_repository.h"
#include "domain/content.h"
#include "services/download_service.h"
#include "services/tracker_service.h"
#include "services/voting_service.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QFont>
#include <QFrame>
#include <QJsonArray>
#include <QNetworkReply>
#include <QPixmap>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QUrl>

using rats::domain::ContentCategory;
using rats::domain::ContentType;

TorrentDetailsPanel::TorrentDetailsPanel(QWidget* parent) : QWidget(parent)
{
    setupUi();
    clear();
}

TorrentDetailsPanel::~TorrentDetailsPanel() { }

void TorrentDetailsPanel::setupUi()
{
    setObjectName("detailsPanel");

    // Main layout for the panel (no margins - scroll area fills entire panel)
    QVBoxLayout* panelLayout = new QVBoxLayout(this);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    // Create scroll area for vertical scrolling
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setObjectName("detailsScrollArea");

    // Content widget inside scroll area
    QWidget* contentWidget = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setContentsMargins(16, 12, 16, 16);
    mainLayout->setSpacing(12);

    // Header with close button
    QHBoxLayout* headerLayout = new QHBoxLayout();
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
    QFrame* sep1 = new QFrame();
    sep1->setFrameShape(QFrame::HLine);
    sep1->setObjectName("detailsSeparator");
    sep1->setFixedHeight(1);
    mainLayout->addWidget(sep1);

    // Stats section (seeders, leechers, completed)
    QLabel* statsTitle = new QLabel(tr("Statistics"));
    statsTitle->setObjectName("sectionTitle");
    mainLayout->addWidget(statsTitle);

    QHBoxLayout* statsLayout = new QHBoxLayout();
    statsLayout->setSpacing(16);

    // Seeders
    QVBoxLayout* seedersLayout = new QVBoxLayout();
    seedersLabel_ = new QLabel("0");
    seedersLabel_->setObjectName("seedersLabel");
    seedersLabel_->setAlignment(Qt::AlignCenter);
    QLabel* seedersText = new QLabel(tr("Seeders"));
    seedersText->setObjectName("statsSubLabel");
    seedersText->setAlignment(Qt::AlignCenter);
    seedersLayout->addWidget(seedersLabel_);
    seedersLayout->addWidget(seedersText);
    statsLayout->addLayout(seedersLayout);

    // Leechers
    QVBoxLayout* leechersLayout = new QVBoxLayout();
    leechersLabel_ = new QLabel("0");
    leechersLabel_->setObjectName("leechersLabel");
    leechersLabel_->setAlignment(Qt::AlignCenter);
    QLabel* leechersText = new QLabel(tr("Leechers"));
    leechersText->setObjectName("statsSubLabel");
    leechersText->setAlignment(Qt::AlignCenter);
    leechersLayout->addWidget(leechersLabel_);
    leechersLayout->addWidget(leechersText);
    statsLayout->addLayout(leechersLayout);

    // Completed
    QVBoxLayout* completedLayout = new QVBoxLayout();
    completedLabel_ = new QLabel("0");
    completedLabel_->setObjectName("completedLabel");
    completedLabel_->setAlignment(Qt::AlignCenter);
    QLabel* completedText = new QLabel(tr("Completed"));
    completedText->setObjectName("statsSubLabel");
    completedText->setAlignment(Qt::AlignCenter);
    completedLayout->addWidget(completedLabel_);
    completedLayout->addWidget(completedText);
    statsLayout->addLayout(completedLayout);

    mainLayout->addLayout(statsLayout);

    // Rating bar
    QHBoxLayout* ratingLayout = new QHBoxLayout();
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
    QHBoxLayout* votingLayout = new QHBoxLayout();
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

    // Tracker info section (poster, description, links from tracker websites)
    trackerInfoWidget_ = new QWidget();
    trackerInfoWidget_->setObjectName("trackerInfoWidget");
    QVBoxLayout* trackerInfoLayout = new QVBoxLayout(trackerInfoWidget_);
    trackerInfoLayout->setContentsMargins(0, 0, 0, 0);
    trackerInfoLayout->setSpacing(8);

    QFrame* sepTracker = new QFrame();
    sepTracker->setFrameShape(QFrame::HLine);
    sepTracker->setObjectName("detailsSeparator");
    sepTracker->setFixedHeight(1);
    trackerInfoLayout->addWidget(sepTracker);

    QLabel* trackerInfoTitle = new QLabel(tr("Tracker Info"));
    trackerInfoTitle->setObjectName("sectionTitle");
    trackerInfoLayout->addWidget(trackerInfoTitle);

    // Loading indicator
    trackerInfoLoadingLabel_ = new QLabel(tr("🔍 Loading tracker info..."));
    trackerInfoLoadingLabel_->setObjectName("trackerLoadingLabel");
    trackerInfoLoadingLabel_->hide();
    trackerInfoLayout->addWidget(trackerInfoLoadingLabel_);

    // Poster image
    posterLabel_ = new QLabel();
    posterLabel_->setObjectName("posterLabel");
    posterLabel_->setAlignment(Qt::AlignCenter);
    posterLabel_->setMaximumHeight(300);
    posterLabel_->setScaledContents(false);
    posterLabel_->hide();
    trackerInfoLayout->addWidget(posterLabel_);

    // Description
    descriptionLabel_ = new QLabel();
    descriptionLabel_->setObjectName("descriptionLabel");
    descriptionLabel_->setWordWrap(true);
    descriptionLabel_->setTextFormat(Qt::PlainText);
    descriptionLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    descriptionLabel_->hide();
    trackerInfoLayout->addWidget(descriptionLabel_);

    // Show more/less toggle
    descriptionToggle_ = new QPushButton(tr("Show more ▼"));
    descriptionToggle_->setObjectName("descriptionToggle");
    descriptionToggle_->setCursor(Qt::PointingHandCursor);
    descriptionToggle_->setFlat(true);
    descriptionToggle_->hide();
    connect(descriptionToggle_, &QPushButton::clicked, this, [this]() {
        descriptionExpanded_ = !descriptionExpanded_;
        if (descriptionExpanded_) {
            descriptionLabel_->setText(fullDescription_);
            descriptionToggle_->setText(tr("Show less ▲"));
        } else {
            // Show first ~300 chars
            QString preview = fullDescription_.left(300);
            if (fullDescription_.length() > 300)
                preview += "...";
            descriptionLabel_->setText(preview);
            descriptionToggle_->setText(tr("Show more ▼"));
        }
    });
    trackerInfoLayout->addWidget(descriptionToggle_);

    // Tracker links
    trackerLinksWidget_ = new QWidget();
    trackerLinksLayout_ = new QHBoxLayout(trackerLinksWidget_);
    trackerLinksLayout_->setContentsMargins(0, 4, 0, 0);
    trackerLinksLayout_->setSpacing(8);
    trackerLinksLayout_->addStretch();
    trackerLinksWidget_->hide();
    trackerInfoLayout->addWidget(trackerLinksWidget_);

    trackerInfoWidget_->hide();
    mainLayout->addWidget(trackerInfoWidget_);

    posterNetworkManager_ = new QNetworkAccessManager(this);

    // Download progress section (hidden by default)
    downloadProgressWidget_ = new QWidget();
    downloadProgressWidget_->setObjectName("downloadProgressWidget");
    QVBoxLayout* downloadLayout = new QVBoxLayout(downloadProgressWidget_);
    downloadLayout->setContentsMargins(12, 8, 12, 8);
    downloadLayout->setSpacing(6);

    QHBoxLayout* downloadHeaderLayout = new QHBoxLayout();
    QLabel* downloadTitle = new QLabel(tr("📥 Downloading..."));
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

    QHBoxLayout* downloadStatusLayout = new QHBoxLayout();
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

    // Go to Downloads button
    goToDownloadsButton_ = new QPushButton(tr("📥 Go to Downloads"));
    goToDownloadsButton_->setObjectName("goToDownloadsButton");
    goToDownloadsButton_->setCursor(Qt::PointingHandCursor);
    connect(goToDownloadsButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::goToDownloadsRequested);
    downloadLayout->addWidget(goToDownloadsButton_);

    downloadProgressWidget_->hide();
    mainLayout->addWidget(downloadProgressWidget_);

    // Info section
    QLabel* infoTitle = new QLabel(tr("Information"));
    infoTitle->setObjectName("sectionTitle");
    mainLayout->addWidget(infoTitle);

    // Size
    QHBoxLayout* sizeRow = new QHBoxLayout();
    QLabel* sizeTitle = new QLabel(tr("Size:"));
    sizeTitle->setObjectName("infoLabel");
    sizeTitle->setFixedWidth(80);
    sizeLabel_ = new QLabel("-");
    sizeLabel_->setObjectName("infoValue");
    sizeRow->addWidget(sizeTitle);
    sizeRow->addWidget(sizeLabel_, 1);
    mainLayout->addLayout(sizeRow);

    // Files
    QHBoxLayout* filesRow = new QHBoxLayout();
    QLabel* filesTitle = new QLabel(tr("Files:"));
    filesTitle->setObjectName("infoLabel");
    filesTitle->setFixedWidth(80);
    filesLabel_ = new QLabel("-");
    filesLabel_->setObjectName("infoValue");
    filesRow->addWidget(filesTitle);
    filesRow->addWidget(filesLabel_, 1);
    mainLayout->addLayout(filesRow);

    // Date
    QHBoxLayout* dateRow = new QHBoxLayout();
    QLabel* dateTitle = new QLabel(tr("Added:"));
    dateTitle->setObjectName("infoLabel");
    dateTitle->setFixedWidth(80);
    dateLabel_ = new QLabel("-");
    dateLabel_->setObjectName("infoValue");
    dateRow->addWidget(dateTitle);
    dateRow->addWidget(dateLabel_, 1);
    mainLayout->addLayout(dateRow);

    // Category
    QHBoxLayout* categoryRow = new QHBoxLayout();
    QLabel* categoryTitle = new QLabel(tr("Category:"));
    categoryTitle->setObjectName("infoLabel");
    categoryTitle->setFixedWidth(80);
    categoryLabel_ = new QLabel("-");
    categoryLabel_->setObjectName("infoValue");
    categoryRow->addWidget(categoryTitle);
    categoryRow->addWidget(categoryLabel_, 1);
    mainLayout->addLayout(categoryRow);

    // Hash section
    QLabel* hashTitle = new QLabel(tr("Info Hash"));
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
    QLabel* actionsTitle = new QLabel(tr("Actions"));
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

    // Favorite button
    favoriteButton_ = new QPushButton(tr("⭐ Add to Favorites"));
    favoriteButton_->setObjectName("secondaryButton");
    favoriteButton_->setCursor(Qt::PointingHandCursor);
    connect(favoriteButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::onFavoriteClicked);
    mainLayout->addWidget(favoriteButton_);

    // Copy hash button
    copyHashButton_ = new QPushButton(tr("Copy Info Hash"));
    copyHashButton_->setObjectName("secondaryButton");
    copyHashButton_->setCursor(Qt::PointingHandCursor);
    connect(copyHashButton_, &QPushButton::clicked, this, &TorrentDetailsPanel::onCopyHashClicked);
    mainLayout->addWidget(copyHashButton_);

    // Set up scroll area with content
    scrollArea->setWidget(contentWidget);
    panelLayout->addWidget(scrollArea);
}

void TorrentDetailsPanel::setApplication(rats::app::Application* app)
{
    app_ = app;
    if (!app_)
        return;

    if (auto* voting = app_->voting()) {
        connect(voting, &rats::service::VotingService::votesUpdated, this, &TorrentDetailsPanel::onVotesUpdated);
    }
    if (auto* repo = app_->torrents()) {
        connect(repo, &rats::data::TorrentRepository::torrentUpdated, this, &TorrentDetailsPanel::onTorrentUpdated);
    }
    if (auto* fav = app_->favorites()) {
        connect(fav, &rats::app::FavoritesStore::favoritesChanged, this, &TorrentDetailsPanel::updateFavoriteButton);
    }
}

void TorrentDetailsPanel::setTorrent(const rats::domain::Torrent& torrent)
{
    currentTorrent_ = torrent;
    currentHash_ = torrent.hash;

    // Check voted status from the voting service (distributed store).
    hasVoted_ = (app_ && app_->voting()) ? app_->voting()->hasVoted(torrent.hash) : false;

    // Update UI - use makeBreakable to allow wrapping of long names without
    // spaces
    titleLabel_->setText(makeBreakable(torrent.name));

    // Content type — emoji glyph + human name from the domain type.
    const QString typeName = (torrent.contentType == ContentType::Unknown)
        ? tr("Unknown")
        : rats::ui::capitalizeFirst(rats::domain::toString(torrent.contentType));
    contentTypeLabel_->setText(rats::ui::contentTypeIcon(torrent.contentType) + " " + typeName);

    // Stats
    seedersLabel_->setText(QString::number(torrent.seeders));
    leechersLabel_->setText(QString::number(torrent.leechers));
    completedLabel_->setText(QString::number(torrent.completed));

    // Info
    sizeLabel_->setText(rats::ui::formatSize(torrent.size));
    filesLabel_->setText(tr("%1 files").arg(torrent.files));
    dateLabel_->setText(torrent.added.isValid() ? torrent.added.toString("MMMM d, yyyy") : "-");

    // Category — display the human content type + optional finer category.
    const bool typeUnknown = (torrent.contentType == ContentType::Unknown);
    const bool catUnknown = (torrent.contentCategory == ContentCategory::Unknown);
    categoryLabel_->setText(typeUnknown ? tr("Unknown")
                                        : rats::ui::capitalizeFirst(rats::domain::toString(torrent.contentType))
                + (catUnknown
                        ? QString()
                        : " (" + rats::ui::capitalizeFirst(rats::domain::toString(torrent.contentCategory)) + ")"));

    // Hash - use makeBreakable for long hash strings
    hashLabel_->setText(makeBreakable(torrent.hash));

    // Rating
    updateRatingDisplay();
    updateVotingButtons();
    updateFavoriteButton();

    // Check if this torrent is currently downloading
    if (app_ && app_->downloads() && app_->downloads()->isDownloading(torrent.hash)) {
        rats::service::Download d = app_->downloads()->getDownload(torrent.hash);
        if (d.completed) {
            setDownloadCompleted();
        } else {
            setDownloadProgress(d.progress, d.downloadedBytes, d.totalSize, static_cast<int>(d.downloadSpeed));
        }
    } else {
        // Reset to normal download button state
        resetDownloadState();
    }

    // Show existing tracker info from database if available
    if (!torrent.info.isEmpty() && torrent.info.contains("trackers")) {
        updateTrackerInfoDisplay(torrent.info);
    } else {
        // Reset tracker info UI
        trackerInfoWidget_->hide();
        posterLabel_->hide();
        descriptionLabel_->hide();
        descriptionToggle_->hide();
        trackerLinksWidget_->hide();
        trackerInfoLoadingLabel_->hide();
        fullDescription_.clear();
        descriptionExpanded_ = false;
    }

    setVisible(true);

    // Kick off background tracker (counts + website info) refresh via the
    // service.
    requestTrackerRefresh();
}

void TorrentDetailsPanel::clear()
{
    currentHash_.clear();
    currentTorrent_ = rats::domain::Torrent();
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

    // Clear tracker info
    trackerInfoWidget_->hide();
    posterLabel_->hide();
    posterLabel_->clear();
    descriptionLabel_->hide();
    descriptionLabel_->clear();
    descriptionToggle_->hide();
    trackerLinksWidget_->hide();
    trackerInfoLoadingLabel_->hide();
    fullDescription_.clear();
    descriptionExpanded_ = false;

    // Remove old tracker link buttons
    while (trackerLinksLayout_->count() > 1) { // keep the stretch
        QLayoutItem* item = trackerLinksLayout_->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
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

void TorrentDetailsPanel::onMagnetClicked()
{
    if (currentHash_.isEmpty())
        return;

    QString magnetLink = QString("magnet:?xt=urn:btih:%1&dn=%2")
                             .arg(currentHash_)
                             .arg(QString::fromUtf8(QUrl::toPercentEncoding(currentTorrent_.name)));

    QDesktopServices::openUrl(QUrl(magnetLink));
}

void TorrentDetailsPanel::onDownloadClicked()
{
    if (currentHash_.isEmpty())
        return;
    emit downloadRequested(currentHash_);
}

void TorrentDetailsPanel::onCopyHashClicked()
{
    if (currentHash_.isEmpty())
        return;

    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(currentHash_);

    // Visual feedback
    copyHashButton_->setText(tr("Copied!"));
    QTimer::singleShot(2000, this, [this]() { copyHashButton_->setText(tr("Copy Info Hash")); });
}

void TorrentDetailsPanel::onFavoriteClicked()
{
    if (currentHash_.isEmpty() || !app_ || !app_->favorites())
        return;

    auto* fav = app_->favorites();
    if (fav->isFavorite(currentHash_)) {
        fav->remove(currentHash_);
    } else {
        fav->add(currentTorrent_);
    }

    updateFavoriteButton();
}

void TorrentDetailsPanel::updateFavoriteButton()
{
    if (!app_ || !app_->favorites() || currentHash_.isEmpty()) {
        favoriteButton_->setText(tr("⭐ Add to Favorites"));
        return;
    }

    if (app_->favorites()->isFavorite(currentHash_)) {
        favoriteButton_->setText(tr("★ In Favorites (Remove)"));
        favoriteButton_->setObjectName("warningButton");
    } else {
        favoriteButton_->setText(tr("⭐ Add to Favorites"));
        favoriteButton_->setObjectName("secondaryButton");
    }
    favoriteButton_->style()->unpolish(favoriteButton_);
    favoriteButton_->style()->polish(favoriteButton_);
}

void TorrentDetailsPanel::onGoodVoteClicked()
{
    if (currentHash_.isEmpty())
        return;

    hasVoted_ = true;
    updateVotingButtons();

    if (app_ && app_->voting()) {
        app_->voting()->vote(currentHash_, true, {});
    }
}

void TorrentDetailsPanel::onBadVoteClicked()
{
    if (currentHash_.isEmpty())
        return;

    hasVoted_ = true;
    updateVotingButtons();

    if (app_ && app_->voting()) {
        app_->voting()->vote(currentHash_, false, {});
    }
}

void TorrentDetailsPanel::onVotesUpdated(const QString& hash, int good, int bad)
{
    if (hash != currentHash_)
        return;

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

    downloadStatusLabel_->setText(
        QString("%1 / %2").arg(rats::ui::formatSize(downloaded), rats::ui::formatSize(total)));
    downloadSpeedLabel_->setText(rats::ui::formatSpeed(speed));
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

QString TorrentDetailsPanel::makeBreakable(const QString& text) const
{
    // Insert zero-width space after common separators to allow line breaking
    // This helps with long filenames without spaces
    const QChar zwsp(0x200B); // Zero-width space
    QString result;
    result.reserve(text.size() * 2);

    int consecutiveChars = 0;
    const int maxConsecutive = 20; // Force break after this many chars without a break opportunity

    for (int i = 0; i < text.size(); ++i) {
        QChar c = text[i];
        result += c;

        if (c.isSpace()) {
            consecutiveChars = 0;
        } else {
            consecutiveChars++;

            // Insert break opportunity after common separators
            if (c == '.' || c == '_' || c == '-' || c == '~' || c == '+' || c == '[' || c == ']' || c == '('
                || c == ')') {
                result += zwsp;
                consecutiveChars = 0;
            }
            // Force break after many consecutive non-space characters
            else if (consecutiveChars >= maxConsecutive) {
                result += zwsp;
                consecutiveChars = 0;
            }
        }
    }

    return result;
}

void TorrentDetailsPanel::updateTrackerStats(int seeders, int leechers, int completed)
{
    currentTorrent_.seeders = seeders;
    currentTorrent_.leechers = leechers;
    currentTorrent_.completed = completed;

    seedersLabel_->setText(QString::number(seeders));
    leechersLabel_->setText(QString::number(leechers));
    completedLabel_->setText(QString::number(completed));
}

// ============================================================================
// Tracker refresh — delegated to TrackerService. Results are persisted to the
// repository, which emits torrentUpdated(hash); onTorrentUpdated() then reloads
// the row and refreshes stats + scraped info. Rate limiting lives in the
// service.
// ============================================================================

void TorrentDetailsPanel::requestTrackerRefresh()
{
    if (!app_ || currentHash_.isEmpty()) {
        return;
    }

    auto* trackers = app_->trackers();
    if (!trackers) {
        return;
    }

    // Refresh seeder/leecher counts.
    trackers->checkCounts(currentHash_);

    // Scrape tracker websites for descriptions/posters when we don't already
    // have them. Show the loading indicator while the scrape runs.
    bool haveInfo = false;
    if (!currentTorrent_.info.isEmpty() && currentTorrent_.info.contains("trackers")) {
        haveInfo = !currentTorrent_.info["trackers"].toArray().isEmpty();
    }
    if (!haveInfo) {
        trackerInfoWidget_->show();
        trackerInfoLoadingLabel_->show();
        trackers->checkInfo(currentHash_, currentTorrent_.name);
    }
}

void TorrentDetailsPanel::onTorrentUpdated(const QString& hash)
{
    if (hash != currentHash_ || !app_ || !app_->torrents()) {
        return;
    }

    auto updated = app_->torrents()->get(hash, false);
    if (!updated) {
        return;
    }

    // Refresh swarm counts.
    updateTrackerStats(updated->seeders, updated->leechers, updated->completed);

    // Refresh scraped tracker info (poster/description/links).
    currentTorrent_.info = updated->info;
    trackerInfoLoadingLabel_->hide();
    if (!updated->info.isEmpty() && updated->info.contains("trackers")) {
        updateTrackerInfoDisplay(updated->info);
    }
}

void TorrentDetailsPanel::updateTrackerInfoDisplay(const QJsonObject& info)
{
    trackerInfoWidget_->show();
    trackerInfoLoadingLabel_->hide();

    // Poster image
    QString posterUrl = info["poster"].toString();
    if (!posterUrl.isEmpty()) {
        loadPosterImage(posterUrl);
    } else {
        posterLabel_->hide();
    }

    // Description
    QString description = info["description"].toString();
    if (!description.isEmpty()) {
        fullDescription_ = description;
        descriptionExpanded_ = false;

        if (description.length() > 300) {
            descriptionLabel_->setText(description.left(300) + "...");
            descriptionToggle_->show();
            descriptionToggle_->setText(tr("Show more ▼"));
        } else {
            descriptionLabel_->setText(description);
            descriptionToggle_->hide();
        }
        descriptionLabel_->show();
    } else {
        descriptionLabel_->hide();
        descriptionToggle_->hide();
    }

    // Tracker links - remove old buttons first (keep the stretch at end)
    while (trackerLinksLayout_->count() > 1) {
        QLayoutItem* item = trackerLinksLayout_->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    bool hasLinks = false;

    // RuTracker link
    int rutrackerThreadId = info["rutrackerThreadId"].toInt();
    if (rutrackerThreadId > 0) {
        QPushButton* rtBtn = new QPushButton(tr("🔗 RuTracker"));
        rtBtn->setObjectName("trackerLinkButton");
        rtBtn->setCursor(Qt::PointingHandCursor);
        rtBtn->setToolTip(QString("https://rutracker.org/forum/viewtopic.php?t=%1").arg(rutrackerThreadId));
        connect(rtBtn, &QPushButton::clicked, this, [rutrackerThreadId]() {
            QDesktopServices::openUrl(
                QUrl(QString("https://rutracker.org/forum/viewtopic.php?t=%1").arg(rutrackerThreadId)));
        });
        trackerLinksLayout_->insertWidget(trackerLinksLayout_->count() - 1, rtBtn);
        hasLinks = true;
    }

    // Nyaa link
    int nyaaThreadId = info["nyaaThreadId"].toInt();
    if (nyaaThreadId > 0) {
        QPushButton* nyaaBtn = new QPushButton(tr("🔗 Nyaa"));
        nyaaBtn->setObjectName("trackerLinkButton");
        nyaaBtn->setCursor(Qt::PointingHandCursor);
        nyaaBtn->setToolTip(QString("https://nyaa.si/view/%1").arg(nyaaThreadId));
        connect(nyaaBtn, &QPushButton::clicked, this, [nyaaThreadId]() {
            QDesktopServices::openUrl(QUrl(QString("https://nyaa.si/view/%1").arg(nyaaThreadId)));
        });
        trackerLinksLayout_->insertWidget(trackerLinksLayout_->count() - 1, nyaaBtn);
        hasLinks = true;
    }

    // Content category from tracker
    QString trackerCategory = info["contentCategory"].toString();
    if (!trackerCategory.isEmpty()) {
        categoryLabel_->setText(trackerCategory);
    }

    trackerLinksWidget_->setVisible(hasLinks);
}

void TorrentDetailsPanel::loadPosterImage(const QString& url)
{
    if (url.isEmpty()) {
        posterLabel_->hide();
        return;
    }

    posterLabel_->setText(tr("Loading image..."));
    posterLabel_->show();

    QUrl imageUrl(url);
    QNetworkRequest request { imageUrl };
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"));
    request.setTransferTimeout(15000);

    QNetworkReply* reply = posterNetworkManager_->get(request);
    QString hash = currentHash_;

    connect(reply, &QNetworkReply::finished, this, [this, reply, hash]() {
        reply->deleteLater();

        // Only update if still showing this torrent
        if (currentHash_ != hash)
            return;

        if (reply->error() != QNetworkReply::NoError) {
            posterLabel_->hide();
            return;
        }

        QByteArray imageData = reply->readAll();
        QPixmap pixmap;
        if (pixmap.loadFromData(imageData)) {
            // Scale to fit panel width, max 300px height
            int maxWidth = this->width() - 40;
            if (maxWidth < 100)
                maxWidth = 250;

            if (pixmap.width() > maxWidth || pixmap.height() > 300) {
                pixmap = pixmap.scaled(maxWidth, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            posterLabel_->setPixmap(pixmap);
            posterLabel_->show();
        } else {
            posterLabel_->hide();
        }
    });
}
