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
#include "activitywidget.h"

// New API layer
#include "api/ratsapi.h"
#include "api/configmanager.h"
#include "api/apiserver.h"
#include "api/updatemanager.h"
#include "api/translationmanager.h"
#include "api/migrationmanager.h"

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
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QLabel>
#include <QTabWidget>
#include <QSplitter>
#include <QTextEdit>
#include <QGroupBox>
#include <QProgressBar>
#include <QCheckBox>
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
#include <QFileDialog>
#include <QStandardPaths>

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
    
    // Check if user has accepted the agreement
    if (!config->agreementAccepted()) {
        if (!showAgreementDialog()) {
            // User declined - schedule application exit
            QTimer::singleShot(0, qApp, &QApplication::quit);
            return;
        }
    }
    
    // Apply config overrides from command line args
    if (p2pPort > 0) config->setP2pPort(p2pPort);
    if (dhtPort > 0) config->setDhtPort(dhtPort);
    
    loadSettings();
    
    setWindowTitle(tr("Rats Search %1 - BitTorrent P2P Search Engine").arg(RATSSEARCH_VERSION_STRING));
    resize(1400, 900);
    
    // Set application icon
    setWindowIcon(QIcon(":/images/icon.png"));
    
    // Enable drag & drop for .torrent files
    setAcceptDrops(true);
    
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
    p2pNetwork = std::make_unique<P2PNetwork>(config->p2pPort(), config->dhtPort(), dataDirectory_, config->p2pConnections());
    p2pNetwork->setClientVersion(RATSSEARCH_VERSION_STRING);
    torrentSpider = std::make_unique<TorrentSpider>(torrentDatabase.get(), p2pNetwork.get());
    api = std::make_unique<RatsAPI>(this);
    updateManager = std::make_unique<UpdateManager>(this);
    migrationManager = std::make_unique<MigrationManager>(dataDirectory_, this);
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
    
    searchTabLayout->addWidget(resultsTableView);
    tabWidget->addTab(searchTab, tr("Search Results"));
    
    // Top Torrents tab
    topTorrentsWidget = new TopTorrentsWidget(this);
    tabWidget->addTab(topTorrentsWidget, tr("🔥 Top"));
    
    // Feed tab
    feedWidget = new FeedWidget(this);
    tabWidget->addTab(feedWidget, tr("📰 Feed"));
    
    // Downloads tab
    downloadsWidget = new DownloadsWidget(this);
    tabWidget->addTab(downloadsWidget, tr("📥 Downloads"));
    
    // Activity tab
    activityWidget = new ActivityWidget(this);
    tabWidget->addTab(activityWidget, tr("⚡ Activity"));
    
    mainSplitter->addWidget(tabWidget);
    
    // Right side - Details panel
    detailsPanel = new TorrentDetailsPanel(this);
    detailsPanel->setMinimumWidth(280);
    detailsPanel->setMaximumWidth(380);
    detailsPanel->setMinimumHeight(150);  // Allow vertical shrinking with scroll
    detailsPanel->hide();  // Hidden by default
    
    mainSplitter->addWidget(detailsPanel);
    mainSplitter->setSizes({900, 350});
    
    // Add horizontal splitter to vertical splitter
    verticalSplitter->addWidget(mainSplitter);
    
    // Bottom panel - Files widget (like qBittorrent)
    filesWidget = new TorrentFilesWidget(this);
    filesWidget->setMinimumHeight(120);
    // No maximum height limit - allow user to expand freely via splitter
    filesWidget->hide();  // Hidden by default until a torrent is selected
    verticalSplitter->addWidget(filesWidget);
    
    mainLayout->addWidget(verticalSplitter, 1);
}

void MainWindow::setupMenuBar()
{
    // File menu
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    
    // Add Torrent - imports .torrent file to search index
    QAction *addTorrentAction = fileMenu->addAction(tr("📥 &Add Torrent..."));
    addTorrentAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    addTorrentAction->setToolTip(tr("Add a .torrent file to the search index"));
    connect(addTorrentAction, &QAction::triggered, this, &MainWindow::addTorrentFile);
    
    // Create Torrent - creates torrent from file/folder and starts seeding
    QAction *createTorrentAction = fileMenu->addAction(tr("🔨 &Create Torrent..."));
    createTorrentAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    createTorrentAction->setToolTip(tr("Create a torrent from a file or folder and start seeding"));
    connect(createTorrentAction, &QAction::triggered, this, &MainWindow::createTorrent);
    
    fileMenu->addSeparator();
    
    QAction *settingsAction = fileMenu->addAction(tr("&Settings"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    
    fileMenu->addSeparator();
    
    QAction *quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QMainWindow::close);
    
    // Help menu
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    
    QAction *checkUpdateAction = helpMenu->addAction(tr("Check for &Updates..."));
    connect(checkUpdateAction, &QAction::triggered, this, &MainWindow::checkForUpdates);
    
    QAction *changelogAction = helpMenu->addAction(tr("📋 &Changelog"));
    connect(changelogAction, &QAction::triggered, this, &MainWindow::showChangelog);
    
    helpMenu->addSeparator();
    
    QAction *aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}


void MainWindow::setupStatusBar()
{
    // P2P status with colored indicator (will be updated via updateP2PIndicator)
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
    
    // Setup timer for periodic network status updates (DHT nodes, etc.)
    statusUpdateTimer_ = new QTimer(this);
    connect(statusUpdateTimer_, &QTimer::timeout, this, &MainWindow::updateNetworkStatus);
    statusUpdateTimer_->start(30000);  // Update every 30 seconds, only dht nodes so not so pressing
}

void MainWindow::connectSignals()
{
    connectSearchSignals();
    connectTabSignals();
    connectDetailsSignals();
    connectP2PSignals();
    
    // ConfigManager signals - for immediate settings application
    if (config) {
        connect(config.get(), &ConfigManager::darkModeChanged, this, &MainWindow::onDarkModeChanged);
        connect(config.get(), &ConfigManager::languageChanged, this, &MainWindow::onLanguageChanged);
    }
}

void MainWindow::connectSearchSignals()
{
    connect(searchButton, &QPushButton::clicked, this, &MainWindow::onSearchButtonClicked);
    connect(searchLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSearchButtonClicked);
    connect(searchLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(sortComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onSortOrderChanged);
    
    // Table view signals (search results tab)
    connect(resultsTableView->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex &current, const QModelIndex &) {
                onTorrentSelected(current);
            });
    connect(resultsTableView, &QTableView::doubleClicked, this, &MainWindow::onTorrentDoubleClicked);
    connect(resultsTableView, &QTableView::customContextMenuRequested,
            this, &MainWindow::showTorrentContextMenu);
}

void MainWindow::connectTabSignals()
{
    connect(tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    
    // All tab widgets share the same selected / double-clicked behaviour
    connect(topTorrentsWidget, &TopTorrentsWidget::torrentSelected,
            this, &MainWindow::showTorrentDetails);
    connect(topTorrentsWidget, &TopTorrentsWidget::torrentDoubleClicked,
            this, &MainWindow::openMagnetLink);
    
    connect(feedWidget, &FeedWidget::torrentSelected,
            this, &MainWindow::showTorrentDetails);
    connect(feedWidget, &FeedWidget::torrentDoubleClicked,
            this, &MainWindow::openMagnetLink);
    
    connect(activityWidget, &ActivityWidget::torrentSelected,
            this, &MainWindow::showTorrentDetails);
    connect(activityWidget, &ActivityWidget::torrentDoubleClicked,
            this, &MainWindow::openMagnetLink);
}

void MainWindow::connectDetailsSignals()
{
    connect(detailsPanel, &TorrentDetailsPanel::closeRequested,
            this, &MainWindow::onDetailsPanelCloseRequested);
    connect(detailsPanel, &TorrentDetailsPanel::downloadRequested,
            this, &MainWindow::onDownloadRequested);
    connect(detailsPanel, &TorrentDetailsPanel::goToDownloadsRequested,
            this, [this]() {
                tabWidget->setCurrentWidget(downloadsWidget);
            });
    connect(detailsPanel, &TorrentDetailsPanel::downloadCancelRequested,
            this, [this](const QString& hash) {
                if (torrentClient) {
                    torrentClient->stopTorrent(hash);
                    statusBar()->showMessage(tr("Download cancelled"), 2000);
                }
            });
}

void MainWindow::connectP2PSignals()
{
    // P2P Network
    connect(p2pNetwork.get(), &P2PNetwork::statusChanged, this, &MainWindow::onP2PStatusChanged);
    connect(p2pNetwork.get(), &P2PNetwork::peerCountChanged, this, &MainWindow::onPeerCountChanged);
    
    connect(p2pNetwork.get(), &P2PNetwork::peerInfoReceived, this, [this](const QString&, const PeerInfo&) {
        cachedRemoteTorrentCount_ = p2pNetwork->getRemoteTorrentsCount();
        updateStatusBar();
    });
    connect(p2pNetwork.get(), &P2PNetwork::peerDisconnected, this, [this](const QString&) {
        cachedRemoteTorrentCount_ = p2pNetwork->getRemoteTorrentsCount();
        updateStatusBar();
    });
    
    // Update max peers at runtime when config changes
    connect(config.get(), &ConfigManager::p2pConnectionsChanged, this, [this](int connections) {
        if (p2pNetwork) {
            p2pNetwork->setMaxPeers(connections);
        }
    });
    
    // Spider
    connect(torrentSpider.get(), &TorrentSpider::statusChanged, this, &MainWindow::onSpiderStatusChanged);
    connect(torrentSpider.get(), &TorrentSpider::torrentIndexed, this, &MainWindow::onTorrentIndexed);
    
    // RatsAPI remote results
    if (api) {
        connect(api.get(), &RatsAPI::torrentIndexed, this, &MainWindow::onTorrentIndexed);
        
        connect(api.get(), &RatsAPI::replicationProgress, this,
            [this](int replicated, qint64 /*total*/) {
                cachedTorrentCount_ += replicated;
                updateStatusBar();
            });
        
        connect(api.get(), &RatsAPI::remoteFileSearchResults, this,
            [this](const QString& searchId, const QJsonArray& torrents) {
                if (searchId.isEmpty() || currentSearchQuery_.isEmpty()) {
                    return;
                }
                for (const QJsonValue& val : torrents) {
                    TorrentInfo info = TorrentInfo::fromJson(val.toObject());
                    info.isFileMatch = true;  // Always file-match in this handler
                    searchResultModel->addFileResult(info);
                }
            });
        
        connect(api.get(), &RatsAPI::remoteTorrentReceived, this,
            [this](const QString& hash, const QJsonObject& torrentData) {
                if (filesWidget && !hash.isEmpty()) {
                    QJsonArray files = torrentData["filesList"].toArray();
                    if (!files.isEmpty()) {
                        QString name = torrentData["name"].toString();
                        filesWidget->setFiles(hash, name, files);
                        filesWidget->show();
                        verticalSplitter->setSizes({600, 200});
                        qInfo() << "Remote torrent files received:" << hash.left(16) << "with" << files.size() << "files";
                    }
                }
            });
    }
}

void MainWindow::startServices()
{
    if (servicesStarted_) {
        return;
    }
    
    QElapsedTimer timer;
    timer.start();
    
    // Initialize database (this starts Manticore - the slowest part)
    qint64 dbStart = timer.elapsed();
    if (!torrentDatabase->initialize()) {
        QMessageBox::critical(this, "Error", "Failed to initialize database!");
        return;
    }
    qInfo() << "Database initialize took:" << (timer.elapsed() - dbStart) << "ms";
    
    // Start P2P network
    qint64 p2pStart = timer.elapsed();
    if (!p2pNetwork->start()) {
        QMessageBox::warning(this, "Warning", "Failed to start P2P network. Some features may be limited.");
    } else {
        qInfo() << "P2P network start took:" << (timer.elapsed() - p2pStart) << "ms";
        
        // Initialize TorrentClient after P2P is running (requires RatsClient)
        if (torrentClient && !torrentClient->isReady()) {
            qint64 tcStart = timer.elapsed();
            if (torrentClient->initialize(p2pNetwork.get(), torrentDatabase.get(), dataDirectory_)) {
                qInfo() << "TorrentClient initialize took:" << (timer.elapsed() - tcStart) << "ms";
            } else {
                qWarning() << "Failed to initialize TorrentClient";
            }
        }

        // Initial DHT node count
        updateNetworkStatus();
    }
    
    // Apply spider walk interval from config before starting
    if (config) {
        torrentSpider->setWalkInterval(config->spiderWalkInterval());
    }
    
    // Start torrent spider
    qint64 spiderStart = timer.elapsed();
    if (!torrentSpider->start()) {
        QMessageBox::warning(this, "Warning", "Failed to start torrent spider. Automatic indexing disabled.");
    } else {
        qInfo() << "Spider start took:" << (timer.elapsed() - spiderStart) << "ms";
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
    
    // Stop migrations first (saves state for resume on next startup)
    if (migrationManager && migrationManager->isRunning()) {
        qInfo() << "Stopping migrations for graceful shutdown...";
        migrationManager->requestStop();
    }
    
    // Save torrent session if not already saved (safety net)
    if (torrentClient && torrentClient->count() > 0) {
        QString sessionFile = dataDirectory_ + "/torrents_session.json";
        torrentClient->saveSession(sessionFile);
    }
    
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
    
    // Connect signals first (before starting services)
    connectSignals();
    
    // Start heavy services (database, P2P, spider) - MUST happen before API init
    // Database must be initialized before RatsAPI (FeedManager needs it)
    qint64 servicesStart = timer.elapsed();
    startServices();
    qInfo() << "startServices took:" << (timer.elapsed() - servicesStart) << "ms";
    
    // Initialize RatsAPI with all dependencies (after database is ready)
    // RatsAPI::initialize() automatically sets up P2P message handlers
    // All P2P API logic is centralized in RatsAPI (like legacy api.js)
    qint64 apiStart = timer.elapsed();
    api->initialize(torrentDatabase.get(), p2pNetwork.get(), torrentClient.get(), config.get());
    qInfo() << "RatsAPI initialize took:" << (timer.elapsed() - apiStart) << "ms";
    
    // Connect RatsAPI remote search results to UI
    connect(api.get(), &RatsAPI::remoteSearchResults,
            this, [this](const QString& /*searchId*/, const QJsonArray& torrents) {
        for (const QJsonValue& val : torrents) {
            TorrentInfo info = TorrentInfo::fromJson(val.toObject());
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
    
    if (detailsPanel) {
        detailsPanel->setApi(api.get());
        detailsPanel->setTorrentClient(torrentClient.get());
    }
    
    // Connect TorrentClient signals to details panel for download status updates
    if (torrentClient) {
        connect(torrentClient.get(), &TorrentClient::progressUpdated, this,
            [this](const QString& infoHash, const QJsonObject& progress) {
                // Update details panel if it's showing this torrent
                if (detailsPanel && detailsPanel->currentHash() == infoHash) {
                    qint64 downloaded = progress["downloaded"].toVariant().toLongLong();
                    qint64 total = progress["size"].toVariant().toLongLong();
                    double progressVal = progress["progress"].toDouble();
                    int speed = static_cast<int>(progress["downloadSpeed"].toDouble());
                    detailsPanel->setDownloadProgress(progressVal, downloaded, total, speed);
                }
            });
        
        connect(torrentClient.get(), &TorrentClient::downloadCompleted, this,
            [this](const QString& infoHash) {
                // Update details panel if it's showing this torrent
                if (detailsPanel && detailsPanel->currentHash() == infoHash) {
                    detailsPanel->setDownloadCompleted();
                }
            });
        
        connect(torrentClient.get(), &TorrentClient::torrentRemoved, this,
            [this](const QString& infoHash) {
                // Reset details panel if it's showing this torrent
                if (detailsPanel && detailsPanel->currentHash() == infoHash) {
                    detailsPanel->resetDownloadState();
                }
                
                // Save session immediately after torrent removal to prevent restoration on restart
                QString sessionFile = dataDirectory_ + "/torrents_session.json";
                torrentClient->saveSession(sessionFile);
            });
    }
    
    // Start REST/WebSocket API server if enabled
    if (config->restApiEnabled()) {
        apiServer = std::make_unique<ApiServer>(api.get());
        apiServer->start(config->httpPort());
    }
    
    // Initialize and run migrations after database is ready
    if (migrationManager && torrentDatabase) {
        qint64 migrationStart = timer.elapsed();
        migrationManager->initialize(torrentDatabase.get(), config.get());
        
        // Run synchronous (blocking) migrations - these MUST complete before app can continue
        // If sync migrations fail, the app cannot start properly
        if (!migrationManager->runSyncMigrations()) {
            QMessageBox::critical(this, tr("Migration Error"),
                tr("Required database migration failed. The application may not work correctly."));
        }
        
        // Connect migration progress signal for UI feedback
        connect(migrationManager.get(), &MigrationManager::migrationProgress,
                this, [this](const QString& migrationId, qint64 current, qint64 total) {
                    statusBar()->showMessage(
                        tr("Migration %1: %2/%3").arg(migrationId).arg(current).arg(total), 2000);
                });
        
        connect(migrationManager.get(), &MigrationManager::migrationCompleted,
                this, [this](const QString& migrationId) {
                    statusBar()->showMessage(tr("Migration completed: %1").arg(migrationId), 3000);
                });
        
        connect(migrationManager.get(), &MigrationManager::allMigrationsCompleted,
                this, [this]() {
                    qInfo() << "All migrations completed successfully";
                });
        
        // Start asynchronous (background) migrations - these run in background
        // and can be interrupted/resumed
        migrationManager->startAsyncMigrations();
        
        qInfo() << "Migration setup took:" << (timer.elapsed() - migrationStart) << "ms";
    }
    
    // Load initial torrent count for statusbar and update P2P client info
    if (api) {
        api->getStatistics([this](const ApiResponse& response) {
            if (response.success) {
                QJsonObject stats = response.data.toObject();
                cachedTorrentCount_ = stats["torrents"].toVariant().toLongLong();
                updateStatusBar();
                
                // Update P2P network with our database stats
                if (p2pNetwork) {
                    qint64 torrents = stats["torrents"].toVariant().toLongLong();
                    qint64 files = stats["files"].toVariant().toLongLong();
                    qint64 size = stats["size"].toVariant().toLongLong();
                    p2pNetwork->updateOurStats(torrents, files, size);
                }
            }
        });
    }
    
    // Restore previous download session (loads resume data to continue from saved progress)
    if (torrentClient && torrentClient->isReady()) {
        QString sessionFile = dataDirectory_ + "/torrents_session.json";
        QFile sessionFileCheck(sessionFile);
        if (sessionFileCheck.exists()) {
            qInfo() << "Loading previous torrent session from" << sessionFile;
            int restored = torrentClient->loadSession(sessionFile);
            if (restored > 0) {
                qInfo() << "Restored" << restored << "torrents from session";
            }
        }
    }

    // Initialize new tab widgets with API
    if (topTorrentsWidget) {
        topTorrentsWidget->setApi(api.get());
        // Connect remote top torrents signal
        connect(api.get(), &RatsAPI::remoteTopTorrents,
                topTorrentsWidget, &TopTorrentsWidget::handleRemoteTopTorrents);
        
        // Invalidate top torrents cache when new torrents are indexed
        // This ensures fresh data when user switches to the Top tab
        connect(api.get(), &RatsAPI::torrentIndexed,
                topTorrentsWidget, [this](const QString&, const QString&) {
                    topTorrentsWidget->invalidateCache();
                });
    }
    
    if (activityWidget) {
        activityWidget->setApi(api.get());
        // Connect top button to switch to Top tab
        connect(activityWidget, &ActivityWidget::navigateToTop,
                this, [this]() {
                    tabWidget->setCurrentWidget(topTorrentsWidget);
                });
    }
    
    qInfo() << "Total deferred initialization:" << timer.elapsed() << "ms";
    
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
    qInfo() << "Search started:" << query.left(50) << (query.length() > 50 ? "..." : "");
    statusBar()->showMessage("🔍 Searching...", 2000);
    
    // Switch to Search Results tab when searching
    tabWidget->setCurrentIndex(0);
    
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
    
    // Search torrents by name
    api->searchTorrents(query, options, [this](const ApiResponse& response) {
        if (!response.success) {
            statusBar()->showMessage(QString("❌ Torrent search failed: %1").arg(response.error), 3000);
            return;
        }
        
        QJsonArray torrents = response.data.toArray();
        QVector<TorrentInfo> results;
        results.reserve(torrents.size());
        for (const QJsonValue& val : torrents) {
            results.append(TorrentInfo::fromJson(val.toObject()));
        }
        
        searchResultModel->addResults(results);
        statusBar()->showMessage(QString("✅ Found %1 torrents").arg(results.size()), 3000);
    });
    
    // Also search files within torrents
    api->searchFiles(query, options, [this](const ApiResponse& response) {
        if (!response.success) {
            return;
        }
        
        QJsonArray torrents = response.data.toArray();
        QVector<TorrentInfo> results;
        results.reserve(torrents.size());
        for (const QJsonValue& val : torrents) {
            TorrentInfo info = TorrentInfo::fromJson(val.toObject());
            info.isFileMatch = true;
            results.append(info);
        }
        
        if (!results.isEmpty()) {
            searchResultModel->addFileResults(results);
            int total = searchResultModel->resultCount();
            statusBar()->showMessage(QString("✅ Found %1 total results (incl. file matches)").arg(total), 3000);
        }
    });
}

void MainWindow::updateStatusBar()
{
    if (cachedRemoteTorrentCount_ > 0) {
        torrentCountLabel->setText(tr("📦 Torrents: %1 + %2").arg(cachedTorrentCount_).arg(cachedRemoteTorrentCount_));
    } else {
        torrentCountLabel->setText(tr("📦 Torrents: %1").arg(cachedTorrentCount_));
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    qInfo() << "Close event received, services running:" << servicesStarted_;
    
    // Hide to tray instead of closing if enabled
    bool closeToTray = config ? config->trayOnClose() : false;
    if (closeToTray && trayIcon && trayIcon->isVisible()) {
        qInfo() << "Minimizing to system tray instead of closing";
        hide();
        if (!trayNotificationShown_) {
            trayIcon->showMessage("Rats Search", 
                "Application is still running in the system tray.",
                QSystemTrayIcon::Information, 2000);
            trayNotificationShown_ = true;
        }
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
    
    // Save torrent download session (preserves downloaded pieces via resume data)
    // Always save session - if empty, saveSession() will remove the session file
    if (torrentClient) {
        QString sessionFile = dataDirectory_ + "/torrents_session.json";
        qInfo() << "Closing: Saving torrent session to" << sessionFile;
        if (torrentClient->saveSession(sessionFile)) {
            qInfo() << "Closing: Torrent session saved";
        } else {
            qWarning() << "Closing: Failed to save torrent session";
        }
    }
    
    qInfo() << "Closing: Saving settings";
    saveSettings();
    stopServices();
    event->accept();
    qInfo() << "Closing: Done (accepted)";

    if (isHidden()) {
        qInfo() << "Closing: Previously hidden, quitting";
        QApplication::quit();
    }
}

void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    Q_UNUSED(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept drag if it contains URLs (files)
    if (event->mimeData()->hasUrls()) {
        // Check if any of the files are .torrent files
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile() && url.toLocalFile().endsWith(".torrent", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }
    
    QStringList torrentFiles;
    
    // Collect all .torrent files
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            QString filePath = url.toLocalFile();
            if (filePath.endsWith(".torrent", Qt::CaseInsensitive)) {
                torrentFiles.append(filePath);
            }
        }
    }
    
    if (torrentFiles.isEmpty()) {
        event->ignore();
        return;
    }
    
    event->acceptProposedAction();
    
    for (const QString& filePath : torrentFiles) {
        if (!api) {
            continue;
        }
        
        api->addTorrentFile(filePath, [this, filePath](const ApiResponse& response) {
            if (response.success) {
                QJsonObject data = response.data.toObject();
                QString name = data["name"].toString();
                bool alreadyExists = data["alreadyExists"].toBool();
                if (alreadyExists) {
                    statusBar()->showMessage(tr("Already indexed: %1").arg(name), 2000);
                } else {
                    statusBar()->showMessage(tr("Added: %1").arg(name), 2000);
                }
            } else {
                qWarning() << "Failed to add torrent file:" << filePath << "-" << response.error;
            }
        });
    }
    
    statusBar()->showMessage(tr("Processing %1 torrent file(s)...").arg(torrentFiles.size()), 3000);
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
        showTorrentDetails(torrent);
    }
}

void MainWindow::onTorrentDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    
    TorrentInfo torrent = searchResultModel->getTorrent(index.row());
    if (torrent.isValid()) {
        openMagnetLink(torrent);
    }
}

void MainWindow::onSortOrderChanged(int index)
{
    Q_UNUSED(index);
    if (!currentSearchQuery_.isEmpty()) {
        performSearch(currentSearchQuery_);
    }
}

void MainWindow::onTabChanged(int index)
{
    Q_UNUSED(index);
    
    // Get the currently active widget
    QWidget* currentWidget = tabWidget->currentWidget();
    if (!currentWidget) {
        return;
    }
    
    TorrentInfo selectedTorrent;
    
    // Check which tab is active and get its selected torrent
    if (currentWidget == resultsTableView->parentWidget()) {
        // Search Results tab
        QModelIndex idx = resultsTableView->currentIndex();
        if (idx.isValid()) {
            selectedTorrent = searchResultModel->getTorrent(idx.row());
        }
    } else if (currentWidget == topTorrentsWidget) {
        // Notify widget that tab became visible - triggers refresh if cache stale
        topTorrentsWidget->onTabBecameVisible();
        selectedTorrent = topTorrentsWidget->getSelectedTorrent();
    } else if (currentWidget == feedWidget) {
        selectedTorrent = feedWidget->getSelectedTorrent();
    } else if (currentWidget == activityWidget) {
        selectedTorrent = activityWidget->getSelectedTorrent();
    } else if (currentWidget == downloadsWidget) {
        // Downloads tab doesn't have torrent selection in the same way
        // Hide details panel when switching to downloads
        detailsPanel->hide();
        filesWidget->clear();
        filesWidget->hide();
        return;
    }
    
    // Update details panel with the selected torrent from new tab
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

void MainWindow::showTorrentDetails(const TorrentInfo& torrent)
{
    detailsPanel->setTorrent(torrent);
    detailsPanel->show();
    filesWidget->clear();
    
    if (api) {
        api->getTorrent(torrent.hash, true, torrent.sourcePeerId,
            [this, hash = torrent.hash, name = torrent.name](const ApiResponse& response) {
                if (response.success) {
                    QJsonObject data = response.data.toObject();
                    QJsonArray files = data["filesList"].toArray();
                    if (!files.isEmpty()) {
                        filesWidget->setFiles(hash, name, files);
                        filesWidget->show();
                        verticalSplitter->setSizes({600, 200});
                    } else {
                        filesWidget->clear();
                        filesWidget->hide();
                    }
                }
            });
    }
}

void MainWindow::openMagnetLink(const TorrentInfo& torrent)
{
    QDesktopServices::openUrl(QUrl(torrent.magnetLink()));
}

void MainWindow::onDownloadRequested(const QString &hash)
{
    qInfo() << "Download requested for torrent:" << hash.left(16);
    
    // Show popup menu to choose download location
    QMenu menu(this);
    menu.setStyleSheet(this->styleSheet());
    
    // Get default download path from config
    QString defaultPath = config ? config->downloadPath() : QString();
    
    // Option 1: Download to default directory
    QString defaultText = tr("📥 Download to default folder");
    if (!defaultPath.isEmpty()) {
        // Show shortened path in menu
        QString shortPath = defaultPath;
        if (shortPath.length() > 40) {
            shortPath = "..." + shortPath.right(37);
        }
        defaultText = tr("📥 Download to: %1").arg(shortPath);
    }
    QAction *defaultAction = menu.addAction(defaultText);
    defaultAction->setToolTip(defaultPath);
    
    // Option 2: Choose custom location
    QAction *customAction = menu.addAction(tr("📂 Choose download location..."));
    
    menu.addSeparator();
    
    // Option 3: Cancel
    QAction *cancelAction = menu.addAction(tr("❌ Cancel"));
    
    // Show menu at cursor position
    QAction *selectedAction = menu.exec(QCursor::pos());
    
    if (selectedAction == cancelAction || selectedAction == nullptr) {
        return;
    }
    
    QString downloadPath;
    
    if (selectedAction == defaultAction) {
        downloadPath = defaultPath;
    } else if (selectedAction == customAction) {
        // Show folder selection dialog
        QString startDir = defaultPath.isEmpty() ? 
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) : defaultPath;
        downloadPath = QFileDialog::getExistingDirectory(this, 
            tr("Select Download Location"), startDir);
        
        if (downloadPath.isEmpty()) {
            return;
        }
    }
    
    // Start the download
    if (api) {
        qInfo() << "Starting download:" << hash.left(16) << "to:" << downloadPath;
        api->downloadAdd(hash, downloadPath, [this, hash](const ApiResponse& response) {
            if (response.success) {
                qInfo() << "Download started successfully:" << hash.left(16);
                statusBar()->showMessage("⬇️ Download started", 2000);
                
                // Update details panel to show download in progress
                if (detailsPanel && detailsPanel->currentHash() == hash) {
                    detailsPanel->setDownloadProgress(0.0, 0, 0, 0);
                }
            } else {
                qWarning() << "Download failed:" << hash.left(16) << "-" << response.error;
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
        openMagnetLink(torrent);
    });
    
    QAction *copyHashAction = contextMenu.addAction(tr("Copy Info Hash"));
    connect(copyHashAction, &QAction::triggered, [this, torrent]() {
        QApplication::clipboard()->setText(torrent.hash);
        statusBar()->showMessage(tr("Hash copied to clipboard"), 2000);
    });
    
    QAction *copyMagnetAction = contextMenu.addAction(tr("Copy Magnet Link"));
    connect(copyMagnetAction, &QAction::triggered, [this, torrent]() {
        QApplication::clipboard()->setText(torrent.magnetLink());
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
    Q_UNUSED(status);
    updateP2PState();
}

void MainWindow::onPeerCountChanged(int count)
{
    peerCountLabel->setText(tr("👥 Peers: %1").arg(count));
    updateP2PState();
}

void MainWindow::updateP2PState()
{
    if (!p2pNetwork || !p2pNetwork->isRunning()) {
        p2pState_ = P2PState::NotStarted;
    } else if (p2pNetwork->getPeerCount() > 0) {
        p2pState_ = P2PState::Connected;
    } else {
        p2pState_ = P2PState::NoConnection;
    }
    updateP2PIndicator();
}

void MainWindow::onSpiderStatusChanged(const QString &status)
{
    spiderStatusLabel->setText(tr("🕷️ Spider: %1").arg(status));
}

void MainWindow::updateP2PIndicator()
{
    // Colored circle indicators using HTML
    // ● (U+25CF) - filled circle
    QString indicator;
    QString statusText;
    
    switch (p2pState_) {
    case P2PState::NotStarted:
        // Red indicator - P2P not started
        indicator = "<span style='color: #e74c3c; font-size: 14px;'>●</span>";
        statusText = tr("P2P: Not Started");
        break;
    case P2PState::NoConnection:
        // Yellow/Orange indicator - No peers
        indicator = "<span style='color: #f39c12; font-size: 14px;'>●</span>";
        statusText = tr("P2P: No Peers");
        break;
    case P2PState::Connected:
        // Green indicator - Connected to peers
        indicator = "<span style='color: #27ae60; font-size: 14px;'>●</span>";
        statusText = tr("P2P: Connected");
        break;
    }
    
    p2pStatusLabel->setText(QString("%1 %2").arg(indicator, statusText));
}

void MainWindow::updateNetworkStatus()
{
    // Update DHT node count periodically
    if (p2pNetwork && p2pNetwork->isRunning()) {
        size_t dhtNodes = p2pNetwork->getDhtNodeCount();
        bool dhtRunning = p2pNetwork->isDhtRunning();
        
        if (dhtRunning) {
            dhtNodeCountLabel->setText(tr("🌐 DHT: %1 nodes").arg(dhtNodes));
        } else {
            dhtNodeCountLabel->setText(tr("🌐 DHT: Offline"));
        }
    } else if (p2pState_ != P2PState::NotStarted) {
        dhtNodeCountLabel->setText(tr("🌐 DHT: Offline"));
    }
}

void MainWindow::onTorrentIndexed(const QString &infoHash, const QString &name)
{
    statusBar()->showMessage(QString("📥 Indexed: %1").arg(name), 2000);
    cachedTorrentCount_++;
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
    qInfo() << "Theme changed to:" << (enabled ? "dark" : "light");
    applyTheme(enabled);
}

void MainWindow::onLanguageChanged(const QString& languageCode)
{
    qInfo() << "Language changed to:" << languageCode;
    
    // Use TranslationManager to switch language at runtime
    auto& translationManager = TranslationManager::instance();
    translationManager.setLanguage(languageCode);
    // Note: Full UI retranslation would require recreating widgets or using Qt's retranslateUi pattern
    // For now, most static strings will update on next window open
}

void MainWindow::showSettings()
{
    if (!config) return;
    
    qInfo() << "Opening settings dialog";
    SettingsDialog dialog(config.get(), api.get(), dataDirectory_, this);
    dialog.setStyleSheet(this->styleSheet());
    
    if (dialog.exec() == QDialog::Accepted) {
        qInfo() << "Settings saved by user";
        // Show restart message only for settings that genuinely require restart
        if (dialog.needsRestart()) {
            QMessageBox::information(this, tr("Restart Required"), 
                tr("Some changes (network ports or data directory) will take effect after restarting the application."));
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
    
    // Header
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    QLabel* iconLabel = new QLabel("📋");
    iconLabel->setObjectName("aboutLogoLabel");
    headerLayout->addWidget(iconLabel);
    
    QLabel* titleLabel = new QLabel(tr("Changelog"));
    titleLabel->setObjectName("aboutTitleLabel");
    headerLayout->addWidget(titleLabel);
    
    headerLayout->addStretch();
    
    // Current version label
    QLabel* versionLabel = new QLabel(QString("v%1").arg(RATSSEARCH_VERSION_STRING));
    versionLabel->setObjectName("subtitleLabel");
    headerLayout->addWidget(versionLabel);
    
    layout->addLayout(headerLayout);
    
    // Subtitle
    QLabel* subtitleLabel = new QLabel(tr("Recent changes and updates to Rats Search"));
    subtitleLabel->setObjectName("hintLabel");
    layout->addWidget(subtitleLabel);
    
    // Changelog content
    QTextEdit* changelogText = new QTextEdit();
    changelogText->setReadOnly(true);
    changelogText->setMinimumHeight(350);
    
    // Load changelog from resources or file
    QString changelogContent;
    
    // Try to load from resources first
    QFile resourceFile(":/CHANGELOG.md");
    if (resourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        changelogContent = QString::fromUtf8(resourceFile.readAll());
        resourceFile.close();
    } else {
        // Try to load from application directory
        QString appPath = QApplication::applicationDirPath();
        QFile localFile(appPath + "/CHANGELOG.md");
        if (localFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            changelogContent = QString::fromUtf8(localFile.readAll());
            localFile.close();
        } else {
            // Fallback to source directory (for development)
            QFile devFile("CHANGELOG.md");
            if (devFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                changelogContent = QString::fromUtf8(devFile.readAll());
                devFile.close();
            }
        }
    }
    
    if (changelogContent.isEmpty()) {
        changelogContent = tr("# Changelog\n\nNo changelog available.");
    }
    
    changelogText->setMarkdown(changelogContent);
    layout->addWidget(changelogText, 1);
    
    // Buttons
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

void MainWindow::addTorrentFile()
{
    qInfo() << "Add torrent file dialog opened";
    
    // Open file dialog to select .torrent file
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Add Torrent File"),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        tr("Torrent Files (*.torrent);;All Files (*)")
    );
    
    if (filePath.isEmpty()) {
        return;  // User cancelled
    }
    
    if (!api) {
        QMessageBox::warning(this, tr("Error"), tr("API not initialized"));
        return;
    }
    
    api->addTorrentFile(filePath, [this, filePath](const ApiResponse& response) {
        if (response.success) {
            QJsonObject data = response.data.toObject();
            QString name = data["name"].toString();
            QString hash = data["hash"].toString();
            bool alreadyExists = data["alreadyExists"].toBool();
            
            qInfo() << "Torrent file added:" << QFileInfo(filePath).fileName() 
                    << "hash:" << hash.left(16) << (alreadyExists ? "(already existed)" : "(new)");
            
            if (alreadyExists) {
                statusBar()->showMessage(tr("Torrent already in index: %1").arg(name), 3000);
            } else {
                statusBar()->showMessage(tr("Added to index: %1").arg(name), 3000);
            }
            
            // Show notification
            if (trayIcon && trayIcon->isVisible()) {
                trayIcon->showMessage(
                    tr("Torrent Added"),
                    tr("%1 has been added to the search index").arg(name),
                    QSystemTrayIcon::Information,
                    3000
                );
            }
        } else {
            qWarning() << "Failed to add torrent file:" << filePath << "-" << response.error;
            QMessageBox::warning(this, tr("Error"), 
                tr("Failed to add torrent file:\n%1").arg(response.error));
        }
    });
}

void MainWindow::createTorrent()
{
    qInfo() << "Create torrent dialog opened";
    
    // First ask user to select file or folder
    QMenu menu(this);
    menu.setStyleSheet(this->styleSheet());
    
    QAction *fileAction = menu.addAction(tr("📄 Create from File..."));
    QAction *folderAction = menu.addAction(tr("📁 Create from Folder..."));
    menu.addSeparator();
    QAction *cancelAction = menu.addAction(tr("❌ Cancel"));
    
    QAction *selected = menu.exec(QCursor::pos());
    
    if (selected == cancelAction || selected == nullptr) {
        return;
    }
    
    QString path;
    
    if (selected == fileAction) {
        path = QFileDialog::getOpenFileName(
            this,
            tr("Select File to Create Torrent From"),
            QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
            tr("All Files (*)")
        );
    } else if (selected == folderAction) {
        path = QFileDialog::getExistingDirectory(
            this,
            tr("Select Folder to Create Torrent From"),
            QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
        );
    }
    
    if (path.isEmpty()) {
        return;  // User cancelled
    }
    
    // Create a simple dialog for torrent creation options
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Create Torrent"));
    dialog.setMinimumWidth(450);
    dialog.setStyleSheet(this->styleSheet());
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 20, 20, 20);
    
    // Source path
    QLabel *pathLabel = new QLabel(tr("Source: %1").arg(QFileInfo(path).fileName()));
    pathLabel->setWordWrap(true);
    pathLabel->setObjectName("subtitleLabel");
    layout->addWidget(pathLabel);
    
    // Trackers
    QLabel *trackersLabel = new QLabel(tr("Trackers (one per line, optional):"));
    layout->addWidget(trackersLabel);
    
    QTextEdit *trackersEdit = new QTextEdit();
    trackersEdit->setPlaceholderText("udp://tracker.example.com:6969/announce\nhttp://tracker2.example.com/announce");
    trackersEdit->setMaximumHeight(80);
    layout->addWidget(trackersEdit);
    
    // Comment
    QLabel *commentLabel = new QLabel(tr("Comment (optional):"));
    layout->addWidget(commentLabel);
    
    QLineEdit *commentEdit = new QLineEdit();
    commentEdit->setPlaceholderText(tr("Created with Rats Search"));
    layout->addWidget(commentEdit);
    
    // Start seeding checkbox
    QCheckBox *seedCheckBox = new QCheckBox(tr("Start seeding immediately"));
    seedCheckBox->setChecked(true);
    layout->addWidget(seedCheckBox);
    
    // Progress bar (hidden initially)
    QProgressBar *progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setFormat(tr("Hashing pieces... %p%"));
    layout->addWidget(progressBar);
    
    // Status label
    QLabel *statusLabel = new QLabel();
    statusLabel->setObjectName("hintLabel");
    layout->addWidget(statusLabel);
    
    layout->addStretch();
    
    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"));
    QPushButton *createBtn = new QPushButton(tr("🔨 Create Torrent"));
    createBtn->setObjectName("primaryButton");
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(createBtn);
    layout->addLayout(buttonLayout);
    
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    connect(createBtn, &QPushButton::clicked, [&]() {
        if (!api) {
            QMessageBox::warning(this, tr("Error"), tr("API not initialized"));
            return;
        }
        
        createBtn->setEnabled(false);
        cancelBtn->setEnabled(false);
        progressBar->setVisible(true);
        progressBar->setValue(0);
        statusLabel->setText(tr("Creating torrent..."));
        
        // Parse trackers
        QStringList trackers;
        QString trackersText = trackersEdit->toPlainText().trimmed();
        if (!trackersText.isEmpty()) {
            trackers = trackersText.split('\n', Qt::SkipEmptyParts);
            for (QString& t : trackers) {
                t = t.trimmed();
            }
            trackers.removeAll("");
        }
        
        QString comment = commentEdit->text().trimmed();
        if (comment.isEmpty()) {
            comment = "Created with Rats Search";
        }
        
        bool startSeeding = seedCheckBox->isChecked();
        
        // Progress callback
        auto progressCallback = [progressBar](int current, int total) {
            if (total > 0) {
                int percent = (current * 100) / total;
                QMetaObject::invokeMethod(progressBar, [progressBar, percent]() {
                    progressBar->setValue(percent);
                }, Qt::QueuedConnection);
            }
        };
        
        api->createTorrent(path, startSeeding, trackers, comment, progressCallback,
            [this, &dialog, statusLabel, createBtn, cancelBtn, startSeeding](const ApiResponse& response) {
                if (response.success) {
                    QJsonObject data = response.data.toObject();
                    QString name = data["name"].toString();
                    QString hash = data["hash"].toString();
                    bool alreadyExists = data["alreadyExists"].toBool();
                    
                    if (alreadyExists) {
                        statusLabel->setText(tr("Torrent already exists"));
                    } else {
                        statusLabel->setText(tr("✅ Torrent created successfully!"));
                    }
                    
                    // Show success message with torrent file path
                    QString torrentFile = data["torrentFile"].toString();
                    QString message = startSeeding 
                        ? tr("Torrent created and seeding:\n%1\n\nHash: %2").arg(name, hash)
                        : tr("Torrent created and indexed:\n%1\n\nHash: %2").arg(name, hash);
                    
                    if (!torrentFile.isEmpty()) {
                        message += tr("\n\nTorrent file saved to:\n%1").arg(torrentFile);
                    }
                    
                    QMessageBox::information(&dialog, tr("Torrent Created"), message);
                    
                    dialog.accept();
                } else {
                    statusLabel->setText(tr("❌ Failed: %1").arg(response.error));
                    
                    createBtn->setEnabled(true);
                    cancelBtn->setEnabled(true);
                    
                    QMessageBox::warning(&dialog, tr("Error"), 
                        tr("Failed to create torrent:\n%1").arg(response.error));
                }
            });
    });
    
    dialog.exec();
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
            if (trayIcon && !trayNotificationShown_) {
                trayIcon->showMessage("Rats Search", 
                    "Application minimized to tray. Click to restore.",
                    QSystemTrayIcon::Information, 2000);
                trayNotificationShown_ = true;
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
    
    statusBar()->showMessage("Checking for updates...", 3000);
    
    // Disconnect previous connections to avoid duplicates
    disconnect(updateManager.get(), &UpdateManager::noUpdateAvailable, nullptr, nullptr);
    disconnect(updateManager.get(), &UpdateManager::checkComplete, nullptr, nullptr);
    
    // Connect for this manual check
    connect(updateManager.get(), &UpdateManager::noUpdateAvailable, this, [this]() {
        QMessageBox::information(this, tr("No Updates Available"),
            tr("You are running the latest version of Rats Search (%1).")
                .arg(UpdateManager::currentVersion()));
    }, Qt::SingleShotConnection);
    
    updateManager->checkForUpdates();
}

void MainWindow::onUpdateAvailable(const QString& version, const QString& releaseNotes)
{
    qInfo() << "Update available:" << version << "current:" << UpdateManager::currentVersion();
    
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
    qInfo() << "Update downloaded and ready to install";
    
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        tr("Install Update"),
        tr("The update has been downloaded and is ready to install.\n\n"
           "The application will close and restart automatically.\n\n"
           "Do you want to install the update now?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    
    if (reply == QMessageBox::Yes) {
        qInfo() << "User accepted update installation, preparing to restart...";
        
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

bool MainWindow::showAgreementDialog()
{
    qInfo() << "Showing End User License Agreement dialog";
    
    QDialog dialog;
    dialog.setWindowTitle(tr("End User License Agreement"));
    dialog.setMinimumSize(700, 600);
    dialog.setModal(true);
    
    // Apply dark theme if configured
    if (config && config->darkMode()) {
        QString stylePath = ":/styles/styles/dark.qss";
        QFile styleFile(stylePath);
        if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            dialog.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
            styleFile.close();
        }
    }
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);
    
    // Header with logo
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    QLabel* logoLabel = new QLabel("🐀");
    logoLabel->setObjectName("aboutLogoLabel");
    headerLayout->addWidget(logoLabel);
    
    QLabel* titleLabel = new QLabel(tr("Rats Search - License Agreement"));
    titleLabel->setObjectName("aboutTitleLabel");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    layout->addLayout(headerLayout);
    
    // Subtitle
    QLabel* subtitleLabel = new QLabel(
        tr("Please read and accept the following End User License Agreement before using this software."));
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setObjectName("subtitleLabel");
    layout->addWidget(subtitleLabel);
    
    // Agreement text in scrollable area
    QTextEdit* agreementText = new QTextEdit();
    agreementText->setReadOnly(true);
    agreementText->setMinimumHeight(350);
    
    // Load agreement from resources or file
    QString agreementContent;
    
    // Try to load from resources first
    QFile resourceFile(":/AGREEMENT.md");
    if (resourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        agreementContent = QString::fromUtf8(resourceFile.readAll());
        resourceFile.close();
    } else {
        // Try to load from application directory
        QString appPath = QApplication::applicationDirPath();
        QFile localFile(appPath + "/AGREEMENT.md");
        if (localFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            agreementContent = QString::fromUtf8(localFile.readAll());
            localFile.close();
        } else {
            // Fallback to source directory (for development)
            QFile devFile("AGREEMENT.md");
            if (devFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                agreementContent = QString::fromUtf8(devFile.readAll());
                devFile.close();
            }
        }
    }
    
    if (agreementContent.isEmpty()) {
        // Embedded fallback agreement if file not found
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
    
    // Checkbox to confirm reading
    QCheckBox* readCheckbox = new QCheckBox(
        tr("I have read and understood the End User License Agreement"));
    layout->addWidget(readCheckbox);
    
    // Warning label
    QLabel* warningLabel = new QLabel(
        tr("⚠️ By clicking 'I Accept', you acknowledge that you have read, understood, and agree to be bound by all terms and conditions of this Agreement. You accept full responsibility for your use of this Software."));
    warningLabel->setWordWrap(true);
    warningLabel->setObjectName("hintLabel");
    layout->addWidget(warningLabel);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    QPushButton* declineBtn = new QPushButton(tr("Decline && Exit"));
    declineBtn->setObjectName("dangerButton");
    declineBtn->setMinimumWidth(140);
    declineBtn->setCursor(Qt::PointingHandCursor);
    
    QPushButton* acceptBtn = new QPushButton(tr("I Accept"));
    acceptBtn->setObjectName("successButton");
    acceptBtn->setMinimumWidth(140);
    acceptBtn->setEnabled(false);  // Disabled until checkbox is checked
    acceptBtn->setCursor(Qt::PointingHandCursor);
    
    buttonLayout->addWidget(declineBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(acceptBtn);
    layout->addLayout(buttonLayout);
    
    // Connect checkbox to enable accept button
    connect(readCheckbox, &QCheckBox::toggled, acceptBtn, &QPushButton::setEnabled);
    
    // Connect buttons
    bool accepted = false;
    
    connect(declineBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(acceptBtn, &QPushButton::clicked, [&dialog, &accepted]() {
        accepted = true;
        dialog.accept();
    });
    
    dialog.exec();
    
    if (accepted) {
        qInfo() << "User accepted the End User License Agreement";
        config->setAgreementAccepted(true);
        return true;
    } else {
        qInfo() << "User declined the End User License Agreement";
        return false;
    }
}
