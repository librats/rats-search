#include "mainwindow.h"
#include "version.h"
#include "torrentdatabase.h"
#include "torrentspider.h"
#include "p2pnetwork.h"
#include "torrentclient.h"
#include "searchresultmodel.h"
#include "torrentitemdelegate.h"
#include "torrentdetailspanel.h"

// New tab widgets (migrated from legacy)
#include "toptorrentswidget.h"
#include "feedwidget.h"
#include "downloadswidget.h"
#include "torrentfileswidget.h"

// New API layer
#include "api/ratsapi.h"
#include "api/configmanager.h"
#include "api/apiserver.h"
#include "api/updatemanager.h"
#include "api/translationmanager.h"

// Settings dialog
#include "settingsdialog.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QCloseEvent>
#include <QLabel>
#include <QTabWidget>
#include <QSplitter>
#include <QTextEdit>
#include <QGroupBox>
#include <QProgressBar>
#include <QApplication>
#include <QComboBox>
#include <QMenu>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QDateTime>
#include <QContextMenuEvent>
#include <QSystemTrayIcon>
#include <QSettings>
#include <QDialog>
#include <QTimer>
#include <QElapsedTimer>
#include <QStyle>
#include <QJsonArray>
#include <QJsonObject>

MainWindow::MainWindow(int p2pPort, int dhtPort, const QString& dataDirectory, QWidget *parent)
    : QMainWindow(parent)
    , dataDirectory_(dataDirectory)
    , servicesStarted_(false)
    , trayIcon(nullptr)
    , trayMenu(nullptr)
{
    QElapsedTimer startupTimer;
    startupTimer.start();
    
    // Initialize configuration manager first (fast)
    qint64 configStart = startupTimer.elapsed();
    config = std::make_unique<ConfigManager>(dataDirectory_ + "/rats.json");
    config->load();
    qInfo() << "Config load took:" << (startupTimer.elapsed() - configStart) << "ms";
    
    // Apply config overrides from command line args
    if (p2pPort > 0) config->setP2pPort(p2pPort);
    if (dhtPort > 0) config->setDhtPort(dhtPort);
    
    loadSettings();
    
    setWindowTitle(tr("Rats Search %1 - BitTorrent P2P Search Engine").arg(RATSSEARCH_VERSION_STRING));
    resize(1400, 900);
    
    // Set application icon
    setWindowIcon(QIcon(":/images/icon.png"));
    
    // UI setup (show window fast)
    qint64 uiStart = startupTimer.elapsed();
    applyTheme(config->darkMode());
    setupUi();
    setupMenuBar();
    setupStatusBar();
    setupSystemTray();
    qInfo() << "UI setup took:" << (startupTimer.elapsed() - uiStart) << "ms";
    
    // Create lightweight objects (just constructors, no heavy work)
    qint64 objectsStart = startupTimer.elapsed();
    torrentDatabase = std::make_unique<TorrentDatabase>(dataDirectory_);
    torrentClient = std::make_unique<TorrentClient>(this);
    p2pNetwork = std::make_unique<P2PNetwork>(config->p2pPort(), config->dhtPort(), dataDirectory_);
    torrentSpider = std::make_unique<TorrentSpider>(torrentDatabase.get(), p2pNetwork.get());
    api = std::make_unique<RatsAPI>(this);
    updateManager = std::make_unique<UpdateManager>(this);
    qInfo() << "Object creation took:" << (startupTimer.elapsed() - objectsStart) << "ms";
    
    qInfo() << "MainWindow constructor (before deferred init):" << startupTimer.elapsed() << "ms";
    
    // Defer heavy initialization to after window is shown
    // This allows the UI to appear immediately
    QTimer::singleShot(0, this, &MainWindow::initializeServicesDeferred);
}

MainWindow::~MainWindow()
{
    saveSettings();
    stopServices();
}

void MainWindow::applyTheme(bool darkMode)
{
    // Load stylesheet from resources based on dark mode setting
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
    // Central widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);
    
    // Search bar section
    QWidget *searchSection = new QWidget();
    searchSection->setObjectName("searchSection");
    QHBoxLayout *searchLayout = new QHBoxLayout(searchSection);
    searchLayout->setContentsMargins(12, 8, 12, 8);
    searchLayout->setSpacing(12);
    
    // Logo/Title
    QLabel *logoLabel = new QLabel("🐀");
    logoLabel->setObjectName("logoLabel");
    searchLayout->addWidget(logoLabel);
    
    QLabel *titleLabel = new QLabel("Rats Search");
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
    
    // Sort combo box
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
    
    // Main vertical splitter: content area on top, files panel at bottom
    verticalSplitter = new QSplitter(Qt::Vertical, this);
    verticalSplitter->setHandleWidth(3);
    
    // Horizontal splitter for tabs + details panel
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->setHandleWidth(2);
    
    // Left side - Tab widget
    tabWidget = new QTabWidget(this);
    tabWidget->setDocumentMode(true);
    
    // Search results tab
    QWidget *searchTab = new QWidget();
    QVBoxLayout *searchTabLayout = new QVBoxLayout(searchTab);
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
    resultsTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTableView->setShowGrid(false);
    resultsTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    resultsTableView->setMouseTracking(true);
    
    // Set column widths
    resultsTableView->setColumnWidth(0, 550);  // Name
    resultsTableView->setColumnWidth(1, 100);  // Size
    resultsTableView->setColumnWidth(2, 80);   // Seeders
    resultsTableView->setColumnWidth(3, 80);   // Leechers
    resultsTableView->setColumnWidth(4, 120);  // Date
    
    // Empty state message
    QLabel *emptyLabel = new QLabel("🔍 Enter a search query to find torrents");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setObjectName("emptyStateLabel");
    
    searchTabLayout->addWidget(resultsTableView);
    tabWidget->addTab(searchTab, tr("Search Results"));
    
    // Activity tab
    QWidget *activityTab = new QWidget();
    QVBoxLayout *activityTabLayout = new QVBoxLayout(activityTab);
    activityTabLayout->setContentsMargins(8, 8, 8, 8);
    
    activityLog = new QTextEdit();
    activityLog->setReadOnly(true);
    activityLog->setPlaceholderText(tr("Activity log will appear here..."));
    activityTabLayout->addWidget(activityLog);
    
    tabWidget->addTab(activityTab, tr("Activity"));
    
    // Statistics tab
    QWidget *statsTab = new QWidget();
    QVBoxLayout *statsTabLayout = new QVBoxLayout(statsTab);
    statsTabLayout->setContentsMargins(16, 16, 16, 16);
    statsTabLayout->setSpacing(16);
    
    // P2P Stats
    QGroupBox *p2pGroup = new QGroupBox(tr("P2P Network"));
    QVBoxLayout *p2pLayout = new QVBoxLayout(p2pGroup);
    statsP2pLabel = new QLabel(tr("Connected peers: %1").arg(0) + "\n" + 
                               tr("DHT nodes: %1").arg(0) + "\n" + 
                               tr("Status: %1").arg(tr("Starting...")));
    statsP2pLabel->setObjectName("subtitleLabel");
    p2pLayout->addWidget(statsP2pLabel);
    
    // Database Stats
    QGroupBox *dbGroup = new QGroupBox(tr("Database"));
    QVBoxLayout *dbLayout = new QVBoxLayout(dbGroup);
    statsDbLabel = new QLabel(tr("Indexed torrents: %1").arg(0) + "\n" + 
                              tr("Total files: %1").arg(0) + "\n" + 
                              tr("Database size: %1 MB").arg(0));
    statsDbLabel->setObjectName("subtitleLabel");
    dbLayout->addWidget(statsDbLabel);
    
    statsTabLayout->addWidget(p2pGroup);
    statsTabLayout->addWidget(dbGroup);
    statsTabLayout->addStretch();
    
    tabWidget->addTab(statsTab, tr("Statistics"));
    
    // Top Torrents tab (migrated from legacy/app/top-page.js)
    topTorrentsWidget = new TopTorrentsWidget(this);
    connect(topTorrentsWidget, &TopTorrentsWidget::torrentSelected,
            this, [this](const TorrentInfo& torrent) {
                detailsPanel->setTorrent(torrent);
                detailsPanel->show();
                // Fetch files for bottom panel
                if (api) {
                    api->getTorrent(torrent.hash, true, QString(), [this, torrent](const ApiResponse& response) {
                        if (response.success) {
                            QJsonObject data = response.data.toObject();
                            QJsonArray files = data["filesList"].toArray();
                            if (!files.isEmpty()) {
                                filesWidget->setFiles(torrent.hash, torrent.name, files);
                                filesWidget->show();
                                verticalSplitter->setSizes({600, 200});
                            }
                        }
                    });
                }
            });
    connect(topTorrentsWidget, &TopTorrentsWidget::torrentDoubleClicked,
            this, [this](const TorrentInfo& torrent) {
                QString magnetLink = QString("magnet:?xt=urn:btih:%1&dn=%2")
                    .arg(torrent.hash)
                    .arg(QUrl::toPercentEncoding(torrent.name));
                QDesktopServices::openUrl(QUrl(magnetLink));
                logActivity(QString("🧲 Opened magnet link for: %1").arg(torrent.name));
            });
    tabWidget->addTab(topTorrentsWidget, tr("🔥 Top"));
    
    // Feed tab (migrated from legacy/app/feed-page.js)
    feedWidget = new FeedWidget(this);
    connect(feedWidget, &FeedWidget::torrentSelected,
            this, [this](const TorrentInfo& torrent) {
                detailsPanel->setTorrent(torrent);
                detailsPanel->show();
                // Fetch files for bottom panel
                if (api) {
                    api->getTorrent(torrent.hash, true, QString(), [this, torrent](const ApiResponse& response) {
                        if (response.success) {
                            QJsonObject data = response.data.toObject();
                            QJsonArray files = data["filesList"].toArray();
                            if (!files.isEmpty()) {
                                filesWidget->setFiles(torrent.hash, torrent.name, files);
                                filesWidget->show();
                                verticalSplitter->setSizes({600, 200});
                            }
                        }
                    });
                }
            });
    connect(feedWidget, &FeedWidget::torrentDoubleClicked,
            this, [this](const TorrentInfo& torrent) {
                QString magnetLink = QString("magnet:?xt=urn:btih:%1&dn=%2")
                    .arg(torrent.hash)
                    .arg(QUrl::toPercentEncoding(torrent.name));
                QDesktopServices::openUrl(QUrl(magnetLink));
                logActivity(QString("🧲 Opened magnet link for: %1").arg(torrent.name));
            });
    tabWidget->addTab(feedWidget, tr("📰 Feed"));
    
    // Downloads tab (migrated from legacy/app/download-page.js)
    downloadsWidget = new DownloadsWidget(this);
    tabWidget->addTab(downloadsWidget, tr("📥 Downloads"));
    
    mainSplitter->addWidget(tabWidget);
    
    // Right side - Details panel
    detailsPanel = new TorrentDetailsPanel(this);
    detailsPanel->setMinimumWidth(280);
    detailsPanel->setMaximumWidth(380);
    detailsPanel->hide();  // Hidden by default
    
    mainSplitter->addWidget(detailsPanel);
    mainSplitter->setSizes({900, 350});
    
    // Add horizontal splitter to vertical splitter
    verticalSplitter->addWidget(mainSplitter);
    
    // Bottom panel - Files widget (like qBittorrent)
    filesWidget = new TorrentFilesWidget(this);
    filesWidget->setMinimumHeight(120);
    filesWidget->setMaximumHeight(350);
    filesWidget->hide();  // Hidden by default until a torrent is selected
    verticalSplitter->addWidget(filesWidget);
    
    mainLayout->addWidget(verticalSplitter, 1);
}

void MainWindow::setupMenuBar()
{
    // File menu
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    
    QAction *settingsAction = fileMenu->addAction(tr("&Settings"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    
    fileMenu->addSeparator();
    
    QAction *quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QMainWindow::close);
    
    // View menu
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    
    QAction *statsAction = viewMenu->addAction(tr("&Statistics"));
    connect(statsAction, &QAction::triggered, this, &MainWindow::showStatistics);
    
    // Help menu
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    
    QAction *checkUpdateAction = helpMenu->addAction(tr("Check for &Updates..."));
    connect(checkUpdateAction, &QAction::triggered, this, &MainWindow::checkForUpdates);
    
    helpMenu->addSeparator();
    
    QAction *aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}


void MainWindow::setupStatusBar()
{
    p2pStatusLabel = new QLabel(tr("P2P: Starting..."));
    peerCountLabel = new QLabel(tr("Peers: %1").arg(0));
    torrentCountLabel = new QLabel(tr("Torrents: %1").arg(0));
    spiderStatusLabel = new QLabel(tr("Spider: Idle"));
    
    statusBar()->addWidget(p2pStatusLabel);
    statusBar()->addWidget(peerCountLabel);
    statusBar()->addWidget(torrentCountLabel);
    statusBar()->addWidget(spiderStatusLabel);
    statusBar()->addPermanentWidget(new QLabel(tr("Ready")));
}

void MainWindow::connectSignals()
{
    // Search signals
    connect(searchButton, &QPushButton::clicked, this, &MainWindow::onSearchButtonClicked);
    connect(searchLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSearchButtonClicked);
    connect(searchLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(sortComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MainWindow::onSortOrderChanged);
    
    // Table view signals
    connect(resultsTableView->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex &current, const QModelIndex &) {
                onTorrentSelected(current);
            });
    connect(resultsTableView, &QTableView::doubleClicked, this, &MainWindow::onTorrentDoubleClicked);
    connect(resultsTableView, &QTableView::customContextMenuRequested, 
            this, &MainWindow::showTorrentContextMenu);
    
    // Details panel signals
    connect(detailsPanel, &TorrentDetailsPanel::closeRequested, 
            this, &MainWindow::onDetailsPanelCloseRequested);
    connect(detailsPanel, &TorrentDetailsPanel::magnetLinkRequested,
            this, &MainWindow::onMagnetLinkRequested);
    connect(detailsPanel, &TorrentDetailsPanel::downloadRequested,
            this, &MainWindow::onDownloadRequested);
    
    // P2P Network signals
    connect(p2pNetwork.get(), &P2PNetwork::statusChanged, this, &MainWindow::onP2PStatusChanged);
    connect(p2pNetwork.get(), &P2PNetwork::peerCountChanged, this, &MainWindow::onPeerCountChanged);
    
    // Spider signals
    connect(torrentSpider.get(), &TorrentSpider::statusChanged, this, &MainWindow::onSpiderStatusChanged);
    connect(torrentSpider.get(), &TorrentSpider::torrentIndexed, this, &MainWindow::onTorrentIndexed);
    
    // RatsAPI signals - for torrents indexed via DHT metadata, P2P, .torrent import
    if (api) {
        connect(api.get(), &RatsAPI::torrentIndexed, this, &MainWindow::onTorrentIndexed);
        
        // Handle remote file search results from P2P peers
        connect(api.get(), &RatsAPI::remoteFileSearchResults, this, 
            [this](const QString& searchId, const QJsonArray& torrents) {
                // Only process if this matches our current search
                if (searchId.isEmpty() || currentSearchQuery_.isEmpty()) {
                    return;
                }
                
                // Convert and add to model
                for (const QJsonValue& val : torrents) {
                    QJsonObject obj = val.toObject();
                    TorrentInfo info;
                    info.hash = obj["hash"].toString();
                    info.name = obj["name"].toString();
                    info.size = obj["size"].toVariant().toLongLong();
                    info.files = obj["files"].toInt();
                    info.seeders = obj["seeders"].toInt();
                    info.leechers = obj["leechers"].toInt();
                    info.completed = obj["completed"].toInt();
                    info.added = QDateTime::fromMSecsSinceEpoch(obj["added"].toVariant().toLongLong());
                    info.contentType = obj["contentType"].toString();
                    info.contentCategory = obj["contentCategory"].toString();
                    info.good = obj["good"].toInt();
                    info.bad = obj["bad"].toInt();
                    info.isFileMatch = obj["isFileMatch"].toBool(true);
                    
                    // Get matching paths
                    if (obj.contains("matchingPaths")) {
                        QJsonArray paths = obj["matchingPaths"].toArray();
                        for (const QJsonValue& pathVal : paths) {
                            info.matchingPaths.append(pathVal.toString());
                        }
                    }
                    
                    searchResultModel->addFileResult(info);
                }
                
                logActivity(QString("📡 Received %1 remote file results").arg(torrents.size()));
            });
    }
    
    // ConfigManager signals - for immediate settings application
    if (config) {
        connect(config.get(), &ConfigManager::darkModeChanged, this, &MainWindow::onDarkModeChanged);
        connect(config.get(), &ConfigManager::languageChanged, this, &MainWindow::onLanguageChanged);
    }
}

void MainWindow::startServices()
{
    if (servicesStarted_) {
        return;
    }
    
    QElapsedTimer timer;
    timer.start();
    
    logActivity("🔧 Initializing services...");
    
    // Initialize database (this starts Manticore - the slowest part)
    qint64 dbStart = timer.elapsed();
    if (!torrentDatabase->initialize()) {
        QMessageBox::critical(this, "Error", "Failed to initialize database!");
        logActivity("❌ Database initialization failed");
        return;
    }
    qInfo() << "Database initialize took:" << (timer.elapsed() - dbStart) << "ms";
    logActivity("✅ Database initialized");
    
    // Start P2P network
    qint64 p2pStart = timer.elapsed();
    if (!p2pNetwork->start()) {
        QMessageBox::warning(this, "Warning", "Failed to start P2P network. Some features may be limited.");
        logActivity("⚠️ P2P network failed to start");
    } else {
        qInfo() << "P2P network start took:" << (timer.elapsed() - p2pStart) << "ms";
        logActivity("✅ P2P network started");
        
        // Initialize TorrentClient after P2P is running (requires RatsClient)
        if (torrentClient && !torrentClient->isReady()) {
            qint64 tcStart = timer.elapsed();
            if (torrentClient->initialize(p2pNetwork.get(), torrentDatabase.get())) {
                qInfo() << "TorrentClient initialize took:" << (timer.elapsed() - tcStart) << "ms";
                logActivity("✅ TorrentClient initialized");
            } else {
                qWarning() << "Failed to initialize TorrentClient";
                logActivity("⚠️ TorrentClient initialization failed - downloads disabled");
            }
        }
    }
    
    // Start torrent spider
    qint64 spiderStart = timer.elapsed();
    if (!torrentSpider->start()) {
        QMessageBox::warning(this, "Warning", "Failed to start torrent spider. Automatic indexing disabled.");
        logActivity("⚠️ Torrent spider failed to start");
    } else {
        qInfo() << "Spider start took:" << (timer.elapsed() - spiderStart) << "ms";
        logActivity("✅ Torrent spider started");
    }
    
    servicesStarted_ = true;
    updateStatusBar();
    
    qInfo() << "All services started in:" << timer.elapsed() << "ms";
}

void MainWindow::stopServices()
{
    if (!servicesStarted_) {
        return;
    }
    
    logActivity("🛑 Stopping services...");
    
    // Stop services in reverse order
    if (torrentSpider) {
        torrentSpider->stop();
    }
    
    if (p2pNetwork) {
        p2pNetwork->stop();
    }
    
    servicesStarted_ = false;
}

void MainWindow::initializeServicesDeferred()
{
    QElapsedTimer timer;
    timer.start();
    
    qInfo() << "Starting deferred initialization...";
    
    // Initialize RatsAPI with all dependencies
    // RatsAPI::initialize() automatically sets up P2P message handlers
    // All P2P API logic is centralized in RatsAPI (like legacy api.js)
    qint64 apiStart = timer.elapsed();
    api->initialize(torrentDatabase.get(), p2pNetwork.get(), torrentClient.get(), config.get());
    qInfo() << "RatsAPI initialize took:" << (timer.elapsed() - apiStart) << "ms";
    
    // Connect signals before starting services
    connectSignals();
    
    // Connect RatsAPI remote search results to UI
    // When RatsAPI receives search results from other peers, update UI
    connect(api.get(), &RatsAPI::remoteSearchResults,
            this, [this](const QString& /*searchId*/, const QJsonArray& torrents) {
        // Add remote results to current search
        for (const QJsonValue& val : torrents) {
            QJsonObject obj = val.toObject();
            TorrentInfo info;
            info.hash = obj["hash"].toString();
            if (info.hash.isEmpty()) {
                info.hash = obj["info_hash"].toString();
            }
            info.name = obj["name"].toString();
            info.size = obj["size"].toVariant().toLongLong();
            info.seeders = obj["seeders"].toInt();
            info.leechers = obj["leechers"].toInt();
            
            if (info.isValid()) {
                searchResultModel->addResult(info);
            }
        }
    });
    
    if (feedWidget) {
        feedWidget->setApi(api.get());
    }
    
    if (downloadsWidget) {
        downloadsWidget->setApi(api.get());
        downloadsWidget->setTorrentClient(torrentClient.get());
    }
    
    // Start REST/WebSocket API server if enabled
    if (config->restApiEnabled()) {
        apiServer = std::make_unique<ApiServer>(api.get());
        if (apiServer->start(config->httpPort())) {
            logActivity(QString("🌐 API server started on port %1").arg(config->httpPort()));
        }
    }
    
    // Start heavy services (database, P2P, spider)
    qint64 servicesStart = timer.elapsed();
    startServices();
    qInfo() << "startServices took:" << (timer.elapsed() - servicesStart) << "ms";

    // Initialize new tab widgets with API
    if (topTorrentsWidget) {
        topTorrentsWidget->setApi(api.get());
        // Connect remote top torrents signal
        connect(api.get(), &RatsAPI::remoteTopTorrents,
                topTorrentsWidget, &TopTorrentsWidget::handleRemoteTopTorrents);
    }
    
    qInfo() << "Total deferred initialization:" << timer.elapsed() << "ms";
    logActivity("🚀 Rats Search started");
    
    // Connect to database statistics signal (updated incrementally like legacy spider.js)
    connect(torrentDatabase.get(), &TorrentDatabase::statisticsChanged,
            this, &MainWindow::onDatabaseStatisticsChanged);
    
    // Load initial statistics from database (one-time, cached)
    auto stats = torrentDatabase->getStatistics();
    cachedTorrents_ = stats.totalTorrents;
    cachedFiles_ = stats.totalFiles;
    cachedTotalSize_ = stats.totalSize;
    
    // Load initial P2P statistics
    if (p2pNetwork) {
        cachedPeerCount_ = p2pNetwork->getPeerCount();
        cachedDhtNodes_ = static_cast<int>(p2pNetwork->getDhtNodeCount());
        cachedP2pConnected_ = p2pNetwork->isConnected();
    }
    
    // Initial UI update
    updateStatisticsTab();
    
    // Setup update manager and check for updates on startup
    if (updateManager) {
        connect(updateManager.get(), &UpdateManager::updateAvailable, 
                this, [this](const UpdateManager::UpdateInfo& info) {
                    onUpdateAvailable(info.version, info.releaseNotes);
                });
        connect(updateManager.get(), &UpdateManager::downloadProgressChanged,
                this, &MainWindow::onUpdateDownloadProgress);
        connect(updateManager.get(), &UpdateManager::updateReady,
                this, &MainWindow::onUpdateReady);
        connect(updateManager.get(), &UpdateManager::errorOccurred,
                this, &MainWindow::onUpdateError);
        
        // Check for updates after a short delay
        if (config->checkUpdatesOnStartup()) {
            QTimer::singleShot(5000, this, [this]() {
                logActivity("🔍 Checking for updates...");
                updateManager->checkForUpdates();
            });
        }
    }
}

void MainWindow::performSearch(const QString &query)
{
    if (query.isEmpty()) {
        return;
    }
    
    currentSearchQuery_ = query;
    statusBar()->showMessage("🔍 Searching...", 2000);
    logActivity(QString("🔍 Searching for: %1").arg(query));
    
    // Parse sort options
    QString sortData = sortComboBox->currentData().toString();
    QString orderBy = "seeders";
    bool orderDesc = true;
    
    if (sortData.contains("seeders")) orderBy = "seeders";
    else if (sortData.contains("size")) orderBy = "size";
    else if (sortData.contains("added")) orderBy = "added";
    else if (sortData.contains("name")) orderBy = "name";
    
    orderDesc = sortData.contains("desc");
    
    // Use RatsAPI for searching - search both torrents and files
    QJsonObject options;
    options["limit"] = 50;  // Lower limit since we search both
    options["safeSearch"] = false;
    options["orderBy"] = orderBy;
    options["orderDesc"] = orderDesc;
    
    // Clear previous results
    searchResultModel->clearResults();
    
    // Helper to convert JSON to TorrentInfo
    auto jsonToTorrentInfo = [](const QJsonObject& obj, bool isFileMatch = false) -> TorrentInfo {
        TorrentInfo info;
        info.hash = obj["hash"].toString();
        info.name = obj["name"].toString();
        info.size = obj["size"].toVariant().toLongLong();
        info.files = obj["files"].toInt();
        info.seeders = obj["seeders"].toInt();
        info.leechers = obj["leechers"].toInt();
        info.completed = obj["completed"].toInt();
        info.added = QDateTime::fromMSecsSinceEpoch(obj["added"].toVariant().toLongLong());
        info.contentType = obj["contentType"].toString();
        info.contentCategory = obj["contentCategory"].toString();
        info.good = obj["good"].toInt();
        info.bad = obj["bad"].toInt();
        info.isFileMatch = isFileMatch || obj["isFileMatch"].toBool(false);
        
        // Get matching paths for file search results
        if (obj.contains("matchingPaths")) {
            QJsonArray paths = obj["matchingPaths"].toArray();
            for (const QJsonValue& pathVal : paths) {
                info.matchingPaths.append(pathVal.toString());
            }
        }
        
        return info;
    };
    
    // Search torrents by name
    api->searchTorrents(query, options, [this, query, jsonToTorrentInfo](const ApiResponse& response) {
        if (!response.success) {
            statusBar()->showMessage(QString("❌ Torrent search failed: %1").arg(response.error), 3000);
            return;
        }
        
        // Convert JSON to TorrentInfo for model
        QJsonArray torrents = response.data.toArray();
        QVector<TorrentInfo> results;
        for (const QJsonValue& val : torrents) {
            results.append(jsonToTorrentInfo(val.toObject(), false));
        }
        
        searchResultModel->addResults(results);
        statusBar()->showMessage(QString("✅ Found %1 torrents").arg(results.size()), 3000);
        logActivity(QString("✅ Found %1 torrent results").arg(results.size()));
    });
    
    // Also search files within torrents
    api->searchFiles(query, options, [this, query, jsonToTorrentInfo](const ApiResponse& response) {
        if (!response.success) {
            // File search may fail for short queries, that's OK
            return;
        }
        
        // Convert JSON to TorrentInfo for model
        QJsonArray torrents = response.data.toArray();
        QVector<TorrentInfo> results;
        for (const QJsonValue& val : torrents) {
            results.append(jsonToTorrentInfo(val.toObject(), true));
        }
        
        if (!results.isEmpty()) {
            // Add file results - will be merged with existing if same hash
            searchResultModel->addFileResults(results);
            int total = searchResultModel->resultCount();
            statusBar()->showMessage(QString("✅ Found %1 total results (incl. file matches)").arg(total), 3000);
            logActivity(QString("✅ Found %1 file match results").arg(results.size()));
        }
    });
}

void MainWindow::updateStatusBar()
{
    if (api) {
        api->getStatistics([this](const ApiResponse& response) {
            if (response.success) {
                QJsonObject stats = response.data.toObject();
                qint64 count = stats["torrents"].toVariant().toLongLong();
                torrentCountLabel->setText(QString("📦 Torrents: %1").arg(count));
            }
        });
    }
}

void MainWindow::updateStatisticsTab()
{
    // Update P2P statistics from cached values (no DB query)
    if (statsP2pLabel) {
        QString statusText = cachedP2pConnected_ ? tr("Connected") : tr("Disconnected");
        
        statsP2pLabel->setText(
            tr("Connected peers: %1").arg(cachedPeerCount_) + "\n" +
            tr("DHT nodes: %1").arg(cachedDhtNodes_) + "\n" +
            tr("Status: %1").arg(statusText)
        );
    }
    
    // Update database statistics from cached values (no DB query)
    if (statsDbLabel) {
        // Convert size to appropriate unit
        QString sizeStr;
        if (cachedTotalSize_ >= 1024LL * 1024 * 1024 * 1024) {
            sizeStr = QString("%1 TB").arg(cachedTotalSize_ / (1024.0 * 1024 * 1024 * 1024), 0, 'f', 2);
        } else if (cachedTotalSize_ >= 1024LL * 1024 * 1024) {
            sizeStr = QString("%1 GB").arg(cachedTotalSize_ / (1024.0 * 1024 * 1024), 0, 'f', 2);
        } else if (cachedTotalSize_ >= 1024LL * 1024) {
            sizeStr = QString("%1 MB").arg(cachedTotalSize_ / (1024.0 * 1024), 0, 'f', 2);
        } else {
            sizeStr = QString("%1 KB").arg(cachedTotalSize_ / 1024.0, 0, 'f', 2);
        }
        
        statsDbLabel->setText(
            tr("Indexed torrents: %1").arg(cachedTorrents_) + "\n" +
            tr("Total files: %1").arg(cachedFiles_) + "\n" +
            tr("Total size: %1").arg(sizeStr)
        );
    }
}

void MainWindow::logActivity(const QString &message)
{
    if (activityLog) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        activityLog->append(QString("[%1] %2").arg(timestamp, message));
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Hide to tray instead of closing if enabled
    bool closeToTray = config ? config->trayOnClose() : false;
    if (closeToTray && trayIcon && trayIcon->isVisible()) {
        hide();
        trayIcon->showMessage("Rats Search", 
            "Application is still running in the system tray.",
            QSystemTrayIcon::Information, 2000);
        event->ignore();
        return;
    }
    
    // Confirm close if services are running
    if (servicesStarted_) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, 
            tr("Confirm Exit"),
            tr("Are you sure you want to exit Rats Search?"),
            QMessageBox::Yes | QMessageBox::No);
            
        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }
    }
    
    // Stop API server
    if (apiServer) {
        apiServer->stop();
    }
    
    saveSettings();
    stopServices();
    event->accept();
}

void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    Q_UNUSED(event);
}

// Slots implementation
void MainWindow::onSearchButtonClicked()
{
    QString query = searchLineEdit->text();
    qInfo() << "Search button clicked, query:" << query;
    performSearch(query);
}

void MainWindow::onSearchTextChanged(const QString &text)
{
    searchButton->setEnabled(!text.isEmpty());
}

void MainWindow::onTorrentSelected(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    
    TorrentInfo torrent = searchResultModel->getTorrent(index.row());
    if (torrent.isValid()) {
        detailsPanel->setTorrent(torrent);
        detailsPanel->show();
        
        // Clear files widget while loading
        filesWidget->clear();
        
        // Fetch full details with files from database/DHT
        if (api) {
            api->getTorrent(torrent.hash, true, QString(), [this, torrent](const ApiResponse& response) {
                if (response.success) {
                    QJsonObject data = response.data.toObject();
                    QJsonArray files = data["filesList"].toArray();
                    if (!files.isEmpty()) {
                        // Update files in bottom panel and show it
                        filesWidget->setFiles(torrent.hash, torrent.name, files);
                        filesWidget->show();
                        // Set splitter sizes after showing
                        verticalSplitter->setSizes({600, 200});
                    }
                }
            });
        }
    }
}

void MainWindow::onTorrentDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    
    // Open magnet link on double click
    TorrentInfo torrent = searchResultModel->getTorrent(index.row());
    if (torrent.isValid()) {
        QString magnetLink = QString("magnet:?xt=urn:btih:%1&dn=%2")
            .arg(torrent.hash)
            .arg(QUrl::toPercentEncoding(torrent.name));
        QDesktopServices::openUrl(QUrl(magnetLink));
        logActivity(QString("🧲 Opened magnet link for: %1").arg(torrent.name));
    }
}

void MainWindow::onSortOrderChanged(int index)
{
    Q_UNUSED(index);
    if (!currentSearchQuery_.isEmpty()) {
        performSearch(currentSearchQuery_);
    }
}

void MainWindow::onDetailsPanelCloseRequested()
{
    detailsPanel->hide();
    filesWidget->clear();
    filesWidget->hide();
    resultsTableView->clearSelection();
}

void MainWindow::onMagnetLinkRequested(const QString &hash, const QString &name)
{
    logActivity(QString("🧲 Magnet link opened: %1").arg(name));
    Q_UNUSED(hash);
}

void MainWindow::onDownloadRequested(const QString &hash)
{
    logActivity(QString("⬇️ Download requested: %1").arg(hash));
    
    // Use RatsAPI to start download
    if (api) {
        api->downloadAdd(hash, QString(), [this, hash](const ApiResponse& response) {
            if (response.success) {
                logActivity(QString("✅ Download started: %1").arg(hash));
                statusBar()->showMessage("⬇️ Download started", 2000);
            } else {
                logActivity(QString("❌ Download failed: %1").arg(response.error));
                QMessageBox::warning(this, "Download Failed", response.error);
            }
        });
    }
}

void MainWindow::showTorrentContextMenu(const QPoint &pos)
{
    QModelIndex index = resultsTableView->indexAt(pos);
    if (!index.isValid()) {
        return;
    }
    
    TorrentInfo torrent = searchResultModel->getTorrent(index.row());
    if (!torrent.isValid()) {
        return;
    }
    
    QMenu contextMenu(this);
    
    QAction *magnetAction = contextMenu.addAction(tr("Open Magnet Link"));
    connect(magnetAction, &QAction::triggered, [this, torrent]() {
        QString magnetLink = QString("magnet:?xt=urn:btih:%1&dn=%2")
            .arg(torrent.hash)
            .arg(QUrl::toPercentEncoding(torrent.name));
        QDesktopServices::openUrl(QUrl(magnetLink));
        logActivity(tr("Opened magnet link for: %1").arg(torrent.name));
    });
    
    QAction *copyHashAction = contextMenu.addAction(tr("Copy Info Hash"));
    connect(copyHashAction, &QAction::triggered, [this, torrent]() {
        QApplication::clipboard()->setText(torrent.hash);
        statusBar()->showMessage(tr("Hash copied to clipboard"), 2000);
    });
    
    QAction *copyMagnetAction = contextMenu.addAction(tr("Copy Magnet Link"));
    connect(copyMagnetAction, &QAction::triggered, [this, torrent]() {
        QString magnetLink = QString("magnet:?xt=urn:btih:%1&dn=%2")
            .arg(torrent.hash)
            .arg(QUrl::toPercentEncoding(torrent.name));
        QApplication::clipboard()->setText(magnetLink);
        statusBar()->showMessage(tr("Magnet link copied to clipboard"), 2000);
    });
    
    contextMenu.addSeparator();
    
    QAction *detailsAction = contextMenu.addAction(tr("Show Details"));
    connect(detailsAction, &QAction::triggered, [this, index]() {
        onTorrentSelected(index);
    });
    
    contextMenu.exec(resultsTableView->viewport()->mapToGlobal(pos));
}

void MainWindow::onP2PStatusChanged(const QString &status)
{
    p2pStatusLabel->setText(tr("P2P: %1").arg(status));
    logActivity(tr("P2P status: %1").arg(status));
    
    // Update cached P2P connection status
    if (p2pNetwork) {
        cachedP2pConnected_ = p2pNetwork->isConnected();
        cachedDhtNodes_ = static_cast<int>(p2pNetwork->getDhtNodeCount());
    }
    updateStatisticsTab();
}

void MainWindow::onPeerCountChanged(int count)
{
    cachedPeerCount_ = count;
    peerCountLabel->setText(tr("Peers: %1").arg(count));
    
    // Also update DHT nodes from P2P network
    if (p2pNetwork) {
        cachedDhtNodes_ = static_cast<int>(p2pNetwork->getDhtNodeCount());
        cachedP2pConnected_ = p2pNetwork->isConnected();
    }
    
    // Update statistics tab with new P2P data
    updateStatisticsTab();
}

void MainWindow::onDatabaseStatisticsChanged(qint64 torrents, qint64 files, qint64 totalSize)
{
    // Update cached statistics (called when torrent is added/removed)
    cachedTorrents_ = torrents;
    cachedFiles_ = files;
    cachedTotalSize_ = totalSize;
    
    // Update status bar
    torrentCountLabel->setText(QString("📦 Torrents: %1").arg(torrents));
    
    // Update statistics tab
    updateStatisticsTab();
}

void MainWindow::onSpiderStatusChanged(const QString &status)
{
    spiderStatusLabel->setText(tr("Spider: %1").arg(status));
}

void MainWindow::onTorrentIndexed(const QString &infoHash, const QString &name)
{
    statusBar()->showMessage(QString("📥 Indexed: %1").arg(name), 2000);
    updateStatusBar();
    
    // Automatically check trackers for seeders/leechers info (like legacy spider.js)
    if (api && config && config->trackersEnabled()) {
        api->checkTrackers(infoHash, [infoHash](const ApiResponse& response) {
            if (response.success) {
                QJsonObject data = response.data.toObject();
                if (data["status"].toString() == "success") {
                    qInfo() << "Tracker check for" << infoHash.left(8) 
                            << "- seeders:" << data["seeders"].toInt()
                             << "leechers:" << data["leechers"].toInt();
                }
            }
        });
    }
}

void MainWindow::onDarkModeChanged(bool enabled)
{
    applyTheme(enabled);
    logActivity(tr("Theme changed to %1 mode").arg(enabled ? tr("dark") : tr("light")));
}

void MainWindow::onLanguageChanged(const QString& languageCode)
{
    // Use TranslationManager to switch language at runtime
    auto& translationManager = TranslationManager::instance();
    if (translationManager.setLanguage(languageCode)) {
        logActivity(tr("Language changed to %1").arg(languageCode));
        // Note: Full UI retranslation would require recreating widgets or using Qt's retranslateUi pattern
        // For now, most static strings will update on next window open
    }
}

void MainWindow::showSettings()
{
    if (!config) return;
    
    SettingsDialog dialog(config.get(), api.get(), dataDirectory_, this);
    dialog.setStyleSheet(this->styleSheet());
    
    if (dialog.exec() == QDialog::Accepted) {
        // Show restart message only for settings that genuinely require restart
        if (dialog.needsRestart()) {
            QMessageBox::information(this, tr("Restart Required"), 
                tr("Some changes (network ports or data directory) will take effect after restarting the application."));
        }
        
        saveSettings();
        logActivity(tr("Settings saved"));
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
    
    // Logo
    QLabel* logoLabel = new QLabel("🐀");
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setObjectName("aboutLogoLabel");
    layout->addWidget(logoLabel);
    
    // Title
    QLabel* titleLabel = new QLabel(QString("Rats Search %1").arg(RATSSEARCH_VERSION_STRING));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setObjectName("aboutTitleLabel");
    layout->addWidget(titleLabel);
    
    // Subtitle
    QLabel* subtitleLabel = new QLabel(tr("BitTorrent P2P Search Engine"));
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setObjectName("subtitleLabel");
    layout->addWidget(subtitleLabel);
    
    // Git version
    QLabel* gitLabel = new QLabel(QString("Git: %1").arg(RATSSEARCH_GIT_DESCRIBE));
    gitLabel->setAlignment(Qt::AlignCenter);
    gitLabel->setObjectName("hintLabel");
    layout->addWidget(gitLabel);
    
    layout->addSpacing(8);
    
    // Description
    QLabel* descLabel = new QLabel(QString(tr("Built with Qt %1 and librats\n\n"
        "A powerful decentralized torrent search engine\n"
        "with DHT crawling and full-text search.")).arg(qVersion()));
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);
    
    layout->addSpacing(8);
    
    // Copyright
    QLabel* copyrightLabel = new QLabel(tr("Copyright © 2026"));
    copyrightLabel->setAlignment(Qt::AlignCenter);
    copyrightLabel->setObjectName("hintLabel");
    layout->addWidget(copyrightLabel);
    
    // GitHub link
    QLabel* linkLabel = new QLabel("<a href='https://github.com/DEgITx/rats-search'>GitHub Repository</a>");
    linkLabel->setAlignment(Qt::AlignCenter);
    linkLabel->setObjectName("linkLabel");
    linkLabel->setOpenExternalLinks(true);
    layout->addWidget(linkLabel);
    
    layout->addStretch();
    
    // OK button
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

void MainWindow::showStatistics()
{
    tabWidget->setCurrentIndex(2);  // Switch to Statistics tab
}

void MainWindow::setupSystemTray()
{
    // Check if system tray is available
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "System tray is not available";
        return;
    }
    
    // Create tray icon
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/images/icon.png"));
    trayIcon->setToolTip("Rats Search - P2P Torrent Search Engine");
    
    // Create tray menu
    trayMenu = new QMenu(this);
    trayMenu->setStyleSheet(this->styleSheet());
    
    QAction *showAction = trayMenu->addAction(tr("Show Window"));
    connect(showAction, &QAction::triggered, this, &MainWindow::toggleWindowVisibility);
    
    trayMenu->addSeparator();
    
    QAction *statsAction = trayMenu->addAction(tr("Statistics"));
    connect(statsAction, &QAction::triggered, [this]() {
        show();
        activateWindow();
        showStatistics();
    });
    
    QAction *settingsAction = trayMenu->addAction(tr("Settings"));
    connect(settingsAction, &QAction::triggered, [this]() {
        show();
        activateWindow();
        showSettings();
    });
    
    trayMenu->addSeparator();
    
    QAction *quitAction = trayMenu->addAction(tr("Quit"));
    connect(quitAction, &QAction::triggered, [this]() {
        if (config) config->setTrayOnClose(false);  // Force actual close
        close();
    });
    
    trayIcon->setContextMenu(trayMenu);
    
    // Connect tray icon signals
    connect(trayIcon, &QSystemTrayIcon::activated, 
            this, &MainWindow::onTrayIconActivated);
    
    trayIcon->show();
    logActivity("🔔 System tray initialized");
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

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    
    if (event->type() == QEvent::WindowStateChange) {
        bool minimizeToTray = config ? config->trayOnMinimize() : false;
        if (isMinimized() && minimizeToTray && trayIcon && trayIcon->isVisible()) {
            // Hide to tray when minimized
            QTimer::singleShot(0, this, &QWidget::hide);
            if (trayIcon) {
                trayIcon->showMessage("Rats Search", 
                    "Application minimized to tray. Click to restore.",
                    QSystemTrayIcon::Information, 2000);
            }
        }
    }
}

void MainWindow::loadSettings()
{
    if (!config) return;
    
    // Window geometry - use QSettings for window-specific state
    QSettings windowSettings("RatsSearch", "RatsSearch");
    if (windowSettings.contains("window/geometry")) {
        restoreGeometry(windowSettings.value("window/geometry").toByteArray());
    }
    if (windowSettings.contains("window/state")) {
        restoreState(windowSettings.value("window/state").toByteArray());
    }
    
    qInfo() << "Settings loaded";
}

// ============================================================================
// Update Management
// ============================================================================

void MainWindow::checkForUpdates()
{
    if (!updateManager) return;
    
    logActivity("🔍 Checking for updates...");
    statusBar()->showMessage("Checking for updates...", 3000);
    
    // Disconnect previous connections to avoid duplicates
    disconnect(updateManager.get(), &UpdateManager::noUpdateAvailable, nullptr, nullptr);
    disconnect(updateManager.get(), &UpdateManager::checkComplete, nullptr, nullptr);
    
    // Connect for this manual check
    connect(updateManager.get(), &UpdateManager::noUpdateAvailable, this, [this]() {
        logActivity("✅ You have the latest version");
        QMessageBox::information(this, tr("No Updates Available"),
            tr("You are running the latest version of Rats Search (%1).")
                .arg(UpdateManager::currentVersion()));
    }, Qt::SingleShotConnection);
    
    updateManager->checkForUpdates();
}

void MainWindow::onUpdateAvailable(const QString& version, const QString& releaseNotes)
{
    logActivity(QString("🆕 Update available: version %1").arg(version));
    
    // Show update dialog
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Update Available"));
    dialog.setMinimumSize(500, 400);
    dialog.setStyleSheet(this->styleSheet());
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);
    
    // Header
    QLabel* headerLabel = new QLabel(QString("🎉 %1").arg(tr("New Version Available!")));
    headerLabel->setObjectName("headerLabel");
    layout->addWidget(headerLabel);
    
    // Version info
    QLabel* versionLabel = new QLabel(
        tr("A new version of Rats Search is available.\n\n"
           "Current version: %1\n"
           "New version: %2")
        .arg(UpdateManager::currentVersion(), version));
    layout->addWidget(versionLabel);
    
    // Release notes
    if (!releaseNotes.isEmpty()) {
        QLabel* notesHeaderLabel = new QLabel(tr("What's new:"));
        layout->addWidget(notesHeaderLabel);
        
        QTextEdit* notesEdit = new QTextEdit();
        notesEdit->setReadOnly(true);
        notesEdit->setMarkdown(releaseNotes);
        notesEdit->setMaximumHeight(150);
        layout->addWidget(notesEdit);
    }
    
    // Progress bar (hidden initially)
    QProgressBar* progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setTextVisible(true);
    progressBar->setFormat(tr("Downloading... %p%"));
    layout->addWidget(progressBar);
    
    // Status label
    QLabel* statusLabel = new QLabel();
    statusLabel->setObjectName("subtitleLabel");
    layout->addWidget(statusLabel);
    
    layout->addStretch();
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    QPushButton* laterBtn = new QPushButton(tr("Remind Me Later"));
    laterBtn->setObjectName("secondaryButton");
    
    QPushButton* downloadBtn = new QPushButton(tr("Download && Install"));
    downloadBtn->setObjectName("successButton");
    
    buttonLayout->addWidget(laterBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(downloadBtn);
    layout->addLayout(buttonLayout);
    
    // Connect buttons
    connect(laterBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    connect(downloadBtn, &QPushButton::clicked, [&]() {
        downloadBtn->setEnabled(false);
        laterBtn->setText(tr("Cancel"));
        progressBar->setVisible(true);
        statusLabel->setText(tr("Starting download..."));
        
        // Connect progress updates for this dialog
        connect(updateManager.get(), &UpdateManager::downloadProgressChanged, 
                progressBar, &QProgressBar::setValue);
        
        connect(updateManager.get(), &UpdateManager::stateChanged,
                [statusLabel](UpdateManager::UpdateState state) {
                    switch (state) {
                    case UpdateManager::UpdateState::Downloading:
                        statusLabel->setText(tr("Downloading update..."));
                        break;
                    case UpdateManager::UpdateState::Extracting:
                        statusLabel->setText(tr("Extracting update..."));
                        break;
                    case UpdateManager::UpdateState::ReadyToInstall:
                        statusLabel->setText(tr("Ready to install!"));
                        break;
                    case UpdateManager::UpdateState::Error:
                        statusLabel->setText(tr("Error occurred"));
                        statusLabel->setObjectName("errorLabel");
                        statusLabel->style()->unpolish(statusLabel);
                        statusLabel->style()->polish(statusLabel);
                        break;
                    default:
                        break;
                    }
                });
        
        updateManager->downloadUpdate();
    });
    
    // Connect update ready signal
    connect(updateManager.get(), &UpdateManager::updateReady, &dialog, [&dialog, this]() {
        dialog.accept();
        onUpdateReady();
    });
    
    // Connect error signal
    connect(updateManager.get(), &UpdateManager::errorOccurred, &dialog, 
            [&dialog, statusLabel, downloadBtn, laterBtn](const QString& error) {
        statusLabel->setText(tr("Error: %1").arg(error));
        statusLabel->setObjectName("errorLabel");
        statusLabel->style()->unpolish(statusLabel);
        statusLabel->style()->polish(statusLabel);
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
    logActivity("✅ Update downloaded and ready to install");
    
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        tr("Install Update"),
        tr("The update has been downloaded and is ready to install.\n\n"
           "The application will close and restart automatically.\n\n"
           "Do you want to install the update now?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    
    if (reply == QMessageBox::Yes) {
        logActivity("🔄 Installing update and restarting...");
        
        // Save settings before update
        saveSettings();
        
        // Stop services gracefully
        stopServices();
        
        // Execute the update script (this will close the app)
        if (updateManager) {
            // Access private method through a workaround - call applyUpdate which leads to ready state
            // Actually we need to call executeUpdateScript, let's make it public or use a signal
            QMetaObject::invokeMethod(updateManager.get(), "executeUpdateScript", Qt::DirectConnection);
        }
    }
}

void MainWindow::onUpdateError(const QString& error)
{
    logActivity(QString("❌ Update error: %1").arg(error));
    statusBar()->showMessage(tr("Update error: %1").arg(error), 5000);
}

void MainWindow::showUpdateDialog()
{
    if (updateManager && updateManager->isUpdateAvailable()) {
        const auto& info = updateManager->updateInfo();
        onUpdateAvailable(info.version, info.releaseNotes);
    } else {
        checkForUpdates();
    }
}

void MainWindow::saveSettings()
{
    if (!config) return;
    
    // Save config to file
    config->save();
    
    // Window geometry - use QSettings for window-specific state
    QSettings windowSettings("RatsSearch", "RatsSearch");
    windowSettings.setValue("window/geometry", saveGeometry());
    windowSettings.setValue("window/state", saveState());
    windowSettings.sync();
    
    qInfo() << "Settings saved";
}
