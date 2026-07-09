#include "mainwindow.h"
#include "searchresultmodel.h"
#include "torrentdetailspanel.h"
#include "torrentitemdelegate.h"
#include "version.h"

// Tab widgets
#include "activitywidget.h"
#include "downloadswidget.h"
#include "favoriteswidget.h"
#include "feedwidget.h"
#include "toptorrentswidget.h"
#include "torrentfileswidget.h"

// Settings dialog
#include "settingsdialog.h"

// Layered application + services
#include "app/application.h"
#include "app/config_store.h"
#include "app/favorites_store.h"
#include "app/translation_manager.h"
#include "common/result.h"
#include "data/torrent_repository.h"
#include "domain/content.h"
#include "domain/torrent_codec.h"
#include "format.h"
#include "net/crawler.h"
#include "net/p2p_transport.h"
#include "peer/peer_api.h"
#include "rest/api_router.h"
#include "services/download_service.h"
#include "services/indexing_service.h"
#include "services/migration_service.h"
#include "services/peer_registry.h"
#include "services/search_service.h"
#include "services/torrent_creator.h"
#include "services/torrent_exporter.h"
#include "services/update_service.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QSysInfo>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTableView>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

using rats::Result;
using rats::domain::SearchHit;
using rats::domain::Torrent;
using rats::service::SearchService;
using rats::service::UpdateService;
namespace codec = rats::domain::codec;

MainWindow::MainWindow(rats::app::Application* app, QWidget* parent)
    : QMainWindow(parent), app_(app), trayIcon(nullptr), trayMenu(nullptr)
{
    // The Application is already started by main(); MainWindow only wires UI.
    rats::app::ConfigStore* config = app_->config();

    // Check if user has accepted the agreement before showing the main UI.
    if (config && !config->agreementAccepted()) {
        if (!showAgreementDialog()) {
            // User declined - schedule application exit (main() owns app_->stop()).
            QTimer::singleShot(0, qApp, &QApplication::quit);
            return;
        }
    }

    loadSettings();

    setWindowTitle(tr("Rats Search %1 - BitTorrent P2P Search Engine").arg(RATSSEARCH_VERSION_STRING));
    resize(1400, 900);
    setWindowIcon(QIcon(":/images/icon.png"));
    setAcceptDrops(true); // drag & drop .torrent files

    applyTheme(config ? config->darkMode() : false);
    setupUi();
    setupMenuBar();
    setupStatusBar();
    setupSystemTray();

    // Hand the running application to every tab/panel, then wire the signals.
    wireWidgets();
    connectSignals();

    // Seed the status bar from live state.
    if (app_->torrents()) {
        cachedTorrentCount_ = app_->torrents()->statistics().torrents;
    }
    if (app_->peers()) {
        cachedRemoteTorrentCount_ = app_->peers()->remoteTorrentsCount();
    }
    updateStatusBar();
    updateP2PState();
    updateNetworkStatus();

    // Kick off a startup update check if enabled.
    if (config && config->checkUpdatesOnStartup() && app_->updates()) {
        QTimer::singleShot(5000, this, [this]() { app_->updates()->checkForUpdates(); });
    }
}

MainWindow::~MainWindow()
{
    saveSettings();
    // Service lifecycle is owned by main()/Application — nothing to stop here.
}

void MainWindow::applyTheme(bool darkMode)
{
    QString stylePath = darkMode ? ":/styles/styles/dark.qss" : ":/styles/styles/light.qss";
    QFile styleFile(stylePath);
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString styleSheet = QString::fromUtf8(styleFile.readAll());
        styleFile.close();
        setStyleSheet(styleSheet);
        qInfo() << (darkMode ? "Dark" : "Light") << "theme loaded from resources";
    } else {
        qWarning() << "Failed to load theme from resources:" << styleFile.errorString();
    }
}

void MainWindow::setupUi()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Search bar section
    QWidget* searchSection = new QWidget();
    searchSection->setObjectName("searchSection");
    QHBoxLayout* searchLayout = new QHBoxLayout(searchSection);
    searchLayout->setContentsMargins(12, 8, 12, 8);
    searchLayout->setSpacing(12);

    QLabel* logoLabel = new QLabel("🐀");
    logoLabel->setObjectName("logoLabel");
    searchLayout->addWidget(logoLabel);

    QLabel* titleLabel = new QLabel("Rats Search");
    titleLabel->setObjectName("titleLabel");
    searchLayout->addWidget(titleLabel);

    searchLayout->addSpacing(20);

    searchLineEdit = new QLineEdit(this);
    searchLineEdit->setPlaceholderText(tr("Search for torrents..."));
    searchLineEdit->setMinimumHeight(44);
    searchLineEdit->setMinimumWidth(400);
    QFont searchFont = searchLineEdit->font();
    searchFont.setPointSize(12);
    searchLineEdit->setFont(searchFont);

    searchButton = new QPushButton(tr("Search"), this);
    searchButton->setMinimumSize(120, 44);
    searchButton->setDefault(true);
    searchButton->setCursor(Qt::PointingHandCursor);

    sortComboBox = new QComboBox(this);
    sortComboBox->addItem(tr("Sort: Seeders ↓"), "seeders_desc");
    sortComboBox->addItem(tr("Sort: Seeders ↑"), "seeders_asc");
    sortComboBox->addItem(tr("Sort: Size ↓"), "size_desc");
    sortComboBox->addItem(tr("Sort: Size ↑"), "size_asc");
    sortComboBox->addItem(tr("Sort: Date ↓"), "added_desc");
    sortComboBox->addItem(tr("Sort: Date ↑"), "added_asc");
    sortComboBox->addItem(tr("Sort: Name A-Z"), "name_asc");
    sortComboBox->addItem(tr("Sort: Name Z-A"), "name_desc");
    sortComboBox->setMinimumHeight(44);

    searchLayout->addWidget(searchLineEdit, 1);
    searchLayout->addWidget(sortComboBox);
    searchLayout->addWidget(searchButton);

    mainLayout->addWidget(searchSection);

    // Vertical splitter: content area on top, files panel at bottom
    verticalSplitter = new QSplitter(Qt::Vertical, this);
    verticalSplitter->setHandleWidth(3);

    // Horizontal splitter for tabs + details panel
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->setHandleWidth(2);

    tabWidget = new QTabWidget(this);
    tabWidget->setDocumentMode(true);

    // Search results tab
    QWidget* searchTab = new QWidget();
    QVBoxLayout* searchTabLayout = new QVBoxLayout(searchTab);
    searchTabLayout->setContentsMargins(0, 8, 0, 0);

    resultsTableView = new QTableView(this);
    searchResultModel = new SearchResultModel(this);
    torrentDelegate = new TorrentItemDelegate(this);

    resultsTableView->setModel(searchResultModel);
    resultsTableView->setItemDelegate(torrentDelegate);
    resultsTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultsTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    resultsTableView->setAlternatingRowColors(true);
    resultsTableView->setSortingEnabled(true);
    resultsTableView->horizontalHeader()->setStretchLastSection(true);
    resultsTableView->verticalHeader()->setVisible(false);
    resultsTableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    resultsTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTableView->setShowGrid(false);
    resultsTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    resultsTableView->setMouseTracking(true);

    resultsTableView->setColumnWidth(0, 550); // Name
    resultsTableView->setColumnWidth(1, 100); // Size
    resultsTableView->setColumnWidth(2, 80); // Seeders
    resultsTableView->setColumnWidth(3, 80); // Leechers
    resultsTableView->setColumnWidth(4, 120); // Date

    searchTabLayout->addWidget(resultsTableView);
    tabWidget->addTab(searchTab, tr("Search Results"));

    topTorrentsWidget = new TopTorrentsWidget(this);
    tabWidget->addTab(topTorrentsWidget, tr("🔥 Top"));

    feedWidget = new FeedWidget(this);
    tabWidget->addTab(feedWidget, tr("📰 Feed"));

    downloadsWidget = new DownloadsWidget(this);
    tabWidget->addTab(downloadsWidget, tr("📥 Downloads"));

    activityWidget = new ActivityWidget(this);
    tabWidget->addTab(activityWidget, tr("⚡ Activity"));

    favoritesWidget = new FavoritesWidget(this);
    tabWidget->addTab(favoritesWidget, tr("⭐ Favorites"));

    mainSplitter->addWidget(tabWidget);

    // Right side - Details panel
    detailsPanel = new TorrentDetailsPanel(this);
    detailsPanel->setMinimumWidth(280);
    detailsPanel->setMinimumHeight(150);
    detailsPanel->hide();

    mainSplitter->addWidget(detailsPanel);
    mainSplitter->setSizes({ 900, 350 });

    verticalSplitter->addWidget(mainSplitter);

    // Bottom panel - Files widget
    filesWidget = new TorrentFilesWidget(this);
    filesWidget->setMinimumHeight(120);
    filesWidget->hide();
    verticalSplitter->addWidget(filesWidget);

    mainLayout->addWidget(verticalSplitter, 1);
}

void MainWindow::setupMenuBar()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    QAction* addTorrentAction = fileMenu->addAction(tr("📥 &Add Torrent..."));
    addTorrentAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    addTorrentAction->setToolTip(tr("Add a .torrent file to the search index"));
    connect(addTorrentAction, &QAction::triggered, this, &MainWindow::addTorrentFile);

    QAction* createTorrentAction = fileMenu->addAction(tr("🔨 &Create Torrent..."));
    createTorrentAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    createTorrentAction->setToolTip(tr("Create a torrent from a file or folder and start seeding"));
    connect(createTorrentAction, &QAction::triggered, this, &MainWindow::createTorrent);

    fileMenu->addSeparator();

    QAction* settingsAction = fileMenu->addAction(tr("&Settings"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);

    fileMenu->addSeparator();

    QAction* quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QMainWindow::close);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));

    QAction* checkUpdateAction = helpMenu->addAction(tr("Check for &Updates..."));
    connect(checkUpdateAction, &QAction::triggered, this, &MainWindow::checkForUpdates);

    QAction* changelogAction = helpMenu->addAction(tr("📋 &Changelog"));
    connect(changelogAction, &QAction::triggered, this, &MainWindow::showChangelog);

    helpMenu->addSeparator();

    QAction* reportBugAction = helpMenu->addAction(tr("🐛 &Report a Bug..."));
    reportBugAction->setToolTip(tr("Open a bug report on GitHub"));
    connect(reportBugAction, &QAction::triggered, this, &MainWindow::reportBug);

    QAction* featureAction = helpMenu->addAction(tr("💡 Request a &Feature..."));
    featureAction->setToolTip(tr("Suggest a new feature on GitHub"));
    connect(featureAction, &QAction::triggered, this, &MainWindow::requestFeature);

    helpMenu->addSeparator();

    QAction* aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::setupStatusBar()
{
    p2pStatusLabel = new QLabel();
    p2pStatusLabel->setTextFormat(Qt::RichText);
    p2pState_ = P2PState::NotStarted;
    updateP2PIndicator();

    peerCountLabel = new QLabel(tr("👥 Peers: %1").arg(0));
    dhtNodeCountLabel = new QLabel(tr("🌐 DHT: %1").arg(0));
    torrentCountLabel = new QLabel(tr("📦 Torrents: %1").arg(0));
    spiderStatusLabel = new QLabel(tr("🕷️ Spider: Idle"));

    statusBar()->addWidget(p2pStatusLabel);
    statusBar()->addWidget(peerCountLabel);
    statusBar()->addWidget(dhtNodeCountLabel);
    statusBar()->addWidget(torrentCountLabel);
    statusBar()->addWidget(spiderStatusLabel);
    statusBar()->addPermanentWidget(new QLabel(tr("Ready")));

    // Periodic network status refresh (DHT node count etc.)
    statusUpdateTimer_ = new QTimer(this);
    connect(statusUpdateTimer_, &QTimer::timeout, this, &MainWindow::updateNetworkStatus);
    statusUpdateTimer_->start(30000);
}

void MainWindow::wireWidgets()
{
    // Every tab/panel reaches its services through the shared Application.
    if (topTorrentsWidget)
        topTorrentsWidget->setApplication(app_);
    if (feedWidget)
        feedWidget->setApplication(app_);
    if (downloadsWidget)
        downloadsWidget->setApplication(app_);
    if (activityWidget)
        activityWidget->setApplication(app_);
    if (favoritesWidget)
        favoritesWidget->setApplication(app_);
    if (detailsPanel)
        detailsPanel->setApplication(app_);
    if (filesWidget)
        filesWidget->setApplication(app_);
}

void MainWindow::connectSignals()
{
    connectSearchSignals();
    connectTabSignals();
    connectDetailsSignals();
    connectServiceSignals();
    connectPeerSignals();

    // Immediate settings application.
    // Language changes are handled by Application, which owns the translators.
    if (app_->config())
        connect(app_->config(), &rats::app::ConfigStore::darkModeChanged, this, &MainWindow::onDarkModeChanged);
}

void MainWindow::connectSearchSignals()
{
    connect(searchButton, &QPushButton::clicked, this, &MainWindow::onSearchButtonClicked);
    connect(searchLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSearchButtonClicked);
    connect(searchLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(sortComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onSortOrderChanged);

    connect(resultsTableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
        [this](const QModelIndex& current, const QModelIndex&) { onTorrentSelected(current); });
    connect(resultsTableView, &QTableView::doubleClicked, this, &MainWindow::onTorrentDoubleClicked);
    connect(resultsTableView, &QTableView::customContextMenuRequested, this, &MainWindow::showTorrentContextMenu);
}

void MainWindow::connectTabSignals()
{
    connect(tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    // Every torrent-table / activity tab exposes the same three signals (payload
    // is rats::domain::Torrent). Selecting shows the details panel and remembers
    // the selection per tab so switching back can restore it.

    // Top torrents
    connect(topTorrentsWidget, &TopTorrentsWidget::torrentSelected, this, [this](const Torrent& t) {
        tabSelection_[topTorrentsWidget] = t;
        showTorrentDetails(t);
    });
    connect(topTorrentsWidget, &TopTorrentsWidget::torrentDoubleClicked, this, &MainWindow::openMagnetLink);
    connect(topTorrentsWidget, &TopTorrentsWidget::exportTorrentRequested, this, &MainWindow::exportTorrentToFile);

    // Feed
    connect(feedWidget, &FeedWidget::torrentSelected, this, [this](const Torrent& t) {
        tabSelection_[feedWidget] = t;
        showTorrentDetails(t);
    });
    connect(feedWidget, &FeedWidget::torrentDoubleClicked, this, &MainWindow::openMagnetLink);
    connect(feedWidget, &FeedWidget::exportTorrentRequested, this, &MainWindow::exportTorrentToFile);

    // Activity
    connect(activityWidget, &ActivityWidget::torrentSelected, this, [this](const Torrent& t) {
        tabSelection_[activityWidget] = t;
        showTorrentDetails(t);
    });
    connect(activityWidget, &ActivityWidget::torrentDoubleClicked, this, &MainWindow::openMagnetLink);
    connect(activityWidget, &ActivityWidget::exportTorrentRequested, this, &MainWindow::exportTorrentToFile);

    // Favorites
    connect(favoritesWidget, &FavoritesWidget::torrentSelected, this, [this](const Torrent& t) {
        tabSelection_[favoritesWidget] = t;
        showTorrentDetails(t);
    });
    connect(favoritesWidget, &FavoritesWidget::torrentDoubleClicked, this, &MainWindow::openMagnetLink);
    connect(favoritesWidget, &FavoritesWidget::exportTorrentRequested, this, &MainWindow::exportTorrentToFile);

    // Activity tab: "Top" shortcut button switches to the Top tab.
    connect(activityWidget, &ActivityWidget::navigateToTop, this,
        [this]() { tabWidget->setCurrentWidget(topTorrentsWidget); });
}

void MainWindow::connectDetailsSignals()
{
    connect(detailsPanel, &TorrentDetailsPanel::closeRequested, this, &MainWindow::onDetailsPanelCloseRequested);
    connect(detailsPanel, &TorrentDetailsPanel::downloadRequested, this, &MainWindow::onDownloadRequested);
    connect(detailsPanel, &TorrentDetailsPanel::goToDownloadsRequested, this,
        [this]() { tabWidget->setCurrentWidget(downloadsWidget); });
    connect(detailsPanel, &TorrentDetailsPanel::downloadCancelRequested, this, [this](const QString& hash) {
        if (app_->downloads()) {
            app_->downloads()->remove(hash, /*saveResumeData*/ false);
            statusBar()->showMessage(tr("Download cancelled"), 2000);
        }
    });
}

void MainWindow::connectServiceSignals()
{
    // Transport: peer count + P2P indicator state.
    if (app_->transport()) {
        auto* transport = app_->transport();
        connect(transport, &rats::net::P2PTransport::peerCountChanged, this, &MainWindow::onPeerCountChanged);
        connect(transport, &rats::net::P2PTransport::started, this, [this]() { updateP2PState(); });
        connect(transport, &rats::net::P2PTransport::stopped, this, [this]() { updateP2PState(); });
        connect(transport, &rats::net::P2PTransport::peerConnected, this, [this](const QString&) { updateP2PState(); });
        connect(transport, &rats::net::P2PTransport::peerDisconnected, this, [this](const QString&) {
            if (app_->peers())
                cachedRemoteTorrentCount_ = app_->peers()->remoteTorrentsCount();
            updateStatusBar();
            updateP2PState();
        });
    }

    // Peer registry: swarm-wide torrent totals.
    if (app_->peers()) {
        connect(app_->peers(), &rats::service::PeerRegistry::peerStatsReceived, this,
            [this](const QString&, const rats::domain::PeerStats&) {
                cachedRemoteTorrentCount_ = app_->peers()->remoteTorrentsCount();
                updateStatusBar();
            });
    }

    // Repository statistics drive the local torrent count authoritatively.
    if (app_->torrents()) {
        connect(app_->torrents(), &rats::data::TorrentRepository::statisticsChanged, this,
            [this](qint64 torrents, qint64, qint64) {
                cachedTorrentCount_ = torrents;
                updateStatusBar();
            });
    }

    // Crawler status text.
    if (app_->crawler()) {
        connect(app_->crawler(), &rats::net::Crawler::statusChanged, this, &MainWindow::onSpiderStatusChanged);
    }

    // New torrents indexed (from any source) update the status message.
    if (app_->indexing()) {
        connect(app_->indexing(), &rats::service::IndexingService::torrentIndexed, this, &MainWindow::onTorrentIndexed);
    }

    // Update service: surface availability / progress / errors.
    if (app_->updates()) {
        auto* updates = app_->updates();
        connect(updates, &UpdateService::updateAvailable, this,
            [this](const UpdateService::UpdateInfo& info) { onUpdateAvailable(info.version, info.releaseNotes); });
        connect(updates, &UpdateService::downloadProgressChanged, this, &MainWindow::onUpdateDownloadProgress);
        connect(updates, &UpdateService::updateReady, this, &MainWindow::onUpdateReady);
        connect(updates, &UpdateService::errorOccurred, this, &MainWindow::onUpdateError);
    }

    // Download progress mirrored into the details panel if it is showing that
    // torrent.
    if (app_->downloads()) {
        auto* downloads = app_->downloads();
        connect(downloads, &rats::service::DownloadService::progressUpdated, this,
            [this](const QString& hash, const QJsonObject& progress) {
                if (detailsPanel && detailsPanel->currentHash() == hash) {
                    qint64 downloaded = progress["downloaded"].toVariant().toLongLong();
                    // progressJson emits the full size under "total" (there is no
                    // "size" key) — reading "size" always yielded 0, so the panel
                    // showed "90 MB / 0" while downloading.
                    qint64 total = progress["total"].toVariant().toLongLong();
                    double progressVal = progress["progress"].toDouble();
                    int speed = static_cast<int>(progress["downloadSpeed"].toDouble());
                    detailsPanel->setDownloadProgress(progressVal, downloaded, total, speed);
                }
            });
        connect(downloads, &rats::service::DownloadService::downloadCompleted, this, [this](const QString& hash) {
            if (detailsPanel && detailsPanel->currentHash() == hash)
                detailsPanel->setDownloadCompleted();
        });
        connect(downloads, &rats::service::DownloadService::torrentRemoved, this, [this](const QString& hash) {
            if (detailsPanel && detailsPanel->currentHash() == hash)
                detailsPanel->resetDownloadState();
        });
    }

    // Torrent export: fetch runs in the service; here we drive the save dialog,
    // surface failures, and mirror progress into the status bar.
    if (app_->exporter()) {
        auto* exporter = app_->exporter();
        connect(exporter, &rats::service::TorrentExporter::exportReady, this, &MainWindow::onExportReady);
        connect(exporter, &rats::service::TorrentExporter::exportFailed, this,
            [this](const QString&, const QString& reason) {
                QMessageBox::warning(this, tr("Export Torrent"), tr("Could not export the torrent.\n\n%1").arg(reason));
            });
        connect(exporter, &rats::service::TorrentExporter::statusMessage, this,
            [this](const QString& message, int timeoutMs) { statusBar()->showMessage(message, timeoutMs); });
    }

    // Background data migrations run (in a worker thread) while the window is up
    // — surface their progress in the status bar and report failures. Sync
    // pre-start migrations finish before this window exists (see
    // Application::start()).
    if (app_->migrations()) {
        auto* migrations = app_->migrations();
        connect(
            migrations, &rats::service::MigrationService::migrationProgress, this, &MainWindow::onMigrationProgress);
        connect(migrations, &rats::service::MigrationService::migrationError, this,
            [this](const QString& migrationId, const QString& error) {
                QMessageBox::warning(this, tr("Data Migration"),
                    tr("A background data migration failed; it "
                       "will retry on the next start.\n\n%1: %2")
                        .arg(migrationId, error));
            });
        connect(migrations, &rats::service::MigrationService::allMigrationsCompleted, this,
            [this]() { statusBar()->showMessage(tr("Data migration complete"), 4000); });
    }
}

void MainWindow::connectPeerSignals()
{
    if (!app_->peerApi())
        return;
    auto* peerApi = app_->peerApi();

    // Remote torrent search hits stream into the search-results model.
    connect(peerApi, &rats::peer::PeerApi::remoteSearchResults, this,
        [this](const QString& /*query*/, const QJsonArray& torrents) {
            if (currentSearchQuery_.isEmpty())
                return;
            for (const QJsonValue& val : torrents) {
                SearchHit hit = codec::searchHitFromJson(val.toObject());
                if (hit.torrent.isValid())
                    searchResultModel->addResult(hit);
            }
        });

    // Remote file-search hits.
    connect(peerApi, &rats::peer::PeerApi::remoteFileSearchResults, this,
        [this](const QString& /*query*/, const QJsonArray& torrents) {
            if (currentSearchQuery_.isEmpty())
                return;
            for (const QJsonValue& val : torrents) {
                SearchHit hit = codec::searchHitFromJson(val.toObject());
                hit.fromFileMatch = true;
                if (hit.torrent.isValid())
                    searchResultModel->addFileResult(hit);
            }
        });

    // A single-torrent reply from a peer: populate the bottom files panel if it
    // matches the torrent currently on screen.
    connect(peerApi, &rats::peer::PeerApi::remoteTorrentReceived, this,
        [this](const QString& hash, const QJsonObject& data) {
            if (!filesWidget || hash.isEmpty())
                return;
            if (detailsPanel && detailsPanel->currentHash() != hash)
                return;
            QVector<rats::domain::File> files = codec::filesFromJson(data["files"].toArray());
            if (!files.isEmpty()) {
                filesWidget->setFiles(hash, data["name"].toString(), files);
                filesWidget->show();
                verticalSplitter->setSizes({ 600, 200 });
            }
        });
}

void MainWindow::performSearch(const QString& query)
{
    if (query.isEmpty())
        return;

    currentSearchQuery_ = query;
    qInfo() << "Search started:" << query.left(50) << (query.length() > 50 ? "..." : "");
    statusBar()->showMessage(tr("🔍 Searching..."), 2000);

    tabWidget->setCurrentIndex(0); // switch to Search Results

    // Map the sort combo selection onto the request.
    const QString sortData = sortComboBox->currentData().toString();
    QString sort = "seeders";
    if (sortData.startsWith("seeders"))
        sort = "seeders";
    else if (sortData.startsWith("size"))
        sort = "size";
    else if (sortData.startsWith("added"))
        sort = "added";
    else if (sortData.startsWith("name"))
        sort = "name";

    SearchService::Request req;
    req.query = query;
    req.limit = 50;
    req.sort = sort;
    req.descending = sortData.endsWith("desc");
    req.safeSearch = false;

    searchResultModel->clearResults();

    // Local torrent search (synchronous).
    QVector<SearchHit> hits;
    if (app_->search())
        hits = app_->search()->searchTorrents(req);
    searchResultModel->setResults(hits);
    statusBar()->showMessage(tr("✅ Found %1 torrents").arg(hits.size()), 3000);

    // Local file search — merged in as file-match results.
    if (app_->search()) {
        QVector<SearchHit> fileHits = app_->search()->searchFiles(req);
        if (!fileHits.isEmpty()) {
            searchResultModel->addFileResults(fileHits);
            statusBar()->showMessage(
                tr("✅ Found %1 total results (incl. file matches)").arg(searchResultModel->resultCount()), 3000);
        }
    }

    // Ask connected peers as well; their answers stream back via peerApi signals.
    if (app_->transport() && app_->transport()->isRunning() && query.length() > 2) {
        QJsonObject msg;
        msg["query"] = query;
        msg["text"] = query;
        msg["limit"] = 50;
        msg["orderBy"] = sort;
        msg["orderDesc"] = req.descending;
        app_->transport()->broadcastMessage("torrent_search", msg);
        app_->transport()->broadcastMessage("searchFiles", msg);
    }

    // DHT fallback: an info-hash query (bare hash OR a magnet link) that isn't
    // indexed locally — pull the metadata from the DHT and add it as a result.
    const QString dhtHash = SearchService::extractInfoHash(query);
    if (hits.isEmpty() && !dhtHash.isEmpty() && app_->api()) {
        app_->api()->call(
            "torrent.get", QJsonObject { { "hash", dhtHash }, { "files", true } }, [this, query](const Result& result) {
                if (!result.ok() || currentSearchQuery_ != query)
                    return;
                Torrent t = codec::torrentFromJson(result.data().toObject());
                if (t.isValid()) {
                    SearchHit hit;
                    hit.torrent = t;
                    searchResultModel->addResult(hit);
                    statusBar()->showMessage(tr("✅ Found torrent via DHT"), 3000);
                }
            });
    }
}

void MainWindow::updateStatusBar()
{
    if (cachedRemoteTorrentCount_ > 0) {
        torrentCountLabel->setText(tr("📦 Torrents: %1 + %2").arg(cachedTorrentCount_).arg(cachedRemoteTorrentCount_));
    } else {
        torrentCountLabel->setText(tr("📦 Torrents: %1").arg(cachedTorrentCount_));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    rats::app::ConfigStore* config = app_ ? app_->config() : nullptr;

    // Hide to tray instead of closing if enabled.
    bool closeToTray = config ? config->trayOnClose() : false;
    if (closeToTray && trayIcon && trayIcon->isVisible()) {
        hide();
        if (!trayNotificationShown_) {
            trayIcon->showMessage(tr("Rats Search"), tr("Application is still running in the system tray."),
                QSystemTrayIcon::Information, 2000);
            trayNotificationShown_ = true;
        }
        event->ignore();
        return;
    }

    // Confirm exit.
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Confirm Exit"), tr("Are you sure you want to exit Rats Search?"), QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::No) {
        event->ignore();
        return;
    }

    // Persist UI state; service shutdown (DB, P2P, session) is
    // main()/Application's job.
    saveSettings();
    event->accept();

    if (isHidden())
        QApplication::quit();
}

void MainWindow::contextMenuEvent(QContextMenuEvent* event)
{
    Q_UNUSED(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile() && url.toLocalFile().endsWith(".torrent", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    QStringList torrentFiles;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            QString filePath = url.toLocalFile();
            if (filePath.endsWith(".torrent", Qt::CaseInsensitive))
                torrentFiles.append(filePath);
        }
    }

    if (torrentFiles.isEmpty()) {
        event->ignore();
        return;
    }

    event->acceptProposedAction();

    for (const QString& filePath : torrentFiles) {
        if (!app_->api())
            continue;
        app_->api()->call(
            "torrent.import", QJsonObject { { "file", filePath } }, [this, filePath](const Result& response) {
                if (response.ok()) {
                    QJsonObject data = response.data().toObject();
                    QString name = data["name"].toString();
                    bool alreadyExists = data["alreadyExists"].toBool();
                    statusBar()->showMessage(
                        alreadyExists ? tr("Already indexed: %1").arg(name) : tr("Added: %1").arg(name), 2000);
                    // Auto-favorite imported torrents.
                    Torrent t = codec::torrentFromJson(data);
                    if (t.isValid())
                        addToFavorites(t);
                } else {
                    qWarning() << "Failed to add torrent file:" << filePath << "-" << response.error();
                }
            });
    }

    statusBar()->showMessage(tr("Processing %1 torrent file(s)...").arg(torrentFiles.size()), 3000);
}

// ============================================================================
// Search / selection slots
// ============================================================================

void MainWindow::onSearchButtonClicked()
{
    performSearch(searchLineEdit->text());
}

void MainWindow::onSearchTextChanged(const QString& text)
{
    searchButton->setEnabled(!text.isEmpty());
}

void MainWindow::onTorrentSelected(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    Torrent torrent = searchResultModel->getTorrent(index.row());
    if (torrent.isValid())
        showTorrentDetails(torrent);
}

void MainWindow::onTorrentDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    Torrent torrent = searchResultModel->getTorrent(index.row());
    if (torrent.isValid())
        openMagnetLink(torrent);
}

void MainWindow::onSortOrderChanged(int index)
{
    Q_UNUSED(index);
    if (!currentSearchQuery_.isEmpty())
        performSearch(currentSearchQuery_);
}

void MainWindow::onTabChanged(int index)
{
    Q_UNUSED(index);

    QWidget* currentWidget = tabWidget->currentWidget();
    if (!currentWidget)
        return;

    Torrent selectedTorrent;

    if (currentWidget == resultsTableView->parentWidget()) {
        QModelIndex idx = resultsTableView->currentIndex();
        if (idx.isValid())
            selectedTorrent = searchResultModel->getTorrent(idx.row());
    } else if (currentWidget == downloadsWidget) {
        // Downloads tab has no torrent selection — hide the detail panels.
        detailsPanel->hide();
        filesWidget->clear();
        filesWidget->hide();
        return;
    } else {
        // Any of the torrent-table / activity tabs: restore its remembered
        // selection.
        selectedTorrent = tabSelection_.value(currentWidget);
    }

    if (selectedTorrent.isValid()) {
        showTorrentDetails(selectedTorrent);
    } else {
        detailsPanel->hide();
        filesWidget->clear();
        filesWidget->hide();
    }
}

void MainWindow::onDetailsPanelCloseRequested()
{
    detailsPanel->hide();
    filesWidget->clear();
    filesWidget->hide();
    resultsTableView->clearSelection();
}

void MainWindow::showTorrentDetails(const Torrent& torrent)
{
    detailsPanel->setTorrent(torrent);
    detailsPanel->show();

    // Show the file list. TorrentFilesWidget fetches the files itself (from the
    // torrent's embedded list or the repository) via the Application. A remote
    // torrent whose files aren't local still gets populated later through the
    // peerApi::remoteTorrentReceived handler.
    filesWidget->setTorrent(torrent);
    filesWidget->show();
    verticalSplitter->setSizes({ 600, 200 });
}

void MainWindow::openMagnetLink(const Torrent& torrent)
{
    QDesktopServices::openUrl(QUrl(torrent.magnetLink()));
}

void MainWindow::addToFavorites(const Torrent& torrent)
{
    if (app_->favorites() && torrent.isValid())
        app_->favorites()->add(torrent);
}

void MainWindow::onDownloadRequested(const QString& hash)
{
    qInfo() << "Download requested for torrent:" << hash.left(16);

    QMenu menu(this);
    menu.setStyleSheet(this->styleSheet());

    QString defaultPath = app_->config() ? app_->config()->downloadPath() : QString();

    QString defaultText = tr("📥 Download to default folder");
    if (!defaultPath.isEmpty()) {
        QString shortPath = defaultPath;
        if (shortPath.length() > 40)
            shortPath = "..." + shortPath.right(37);
        defaultText = tr("📥 Download to: %1").arg(shortPath);
    }
    QAction* defaultAction = menu.addAction(defaultText);
    defaultAction->setToolTip(defaultPath);

    QAction* customAction = menu.addAction(tr("📂 Choose download location..."));
    menu.addSeparator();
    QAction* cancelAction = menu.addAction(tr("❌ Cancel"));

    QAction* selectedAction = menu.exec(QCursor::pos());
    if (selectedAction == cancelAction || selectedAction == nullptr)
        return;

    QString downloadPath;
    if (selectedAction == defaultAction) {
        downloadPath = defaultPath;
    } else if (selectedAction == customAction) {
        QString startDir
            = defaultPath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) : defaultPath;
        downloadPath = QFileDialog::getExistingDirectory(this, tr("Select Download Location"), startDir);
        if (downloadPath.isEmpty())
            return;
    }

    if (app_->downloads()) {
        qInfo() << "Starting download:" << hash.left(16) << "to:" << downloadPath;
        bool ok = app_->downloads()->add(hash, downloadPath);
        if (ok) {
            statusBar()->showMessage(tr("⬇️ Download started"), 2000);
            if (detailsPanel && detailsPanel->currentHash() == hash)
                detailsPanel->setDownloadProgress(0.0, 0, 0, 0);
        } else {
            QMessageBox::warning(this, tr("Download Failed"),
                tr("Could not start the download. Check the info "
                   "hash and try again."));
        }
    }
}

void MainWindow::showTorrentContextMenu(const QPoint& pos)
{
    QModelIndex index = resultsTableView->indexAt(pos);
    if (!index.isValid())
        return;

    Torrent torrent = searchResultModel->getTorrent(index.row());
    if (!torrent.isValid())
        return;

    QMenu contextMenu(this);

    QAction* magnetAction = contextMenu.addAction(tr("Open Magnet Link"));
    connect(magnetAction, &QAction::triggered, [this, torrent]() { openMagnetLink(torrent); });

    QAction* copyHashAction = contextMenu.addAction(tr("Copy Info Hash"));
    connect(copyHashAction, &QAction::triggered, [this, torrent]() {
        QApplication::clipboard()->setText(torrent.hash);
        statusBar()->showMessage(tr("Hash copied to clipboard"), 2000);
    });

    QAction* copyMagnetAction = contextMenu.addAction(tr("Copy Magnet Link"));
    connect(copyMagnetAction, &QAction::triggered, [this, torrent]() {
        QApplication::clipboard()->setText(torrent.magnetLink());
        statusBar()->showMessage(tr("Magnet link copied to clipboard"), 2000);
    });

    contextMenu.addSeparator();

    // Favorites.
    if (app_->favorites()) {
        if (app_->favorites()->isFavorite(torrent.hash)) {
            QAction* removeFavAction = contextMenu.addAction(tr("★ Remove from Favorites"));
            connect(removeFavAction, &QAction::triggered, [this, torrent]() {
                app_->favorites()->remove(torrent.hash);
                statusBar()->showMessage(tr("Removed from favorites"), 2000);
            });
        } else {
            QAction* addFavAction = contextMenu.addAction(tr("⭐ Add to Favorites"));
            connect(addFavAction, &QAction::triggered, [this, torrent]() {
                addToFavorites(torrent);
                statusBar()->showMessage(tr("Added to favorites: %1").arg(torrent.name), 2000);
            });
        }
    }

    contextMenu.addSeparator();

    QAction* exportAction = contextMenu.addAction(tr("💾 Export to .torrent file..."));
    connect(exportAction, &QAction::triggered, [this, torrent]() { exportTorrentToFile(torrent); });

    QAction* detailsAction = contextMenu.addAction(tr("Show Details"));
    connect(detailsAction, &QAction::triggered, [this, index]() { onTorrentSelected(index); });

    contextMenu.exec(resultsTableView->viewport()->mapToGlobal(pos));
}

void MainWindow::exportTorrentToFile(const Torrent& torrent)
{
    if (!torrent.isValid())
        return;
    if (!app_->exporter()) {
        QMessageBox::warning(this, tr("Export Torrent"), tr("Export is not available in this build."));
        return;
    }
    // Async: TorrentExporter fetches metadata (BEP 9) if needed, caches the
    // .torrent, then fires exportReady → onExportReady prompts for a save path.
    app_->exporter()->requestExport(torrent.hash, torrent.name);
}

void MainWindow::onExportReady(const QString& hash, const QString& name, const QString& cachePath)
{
    // Build a readable default filename from the torrent name.
    QString suggested = name.trimmed();
    static const QRegularExpression illegal(R"([<>:"/\\|?*\x00-\x1f])");
    suggested.replace(illegal, QStringLiteral("_"));
    if (suggested.isEmpty())
        suggested = hash;
    if (suggested.length() > 200)
        suggested.truncate(200);
    const QString defaultPath = QDir(QDir::homePath()).absoluteFilePath(suggested + QStringLiteral(".torrent"));

    QString destination = QFileDialog::getSaveFileName(
        this, tr("Export Torrent As"), defaultPath, tr("Torrent Files (*.torrent);;All Files (*)"));
    if (destination.isEmpty())
        return; // user cancelled
    if (!destination.endsWith(QStringLiteral(".torrent"), Qt::CaseInsensitive))
        destination += QStringLiteral(".torrent");

    if (QFile::exists(destination))
        QFile::remove(destination);
    if (!QFile::copy(cachePath, destination)) {
        QMessageBox::critical(this, tr("Export Torrent"), tr("Failed to save torrent to:\n%1").arg(destination));
        return;
    }
    statusBar()->showMessage(tr("Torrent exported to %1").arg(destination), 4000);
}

void MainWindow::onMigrationProgress(const QString& migrationId, qint64 current, qint64 total)
{
    Q_UNUSED(migrationId);
    // Pull the human-readable description from the service (thread-safe).
    const auto progress = app_->migrations()->currentProgress();
    const QString what = progress.description.isEmpty() ? tr("Migrating data") : progress.description;
    // Shown on the left message area; the permanent peer/count widgets on the
    // right are unaffected. 0 = persist until the next progress tick /
    // completion.
    if (total > 0) {
        const int percent = static_cast<int>((current * 100) / total);
        statusBar()->showMessage(tr("%1: %2 / %3 (%4%)").arg(what).arg(current).arg(total).arg(percent), 0);
    } else {
        statusBar()->showMessage(tr("%1…").arg(what), 0);
    }
}

// ============================================================================
// Network status
// ============================================================================

void MainWindow::onPeerCountChanged(int count)
{
    peerCountLabel->setText(tr("👥 Peers: %1").arg(count));
    updateP2PState();
}

void MainWindow::updateP2PState()
{
    auto* transport = app_ ? app_->transport() : nullptr;
    if (!transport || !transport->isRunning()) {
        p2pState_ = P2PState::NotStarted;
    } else if (transport->peerCount() > 0) {
        p2pState_ = P2PState::Connected;
    } else {
        p2pState_ = P2PState::NoConnection;
    }
    updateP2PIndicator();
}

void MainWindow::onSpiderStatusChanged(const QString& status)
{
    spiderStatusLabel->setText(tr("🕷️ Spider: %1").arg(status));
}

void MainWindow::updateP2PIndicator()
{
    QString indicator;
    QString statusText;

    switch (p2pState_) {
    case P2PState::NotStarted:
        indicator = "<span style='color: #e74c3c; font-size: 14px;'>●</span>";
        statusText = tr("P2P: Not Started");
        break;
    case P2PState::NoConnection:
        indicator = "<span style='color: #f39c12; font-size: 14px;'>●</span>";
        statusText = tr("P2P: No Peers");
        break;
    case P2PState::Connected:
        indicator = "<span style='color: #27ae60; font-size: 14px;'>●</span>";
        statusText = tr("P2P: Connected");
        break;
    }

    p2pStatusLabel->setText(QString("%1 %2").arg(indicator, statusText));
}

void MainWindow::updateNetworkStatus()
{
    auto* transport = app_ ? app_->transport() : nullptr;
    if (transport && transport->isRunning()) {
        size_t dhtNodes = transport->dhtNodeCount();
        if (transport->isDhtRunning())
            dhtNodeCountLabel->setText(tr("🌐 DHT: %1 nodes").arg(dhtNodes));
        else
            dhtNodeCountLabel->setText(tr("🌐 DHT: Offline"));
    } else if (p2pState_ != P2PState::NotStarted) {
        dhtNodeCountLabel->setText(tr("🌐 DHT: Offline"));
    }
}

void MainWindow::onTorrentIndexed(const Torrent& torrent)
{
    statusBar()->showMessage(tr("📥 Indexed: %1").arg(torrent.name), 2000);
    // The torrent count follows the repository's statisticsChanged signal, and
    // tracker checks are driven by TrackerService (wired inside Application), so
    // nothing else to do here.
}

// ============================================================================
// Settings / theme / language
// ============================================================================

void MainWindow::onDarkModeChanged(bool enabled)
{
    qInfo() << "Theme changed to:" << (enabled ? "dark" : "light");
    applyTheme(enabled);
}

void MainWindow::showSettings()
{
    qInfo() << "Opening settings dialog";
    SettingsDialog dialog(app_, this);
    dialog.setStyleSheet(this->styleSheet());

    if (dialog.exec() == QDialog::Accepted) {
        qInfo() << "Settings saved by user";
        if (dialog.needsRestart()) {
            QMessageBox::information(this, tr("Restart Required"),
                tr("Some changes (network ports or data directory) will take effect "
                   "after restarting the "
                   "application."));
        }
        saveSettings();
    }
}

void MainWindow::showAbout()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("About Rats Search"));
    dialog.setFixedSize(420, 380);
    dialog.setStyleSheet(this->styleSheet());

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(12);
    layout->setContentsMargins(32, 24, 32, 24);

    QLabel* logoLabel = new QLabel("🐀");
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setObjectName("aboutLogoLabel");
    layout->addWidget(logoLabel);

    QLabel* titleLabel = new QLabel(QString("Rats Search %1").arg(RATSSEARCH_VERSION_STRING));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setObjectName("aboutTitleLabel");
    layout->addWidget(titleLabel);

    QLabel* subtitleLabel = new QLabel(tr("BitTorrent P2P Search Engine"));
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setObjectName("subtitleLabel");
    layout->addWidget(subtitleLabel);

    QLabel* gitLabel = new QLabel(QString("Git: %1").arg(RATSSEARCH_GIT_DESCRIBE));
    gitLabel->setAlignment(Qt::AlignCenter);
    gitLabel->setObjectName("hintLabel");
    layout->addWidget(gitLabel);

    layout->addSpacing(8);

    QLabel* descLabel = new QLabel(QString(tr("Built with Qt %1 and librats\n\n"
                                              "A powerful decentralized torrent search engine\n"
                                              "with DHT crawling and full-text search."))
            .arg(qVersion()));
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    layout->addSpacing(8);

    QLabel* copyrightLabel = new QLabel(tr("Copyright © 2026"));
    copyrightLabel->setAlignment(Qt::AlignCenter);
    copyrightLabel->setObjectName("hintLabel");
    layout->addWidget(copyrightLabel);

    QLabel* linkLabel
        = new QLabel(QString("<a href='https://github.com/DEgITx/rats-search'>%1</a>").arg(tr("GitHub Repository")));
    linkLabel->setAlignment(Qt::AlignCenter);
    linkLabel->setObjectName("linkLabel");
    linkLabel->setOpenExternalLinks(true);
    layout->addWidget(linkLabel);

    layout->addStretch();

    QPushButton* okButton = new QPushButton(tr("OK"));
    okButton->setFixedWidth(100);
    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    dialog.exec();
}

void MainWindow::showChangelog()
{
    qInfo() << "Showing changelog dialog";

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Changelog - What's New"));
    dialog.setMinimumSize(650, 550);
    dialog.setStyleSheet(this->styleSheet());

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    QHBoxLayout* headerLayout = new QHBoxLayout();

    QLabel* iconLabel = new QLabel("📋");
    iconLabel->setObjectName("aboutLogoLabel");
    headerLayout->addWidget(iconLabel);

    QLabel* titleLabel = new QLabel(tr("Changelog"));
    titleLabel->setObjectName("aboutTitleLabel");
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();

    QLabel* versionLabel = new QLabel(QString("v%1").arg(RATSSEARCH_VERSION_STRING));
    versionLabel->setObjectName("subtitleLabel");
    headerLayout->addWidget(versionLabel);

    layout->addLayout(headerLayout);

    QLabel* subtitleLabel = new QLabel(tr("Recent changes and updates to Rats Search"));
    subtitleLabel->setObjectName("hintLabel");
    layout->addWidget(subtitleLabel);

    QTextEdit* changelogText = new QTextEdit();
    changelogText->setReadOnly(true);
    changelogText->setMinimumHeight(350);

    QString changelogContent;
    QFile resourceFile(":/CHANGELOG.md");
    if (resourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        changelogContent = QString::fromUtf8(resourceFile.readAll());
        resourceFile.close();
    } else {
        QString appPath = QApplication::applicationDirPath();
        QFile localFile(appPath + "/CHANGELOG.md");
        if (localFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            changelogContent = QString::fromUtf8(localFile.readAll());
            localFile.close();
        } else {
            QFile devFile("CHANGELOG.md");
            if (devFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                changelogContent = QString::fromUtf8(devFile.readAll());
                devFile.close();
            }
        }
    }

    if (changelogContent.isEmpty())
        changelogContent = tr("# Changelog\n\nNo changelog available.");

    changelogText->setMarkdown(changelogContent);
    layout->addWidget(changelogText, 1);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton* closeBtn = new QPushButton(tr("Close"));
    closeBtn->setMinimumWidth(100);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    buttonLayout->addWidget(closeBtn);
    layout->addLayout(buttonLayout);

    dialog.exec();
}

// ============================================================================
// Torrent management (add / create)
// ============================================================================

void MainWindow::addTorrentFile()
{
    qInfo() << "Add torrent file dialog opened";

    QString filePath = QFileDialog::getOpenFileName(this, tr("Add Torrent File"),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        tr("Torrent Files (*.torrent);;All Files (*)"));

    if (filePath.isEmpty())
        return;

    if (!app_->api()) {
        QMessageBox::warning(this, tr("Error"), tr("API not initialized"));
        return;
    }

    app_->api()->call("torrent.import", QJsonObject { { "file", filePath } }, [this, filePath](const Result& response) {
        if (response.ok()) {
            QJsonObject data = response.data().toObject();
            QString name = data["name"].toString();
            bool alreadyExists = data["alreadyExists"].toBool();

            statusBar()->showMessage(
                alreadyExists ? tr("Torrent already in index: %1").arg(name) : tr("Added to index: %1").arg(name),
                3000);

            Torrent t = codec::torrentFromJson(data);
            if (t.isValid())
                addToFavorites(t);

            if (trayIcon && trayIcon->isVisible()) {
                trayIcon->showMessage(tr("Torrent Added"), tr("%1 has been added to the search index").arg(name),
                    QSystemTrayIcon::Information, 3000);
            }
        } else {
            qWarning() << "Failed to add torrent file:" << filePath << "-" << response.error();
            QMessageBox::warning(this, tr("Error"), tr("Failed to add torrent file:\n%1").arg(response.error()));
        }
    });
}

void MainWindow::createTorrent()
{
    qInfo() << "Create torrent dialog opened";

    QMenu menu(this);
    menu.setStyleSheet(this->styleSheet());

    QAction* fileAction = menu.addAction(tr("📄 Create from File..."));
    QAction* folderAction = menu.addAction(tr("📁 Create from Folder..."));
    menu.addSeparator();
    QAction* cancelAction = menu.addAction(tr("❌ Cancel"));

    QAction* selected = menu.exec(QCursor::pos());
    if (selected == cancelAction || selected == nullptr)
        return;

    QString path;
    if (selected == fileAction) {
        path = QFileDialog::getOpenFileName(this, tr("Select File to Create Torrent From"),
            QStandardPaths::writableLocation(QStandardPaths::HomeLocation), tr("All Files (*)"));
    } else if (selected == folderAction) {
        path = QFileDialog::getExistingDirectory(this, tr("Select Folder to Create Torrent From"),
            QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    }

    if (path.isEmpty())
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Create Torrent"));
    dialog.setMinimumWidth(450);
    dialog.setStyleSheet(this->styleSheet());

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 20, 20, 20);

    QLabel* pathLabel = new QLabel(tr("Source: %1").arg(QFileInfo(path).fileName()));
    pathLabel->setWordWrap(true);
    pathLabel->setObjectName("subtitleLabel");
    layout->addWidget(pathLabel);

    QLabel* trackersLabel = new QLabel(tr("Trackers (one per line, optional):"));
    layout->addWidget(trackersLabel);

    QTextEdit* trackersEdit = new QTextEdit();
    trackersEdit->setPlaceholderText("udp://tracker.example.com:6969/announce\nhttp://tracker2.example.com/"
                                     "announce");
    trackersEdit->setMaximumHeight(80);
    layout->addWidget(trackersEdit);

    QLabel* commentLabel = new QLabel(tr("Comment (optional):"));
    layout->addWidget(commentLabel);

    QLineEdit* commentEdit = new QLineEdit();
    commentEdit->setPlaceholderText(tr("Created with Rats Search"));
    layout->addWidget(commentEdit);

    QCheckBox* seedCheckBox = new QCheckBox(tr("Start seeding immediately"));
    seedCheckBox->setChecked(true);
    layout->addWidget(seedCheckBox);

    QProgressBar* progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setFormat(tr("Hashing pieces... %p%"));
    layout->addWidget(progressBar);

    QLabel* statusLabel = new QLabel();
    statusLabel->setObjectName("hintLabel");
    layout->addWidget(statusLabel);

    layout->addStretch();

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* cancelBtn = new QPushButton(tr("Cancel"));
    QPushButton* createBtn = new QPushButton(tr("🔨 Create Torrent"));
    createBtn->setObjectName("primaryButton");

    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(createBtn);
    layout->addLayout(buttonLayout);

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    connect(createBtn, &QPushButton::clicked, [&]() {
        if (!app_->creator()) {
            QMessageBox::warning(this, tr("Error"), tr("Torrent creator not available"));
            return;
        }

        createBtn->setEnabled(false);
        cancelBtn->setEnabled(false);
        progressBar->setVisible(true);
        progressBar->setValue(0);
        statusLabel->setText(tr("Creating torrent..."));

        QStringList trackers;
        QString trackersText = trackersEdit->toPlainText().trimmed();
        if (!trackersText.isEmpty()) {
            trackers = trackersText.split('\n', Qt::SkipEmptyParts);
            for (QString& t : trackers)
                t = t.trimmed();
            trackers.removeAll("");
        }

        QString comment = commentEdit->text().trimmed();
        if (comment.isEmpty())
            comment = "Created with Rats Search";

        auto progressCallback = [progressBar](int current, int total) {
            if (total > 0) {
                int percent = (current * 100) / total;
                QMetaObject::invokeMethod(
                    progressBar, [progressBar, percent]() { progressBar->setValue(percent); }, Qt::QueuedConnection);
            }
        };

        // TorrentCreator hashes the content, starts seeding and registers the
        // download. This is a blocking call (hashing) — acceptable for the modal.
        QString hash = app_->creator()->createAndSeed(path, trackers, comment,
            /*saveTorrentFilePath*/ QString(), progressCallback);

        if (!hash.isEmpty()) {
            statusLabel->setText(tr("✅ Torrent created successfully!"));
            QMessageBox::information(
                &dialog, tr("Torrent Created"), tr("Torrent created and seeding.\n\nHash: %1").arg(hash));
            dialog.accept();
        } else {
            statusLabel->setText(tr("❌ Failed to create torrent"));
            createBtn->setEnabled(true);
            cancelBtn->setEnabled(true);
            QMessageBox::warning(&dialog, tr("Error"), tr("Failed to create torrent."));
        }
    });

    dialog.exec();
}

// ============================================================================
// System tray
// ============================================================================

void MainWindow::setupSystemTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "System tray is not available";
        return;
    }

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/images/icon.png"));
    trayIcon->setToolTip(tr("Rats Search - P2P Torrent Search Engine"));

    trayMenu = new QMenu(this);
    trayMenu->setStyleSheet(this->styleSheet());

    QAction* showAction = trayMenu->addAction(tr("Show Window"));
    connect(showAction, &QAction::triggered, this, &MainWindow::toggleWindowVisibility);

    trayMenu->addSeparator();

    QAction* settingsAction = trayMenu->addAction(tr("Settings"));
    connect(settingsAction, &QAction::triggered, [this]() {
        show();
        activateWindow();
        showSettings();
    });

    trayMenu->addSeparator();

    QAction* quitAction = trayMenu->addAction(tr("Quit"));
    connect(quitAction, &QAction::triggered, [this]() {
        if (app_->config())
            app_->config()->setTrayOnClose(false); // Force actual close
        close();
    });

    trayIcon->setContextMenu(trayMenu);
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
    trayIcon->show();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        toggleWindowVisibility();
        break;
    default:
        break;
    }
}

void MainWindow::toggleWindowVisibility()
{
    if (isVisible() && !isMinimized()) {
        hide();
    } else {
        show();
        setWindowState(windowState() & ~Qt::WindowMinimized);
        activateWindow();
        raise();
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange) {
        bool minimizeToTray = app_ && app_->config() ? app_->config()->trayOnMinimize() : false;
        if (isMinimized() && minimizeToTray && trayIcon && trayIcon->isVisible()) {
            QTimer::singleShot(0, this, &QWidget::hide);
            if (trayIcon && !trayNotificationShown_) {
                trayIcon->showMessage(tr("Rats Search"), tr("Application minimized to tray. Click to restore."),
                    QSystemTrayIcon::Information, 2000);
                trayNotificationShown_ = true;
            }
        }
    }
}

// ============================================================================
// Settings persistence (window geometry only; config is owned by ConfigStore)
// ============================================================================

void MainWindow::loadSettings()
{
    QSettings windowSettings("RatsSearch", "RatsSearch");
    if (windowSettings.contains("window/geometry"))
        restoreGeometry(windowSettings.value("window/geometry").toByteArray());
    if (windowSettings.contains("window/state"))
        restoreState(windowSettings.value("window/state").toByteArray());
    qInfo() << "Window settings loaded";
}

void MainWindow::saveSettings()
{
    if (app_ && app_->config())
        app_->config()->save();

    QSettings windowSettings("RatsSearch", "RatsSearch");
    windowSettings.setValue("window/geometry", saveGeometry());
    windowSettings.setValue("window/state", saveState());
    windowSettings.sync();

    qInfo() << "Settings saved";
}

// ============================================================================
// Update management
// ============================================================================

void MainWindow::checkForUpdates()
{
    if (!app_->updates())
        return;
    auto* updates = app_->updates();

    statusBar()->showMessage(tr("Checking for updates..."), 3000);

    disconnect(updates, &UpdateService::noUpdateAvailable, nullptr, nullptr);

    connect(
        updates, &UpdateService::noUpdateAvailable, this,
        [this]() {
            QMessageBox::information(this, tr("No Updates Available"),
                tr("You are running the latest version of Rats Search (%1).").arg(UpdateService::currentVersion()));
        },
        Qt::SingleShotConnection);

    updates->checkForUpdates();
}

void MainWindow::onUpdateAvailable(const QString& version, const QString& releaseNotes)
{
    auto* updates = app_->updates();
    if (!updates)
        return;

    qInfo() << "Update available:" << version << "current:" << UpdateService::currentVersion();

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Update Available"));
    dialog.setMinimumSize(500, 400);
    dialog.setStyleSheet(this->styleSheet());

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    QLabel* headerLabel = new QLabel(QString("🎉 %1").arg(tr("New Version Available!")));
    headerLabel->setObjectName("headerLabel");
    layout->addWidget(headerLabel);

    QLabel* versionLabel = new QLabel(tr("A new version of Rats Search is available.\n\n"
                                         "Current version: %1\n"
                                         "New version: %2")
            .arg(UpdateService::currentVersion(), version));
    layout->addWidget(versionLabel);

    if (!releaseNotes.isEmpty()) {
        QLabel* notesHeaderLabel = new QLabel(tr("What's new:"));
        layout->addWidget(notesHeaderLabel);

        QTextEdit* notesEdit = new QTextEdit();
        notesEdit->setReadOnly(true);
        notesEdit->setMarkdown(releaseNotes);
        notesEdit->setMaximumHeight(150);
        layout->addWidget(notesEdit);
    }

    QProgressBar* progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setTextVisible(true);
    progressBar->setFormat(tr("Downloading... %p%"));
    layout->addWidget(progressBar);

    QLabel* statusLabel = new QLabel();
    statusLabel->setObjectName("subtitleLabel");
    layout->addWidget(statusLabel);

    layout->addStretch();

    QHBoxLayout* buttonLayout = new QHBoxLayout();

    QPushButton* laterBtn = new QPushButton(tr("Remind Me Later"));
    laterBtn->setObjectName("secondaryButton");

    QPushButton* downloadBtn = new QPushButton(tr("Download && Install"));
    downloadBtn->setObjectName("successButton");

    buttonLayout->addWidget(laterBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(downloadBtn);
    layout->addLayout(buttonLayout);

    connect(laterBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    connect(downloadBtn, &QPushButton::clicked, [&]() {
        downloadBtn->setEnabled(false);
        laterBtn->setText(tr("Cancel"));
        progressBar->setVisible(true);
        statusLabel->setText(tr("Starting download..."));

        connect(updates, &UpdateService::downloadProgressChanged, progressBar, &QProgressBar::setValue);

        connect(updates, &UpdateService::stateChanged, [statusLabel](UpdateService::UpdateState state) {
            switch (state) {
            case UpdateService::UpdateState::Downloading:
                statusLabel->setText(tr("Downloading update..."));
                break;
            case UpdateService::UpdateState::Extracting:
                statusLabel->setText(tr("Extracting update..."));
                break;
            case UpdateService::UpdateState::ReadyToInstall:
                statusLabel->setText(tr("Ready to install!"));
                break;
            case UpdateService::UpdateState::Error:
                statusLabel->setText(tr("Error occurred"));
                break;
            default:
                break;
            }
        });

        updates->downloadUpdate();
    });

    // Just close the dialog; the install prompt is driven by the global
    // updateReady handler wired in connectServiceSignals().
    connect(updates, &UpdateService::updateReady, &dialog, &QDialog::accept);

    connect(
        updates, &UpdateService::errorOccurred, &dialog, [statusLabel, downloadBtn, laterBtn](const QString& error) {
            statusLabel->setText(tr("Error: %1").arg(error));
            downloadBtn->setEnabled(true);
            downloadBtn->setText(tr("Retry"));
            laterBtn->setText(tr("Close"));
        });

    dialog.exec();
}

void MainWindow::onUpdateDownloadProgress(int percent)
{
    statusBar()->showMessage(tr("Downloading update: %1%").arg(percent), 1000);
}

void MainWindow::onUpdateReady()
{
    qInfo() << "Update downloaded and ready to install";

    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Install Update"),
        tr("The update has been downloaded and is ready to install.\n\n"
           "The application will close and restart automatically.\n\n"
           "Do you want to install the update now?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    if (reply == QMessageBox::Yes) {
        qInfo() << "User accepted update installation, preparing to restart...";
        saveSettings();
        // executeUpdateScript() launches the external updater and quits the app;
        // main()'s aboutToQuit → app_->stop() releases the database/searchd locks.
        if (app_->updates())
            app_->updates()->executeUpdateScript();
    }
}

void MainWindow::onUpdateError(const QString& error)
{
    statusBar()->showMessage(tr("Update error: %1").arg(error), 5000);
}

// ============================================================================
// GitHub helpers
// ============================================================================

void MainWindow::reportBug()
{
    QString body
        = QString("**Describe the bug**\n"
                  "A clear description of what the bug is.\n\n"
                  "**To Reproduce**\n"
                  "Steps to reproduce the behavior:\n"
                  "1. Go to '...'\n"
                  "2. Click on '...'\n"
                  "3. See error\n\n"
                  "**Expected behavior**\n"
                  "What you expected to happen.\n\n"
                  "**Screenshots**\n"
                  "If applicable, add screenshots.\n\n"
                  "**Environment:**\n"
                  "- App Version: %1\n"
                  "- Git: %2\n"
                  "- OS: %3\n"
                  "- Qt: %4\n")
              .arg(RATSSEARCH_VERSION_STRING, RATSSEARCH_GIT_DESCRIBE, QSysInfo::prettyProductName(), qVersion());

    QUrl url("https://github.com/DEgITx/rats-search/issues/new");
    QUrlQuery query;
    query.addQueryItem("labels", "bug");
    query.addQueryItem("title", "[Bug] ");
    query.addQueryItem("body", body);
    url.setQuery(query);

    QDesktopServices::openUrl(url);
}

void MainWindow::requestFeature()
{
    QString body = QString("**Is your feature request related to a problem?**\n"
                           "A clear description of the problem.\n\n"
                           "**Describe the solution you'd like**\n"
                           "What you want to happen.\n\n"
                           "**Additional context**\n"
                           "Any other context or screenshots.\n\n"
                           "---\n"
                           "App Version: %1\n")
                       .arg(RATSSEARCH_VERSION_STRING);

    QUrl url("https://github.com/DEgITx/rats-search/issues/new");
    QUrlQuery query;
    query.addQueryItem("labels", "enhancement");
    query.addQueryItem("title", "[Feature] ");
    query.addQueryItem("body", body);
    url.setQuery(query);

    QDesktopServices::openUrl(url);
}

// ============================================================================
// End User License Agreement
// ============================================================================

bool MainWindow::showAgreementDialog()
{
    qInfo() << "Showing End User License Agreement dialog";

    rats::app::ConfigStore* config = app_->config();

    QDialog dialog;
    dialog.setWindowTitle(tr("End User License Agreement"));
    dialog.setMinimumSize(700, 600);
    dialog.setModal(true);

    if (config && config->darkMode()) {
        QFile styleFile(":/styles/styles/dark.qss");
        if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            dialog.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
            styleFile.close();
        }
    }

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    QHBoxLayout* headerLayout = new QHBoxLayout();

    QLabel* logoLabel = new QLabel("🐀");
    logoLabel->setObjectName("aboutLogoLabel");
    headerLayout->addWidget(logoLabel);

    QLabel* titleLabel = new QLabel(tr("Rats Search - License Agreement"));
    titleLabel->setObjectName("aboutTitleLabel");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    layout->addLayout(headerLayout);

    QLabel* subtitleLabel = new QLabel(tr("Please read and accept the following End User License "
                                          "Agreement before using this software."));
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setObjectName("subtitleLabel");
    layout->addWidget(subtitleLabel);

    QTextEdit* agreementText = new QTextEdit();
    agreementText->setReadOnly(true);
    agreementText->setMinimumHeight(350);

    QString agreementContent;
    QFile resourceFile(":/AGREEMENT.md");
    if (resourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        agreementContent = QString::fromUtf8(resourceFile.readAll());
        resourceFile.close();
    } else {
        QString appPath = QApplication::applicationDirPath();
        QFile localFile(appPath + "/AGREEMENT.md");
        if (localFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            agreementContent = QString::fromUtf8(localFile.readAll());
            localFile.close();
        } else {
            QFile devFile("AGREEMENT.md");
            if (devFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                agreementContent = QString::fromUtf8(devFile.readAll());
                devFile.close();
            }
        }
    }

    if (agreementContent.isEmpty()) {
        agreementContent = R"(# END USER LICENSE AGREEMENT

**IMPORTANT — READ CAREFULLY BEFORE USING THIS SOFTWARE**

By using Rats Search, you acknowledge and agree that:

1. **ASSUMPTION OF RISK**: You assume ALL risks associated with the use of this Software, including legal, security, and privacy risks.

2. **CONTENT RESPONSIBILITY**: You are SOLELY responsible for determining the legality of any content you access, download, or share.

3. **NO WARRANTIES**: The Software is provided "AS IS" without warranty of any kind.

4. **NO LIABILITY**: The Developers shall not be liable for any damages arising from your use of the Software.

5. **COMPLIANCE**: You agree to comply with all applicable laws and regulations.

6. **INDEMNIFICATION**: You agree to indemnify and hold harmless the Developers from any claims arising from your use of the Software.

**IF YOU DO NOT AGREE TO THESE TERMS, DO NOT USE THIS SOFTWARE.**
)";
    }

    agreementText->setMarkdown(agreementContent);
    layout->addWidget(agreementText, 1);

    QCheckBox* readCheckbox = new QCheckBox(tr("I have read and understood the End User License Agreement"));
    layout->addWidget(readCheckbox);

    QLabel* warningLabel = new QLabel(tr("⚠️ By clicking 'I Accept', you acknowledge that you have "
                                         "read, understood, and agree to be bound by all "
                                         "terms and conditions of this Agreement. You accept full "
                                         "responsibility for your use of this Software."));
    warningLabel->setWordWrap(true);
    warningLabel->setObjectName("hintLabel");
    layout->addWidget(warningLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout();

    QPushButton* declineBtn = new QPushButton(tr("Decline && Exit"));
    declineBtn->setObjectName("dangerButton");
    declineBtn->setMinimumWidth(140);
    declineBtn->setCursor(Qt::PointingHandCursor);

    QPushButton* acceptBtn = new QPushButton(tr("I Accept"));
    acceptBtn->setObjectName("successButton");
    acceptBtn->setMinimumWidth(140);
    acceptBtn->setEnabled(false);
    acceptBtn->setCursor(Qt::PointingHandCursor);

    buttonLayout->addWidget(declineBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(acceptBtn);
    layout->addLayout(buttonLayout);

    connect(readCheckbox, &QCheckBox::toggled, acceptBtn, &QPushButton::setEnabled);

    bool accepted = false;
    connect(declineBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(acceptBtn, &QPushButton::clicked, [&dialog, &accepted]() {
        accepted = true;
        dialog.accept();
    });

    dialog.exec();

    if (accepted) {
        qInfo() << "User accepted the End User License Agreement";
        if (config)
            config->setAgreementAccepted(true);
        return true;
    }
    qInfo() << "User declined the End User License Agreement";
    return false;
}
