#include "mainwindow.h"
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

// New API layer
#include "api/ratsapi.h"
#include "api/configmanager.h"
#include "api/apiserver.h"
#include "api/updatemanager.h"
#include "api/translationmanager.h"
#include "api/downloadmanager.h"

#include <QMenuBar>
#include <QToolBar>
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
#include <QFrame>
#include <QScrollArea>
#include <QSystemTrayIcon>
#include <QSettings>
#include <QDialog>
#include <QFormLayout>
#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QTimer>
#include <QElapsedTimer>

MainWindow::MainWindow(int p2pPort, int dhtPort, const QString& dataDirectory, QWidget *parent)
    : QMainWindow(parent)
    , ui(nullptr)
    , dataDirectory_(dataDirectory)
    , servicesStarted_(false)
    , currentSortField_("seeders")
    , currentSortDesc_(true)
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
    
    setWindowTitle(tr("Rats Search - BitTorrent P2P Search Engine"));
    resize(1400, 900);
    
    // Set application icon
    setWindowIcon(QIcon(":/images/icon.png"));
    
    // UI setup (show window fast)
    qint64 uiStart = startupTimer.elapsed();
    applyDarkTheme();
    setupUi();
    setupMenuBar();
    setupToolBar();
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

void MainWindow::applyDarkTheme()
{
    // Load stylesheet from resources or apply inline
    QString styleSheet = R"(
        QMainWindow {
            background-color: #1e1e1e;
            color: #ffffff;
        }
        
        QWidget {
            background-color: #1e1e1e;
            color: #ffffff;
        }
        
        QLineEdit {
            background-color: #2d2d2d;
            border: 2px solid #3c3f41;
            border-radius: 8px;
            padding: 10px 16px;
            color: #ffffff;
            font-size: 14px;
            selection-background-color: #4a9eff;
        }
        
        QLineEdit:focus {
            border: 2px solid #4a9eff;
            background-color: #353535;
        }
        
        QLineEdit::placeholder {
            color: #666666;
        }
        
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #4a9eff, stop:1 #6eb5ff);
            color: #ffffff;
            border: none;
            border-radius: 8px;
            padding: 10px 24px;
            font-size: 14px;
            font-weight: bold;
        }
        
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #6eb5ff, stop:1 #8fcfff);
        }
        
        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #2e6fb8, stop:1 #4a9eff);
        }
        
        QPushButton:disabled {
            background-color: #3c3f41;
            color: #666666;
        }
        
        QTableView {
            background-color: #1e1e1e;
            alternate-background-color: #252526;
            color: #ffffff;
            gridline-color: #2d2d2d;
            border: 1px solid #3c3f41;
            border-radius: 8px;
            selection-background-color: #3d6a99;
        }
        
        QTableView::item {
            padding: 8px;
            border-bottom: 1px solid #2d2d2d;
        }
        
        QTableView::item:selected {
            background-color: #3d6a99;
        }
        
        QTableView::item:hover {
            background-color: #2d3748;
        }
        
        QHeaderView::section {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #3c3f41, stop:1 #2d2d2d);
            color: #ffffff;
            padding: 10px 8px;
            border: none;
            border-right: 1px solid #2d2d2d;
            border-bottom: 2px solid #4a9eff;
            font-weight: bold;
            font-size: 12px;
        }
        
        QHeaderView::section:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #4c4f51, stop:1 #3c3f41);
        }
        
        QTabWidget::pane {
            border: 1px solid #3c3f41;
            border-radius: 8px;
            background-color: #1e1e1e;
            top: -1px;
        }
        
        QTabBar::tab {
            background-color: #2d2d2d;
            color: #888888;
            padding: 10px 20px;
            margin-right: 2px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            font-weight: bold;
        }
        
        QTabBar::tab:selected {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #4a9eff, stop:1 #3d8ce8);
            color: #ffffff;
        }
        
        QTabBar::tab:hover:!selected {
            background-color: #3c3f41;
            color: #ffffff;
        }
        
        QStatusBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2d2d2d, stop:1 #252526);
            color: #888888;
            border-top: 1px solid #3c3f41;
        }
        
        QStatusBar QLabel {
            color: #888888;
            padding: 0 12px;
        }
        
        QMenuBar {
            background-color: #2d2d2d;
            color: #ffffff;
            border-bottom: 1px solid #3c3f41;
        }
        
        QMenuBar::item {
            padding: 6px 12px;
        }
        
        QMenuBar::item:selected {
            background-color: #4a9eff;
            border-radius: 4px;
        }
        
        QMenu {
            background-color: #2d2d2d;
            color: #ffffff;
            border: 1px solid #3c3f41;
            border-radius: 6px;
            padding: 4px;
        }
        
        QMenu::item {
            padding: 8px 24px;
            border-radius: 4px;
        }
        
        QMenu::item:selected {
            background-color: #4a9eff;
        }
        
        QMenu::separator {
            height: 1px;
            background-color: #3c3f41;
            margin: 4px 8px;
        }
        
        QToolBar {
            background-color: #2d2d2d;
            border: none;
            border-bottom: 1px solid #3c3f41;
            spacing: 8px;
            padding: 4px;
        }
        
        QToolBar QToolButton {
            background-color: transparent;
            border: none;
            border-radius: 4px;
            padding: 6px;
            color: #ffffff;
        }
        
        QToolBar QToolButton:hover {
            background-color: #3c3f41;
        }
        
        QComboBox {
            background-color: #2d2d2d;
            border: 1px solid #3c3f41;
            border-radius: 6px;
            padding: 8px 12px;
            color: #ffffff;
            min-width: 140px;
        }
        
        QComboBox:hover {
            border-color: #4a9eff;
        }
        
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 6px solid #888888;
            margin-right: 8px;
        }
        
        QComboBox QAbstractItemView {
            background-color: #2d2d2d;
            color: #ffffff;
            border: 1px solid #3c3f41;
            selection-background-color: #4a9eff;
        }
        
        QScrollBar:vertical {
            background-color: #1e1e1e;
            width: 12px;
            border-radius: 6px;
        }
        
        QScrollBar::handle:vertical {
            background-color: #3c3f41;
            border-radius: 6px;
            min-height: 30px;
        }
        
        QScrollBar::handle:vertical:hover {
            background-color: #4a9eff;
        }
        
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        
        QScrollBar:horizontal {
            background-color: #1e1e1e;
            height: 12px;
            border-radius: 6px;
        }
        
        QScrollBar::handle:horizontal {
            background-color: #3c3f41;
            border-radius: 6px;
            min-width: 30px;
        }
        
        QScrollBar::handle:horizontal:hover {
            background-color: #4a9eff;
        }
        
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        
        QTextEdit {
            background-color: #1e1e1e;
            color: #cccccc;
            border: 1px solid #3c3f41;
            border-radius: 8px;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 11px;
            padding: 8px;
        }
        
        QGroupBox {
            background-color: #252526;
            border: 1px solid #3c3f41;
            border-radius: 8px;
            margin-top: 12px;
            padding-top: 8px;
            font-weight: bold;
        }
        
        QGroupBox::title {
            color: #4a9eff;
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 8px;
        }
        
        QSplitter::handle {
            background-color: #3c3f41;
        }
        
        QSplitter::handle:horizontal {
            width: 2px;
        }
        
        QSplitter::handle:vertical {
            height: 2px;
        }
        
        QLabel {
            color: #ffffff;
        }
    )";
    
    setStyleSheet(styleSheet);
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
    searchSection->setStyleSheet("background-color: #252526; border-radius: 12px; padding: 8px;");
    QHBoxLayout *searchLayout = new QHBoxLayout(searchSection);
    searchLayout->setContentsMargins(12, 8, 12, 8);
    searchLayout->setSpacing(12);
    
    // Logo/Title
    QLabel *logoLabel = new QLabel("🐀");
    logoLabel->setStyleSheet("font-size: 28px; background: transparent;");
    searchLayout->addWidget(logoLabel);
    
    QLabel *titleLabel = new QLabel("Rats Search");
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #4a9eff; background: transparent;");
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
    
    // Main content with splitter
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
    emptyLabel->setStyleSheet("font-size: 16px; color: #666666; padding: 40px;");
    
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
    QLabel *p2pStatsLabel = new QLabel(tr("Connected peers: %1").arg(0) + "\n" + 
                                        tr("DHT nodes: %1").arg(0) + "\n" + 
                                        tr("Total data exchanged: %1 MB").arg(0));
    p2pStatsLabel->setStyleSheet("color: #cccccc;");
    p2pLayout->addWidget(p2pStatsLabel);
    
    // Database Stats
    QGroupBox *dbGroup = new QGroupBox(tr("Database"));
    QVBoxLayout *dbLayout = new QVBoxLayout(dbGroup);
    QLabel *dbStatsLabel = new QLabel(tr("Indexed torrents: %1").arg(0) + "\n" + 
                                       tr("Total files: %1").arg(0) + "\n" + 
                                       tr("Database size: %1 MB").arg(0));
    dbStatsLabel->setStyleSheet("color: #cccccc;");
    dbLayout->addWidget(dbStatsLabel);
    
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
    detailsPanel->setMinimumWidth(320);
    detailsPanel->setMaximumWidth(400);
    detailsPanel->hide();  // Hidden by default
    
    mainSplitter->addWidget(detailsPanel);
    mainSplitter->setSizes({900, 350});
    
    mainLayout->addWidget(mainSplitter, 1);
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

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar(tr("Main Toolbar"));
    toolBar->setObjectName("MainToolBar");
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(20, 20));
    
    // Add actions to toolbar
    QAction *refreshAction = toolBar->addAction(tr("Refresh"));
    connect(refreshAction, &QAction::triggered, [this]() {
        if (!currentSearchQuery_.isEmpty()) {
            performSearch(currentSearchQuery_);
        }
    });
    
    toolBar->addSeparator();
    
    QAction *clearAction = toolBar->addAction(tr("Clear"));
    connect(clearAction, &QAction::triggered, [this]() {
        searchResultModel->clearResults();
        detailsPanel->clear();
        detailsPanel->hide();
    });
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
    
    // Initialize new tab widgets with API
    if (topTorrentsWidget) {
        topTorrentsWidget->setApi(api.get());
        // Connect remote top torrents signal
        connect(api.get(), &RatsAPI::remoteTopTorrents,
                topTorrentsWidget, &TopTorrentsWidget::handleRemoteTopTorrents);
    }
    
    if (feedWidget) {
        feedWidget->setApi(api.get());
    }
    
    if (downloadsWidget) {
        downloadsWidget->setApi(api.get());
        DownloadManager* downloadManager = api->getDownloadManager();
        if (downloadManager) {
            downloadsWidget->setDownloadManager(downloadManager);
        }
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
    
    qInfo() << "Total deferred initialization:" << timer.elapsed() << "ms";
    logActivity("🚀 Rats Search started");
    
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
    
    // Use RatsAPI for searching
    QJsonObject options;
    options["limit"] = 100;
    options["safeSearch"] = false;
    options["orderBy"] = orderBy;
    options["orderDesc"] = orderDesc;
    
    api->searchTorrents(query, options, [this, query](const ApiResponse& response) {
        if (!response.success) {
            statusBar()->showMessage(QString("❌ Search failed: %1").arg(response.error), 3000);
            logActivity(QString("❌ Search failed: %1").arg(response.error));
            return;
        }
        
        // Convert JSON to TorrentInfo for model
        QJsonArray torrents = response.data.toArray();
        QVector<TorrentInfo> results;
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
            results.append(info);
        }
        
        searchResultModel->setResults(results);
        statusBar()->showMessage(QString("✅ Found %1 results for '%2'").arg(results.size()).arg(query), 3000);
        logActivity(QString("✅ Found %1 results").arg(results.size()));
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
}

void MainWindow::onPeerCountChanged(int count)
{
    peerCountLabel->setText(tr("Peers: %1").arg(count));
}

void MainWindow::onSpiderStatusChanged(const QString &status)
{
    spiderStatusLabel->setText(tr("Spider: %1").arg(status));
}

void MainWindow::onTorrentIndexed(const QString &infoHash, const QString &name)
{
    Q_UNUSED(infoHash);
    statusBar()->showMessage(QString("📥 Indexed: %1").arg(name), 2000);
    updateStatusBar();
}

void MainWindow::showSettings()
{
    if (!config) return;
    
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Settings"));
    dialog.setMinimumSize(550, 600);
    dialog.resize(550, 700);
    dialog.setStyleSheet(this->styleSheet());
    
    QVBoxLayout *dialogLayout = new QVBoxLayout(&dialog);
    dialogLayout->setSpacing(12);
    dialogLayout->setContentsMargins(16, 16, 16, 16);
    
    // Title (outside scroll area)
    QLabel *titleLabel = new QLabel(tr("Rats Search Settings"));
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #4a9eff;");
    dialogLayout->addWidget(titleLabel);
    
    // Create scroll area for settings content
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    
    // Container widget for all settings
    QWidget *scrollContent = new QWidget();
    scrollContent->setStyleSheet("background: transparent;");
    QVBoxLayout *mainLayout = new QVBoxLayout(scrollContent);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    
    // General Settings Group
    QGroupBox *generalGroup = new QGroupBox(tr("General"));
    QFormLayout *generalLayout = new QFormLayout(generalGroup);
    generalLayout->setSpacing(12);
    
    // Language selector
    QComboBox *languageCombo = new QComboBox();
    languageCombo->addItem("🇬🇧 English", "en");
    languageCombo->addItem("🇷🇺 Русский", "ru");
    languageCombo->addItem("🇩🇪 Deutsch", "de");
    languageCombo->addItem("🇪🇸 Español", "es");
    languageCombo->addItem("🇫🇷 Français", "fr");
    
    // Set current language
    QString currentLang = config->language();
    for (int i = 0; i < languageCombo->count(); ++i) {
        if (languageCombo->itemData(i).toString() == currentLang) {
            languageCombo->setCurrentIndex(i);
            break;
        }
    }
    generalLayout->addRow(tr("Language:"), languageCombo);
    
    QCheckBox *minimizeToTrayCheck = new QCheckBox(tr("Hide to tray on minimize"));
    minimizeToTrayCheck->setChecked(config->trayOnMinimize());
    generalLayout->addRow(minimizeToTrayCheck);
    
    QCheckBox *closeToTrayCheck = new QCheckBox(tr("Hide to tray on close"));
    closeToTrayCheck->setChecked(config->trayOnClose());
    generalLayout->addRow(closeToTrayCheck);
    
    QCheckBox *startMinimizedCheck = new QCheckBox(tr("Start minimized"));
    startMinimizedCheck->setChecked(config->startMinimized());
    generalLayout->addRow(startMinimizedCheck);
    
    QCheckBox *darkModeCheck = new QCheckBox(tr("Dark mode"));
    darkModeCheck->setChecked(config->darkMode());
    generalLayout->addRow(darkModeCheck);
    
    QCheckBox *checkUpdatesCheck = new QCheckBox(tr("Check for updates on startup"));
    checkUpdatesCheck->setChecked(config->checkUpdatesOnStartup());
    generalLayout->addRow(checkUpdatesCheck);
    
    mainLayout->addWidget(generalGroup);
    
    // Network Settings Group
    QGroupBox *networkGroup = new QGroupBox(tr("Network"));
    QFormLayout *networkLayout = new QFormLayout(networkGroup);
    networkLayout->setSpacing(12);
    
    QSpinBox *p2pPortSpin = new QSpinBox();
    p2pPortSpin->setRange(1024, 65535);
    p2pPortSpin->setValue(config->p2pPort());
    networkLayout->addRow(tr("P2P Port:"), p2pPortSpin);
    
    QSpinBox *dhtPortSpin = new QSpinBox();
    dhtPortSpin->setRange(1024, 65535);
    dhtPortSpin->setValue(config->dhtPort());
    networkLayout->addRow(tr("DHT Port:"), dhtPortSpin);
    
    QSpinBox *httpPortSpin = new QSpinBox();
    httpPortSpin->setRange(1024, 65535);
    httpPortSpin->setValue(config->httpPort());
    networkLayout->addRow(tr("HTTP API Port:"), httpPortSpin);
    
    QCheckBox *restApiCheck = new QCheckBox(tr("Enable REST API server"));
    restApiCheck->setChecked(config->restApiEnabled());
    networkLayout->addRow(restApiCheck);
    
    mainLayout->addWidget(networkGroup);
    
    // Indexer Settings Group
    QGroupBox *indexerGroup = new QGroupBox(tr("Indexer"));
    QFormLayout *indexerLayout = new QFormLayout(indexerGroup);
    
    QCheckBox *indexerCheck = new QCheckBox(tr("Enable DHT indexer"));
    indexerCheck->setChecked(config->indexerEnabled());
    indexerLayout->addRow(indexerCheck);
    
    QCheckBox *trackersCheck = new QCheckBox(tr("Enable tracker checking"));
    trackersCheck->setChecked(config->trackersEnabled());
    indexerLayout->addRow(trackersCheck);
    
    mainLayout->addWidget(indexerGroup);
    
    // P2P Settings Group (migrated from legacy/app/config-page.js)
    QGroupBox *p2pGroup = new QGroupBox(tr("P2P Network"));
    QFormLayout *p2pLayout = new QFormLayout(p2pGroup);
    p2pLayout->setSpacing(12);
    
    QCheckBox *p2pBootstrapCheck = new QCheckBox(tr("Enable bootstrap nodes"));
    p2pBootstrapCheck->setChecked(config->p2pBootstrap());
    p2pLayout->addRow(p2pBootstrapCheck);
    
    QSpinBox *p2pConnectionsSpin = new QSpinBox();
    p2pConnectionsSpin->setRange(5, 100);
    p2pConnectionsSpin->setValue(config->p2pConnections());
    p2pConnectionsSpin->setToolTip(tr("Maximum number of P2P connections"));
    p2pLayout->addRow(tr("Max connections:"), p2pConnectionsSpin);
    
    QCheckBox *p2pReplicationCheck = new QCheckBox(tr("Enable P2P replication (client)"));
    p2pReplicationCheck->setChecked(config->p2pReplication());
    p2pReplicationCheck->setToolTip(tr("Replicate database from other peers"));
    p2pLayout->addRow(p2pReplicationCheck);
    
    QCheckBox *p2pReplicationServerCheck = new QCheckBox(tr("Enable P2P replication server"));
    p2pReplicationServerCheck->setChecked(config->p2pReplicationServer());
    p2pReplicationServerCheck->setToolTip(tr("Serve database to other peers"));
    p2pLayout->addRow(p2pReplicationServerCheck);
    
    mainLayout->addWidget(p2pGroup);
    
    // Performance Settings Group (migrated from legacy/app/config-page.js)
    QGroupBox *perfGroup = new QGroupBox(tr("Performance"));
    QFormLayout *perfLayout = new QFormLayout(perfGroup);
    perfLayout->setSpacing(12);
    
    QSpinBox *walkIntervalSpin = new QSpinBox();
    walkIntervalSpin->setRange(1, 150);
    walkIntervalSpin->setValue(config->spiderWalkInterval());
    walkIntervalSpin->setToolTip(tr("Interval between DHT walks (lower = faster, more CPU)"));
    perfLayout->addRow(tr("Spider walk interval:"), walkIntervalSpin);
    
    QSpinBox *nodesUsageSpin = new QSpinBox();
    nodesUsageSpin->setRange(0, 1000);
    nodesUsageSpin->setValue(config->spiderNodesUsage());
    nodesUsageSpin->setToolTip(tr("Number of DHT nodes to use (0 = auto)"));
    perfLayout->addRow(tr("DHT nodes usage:"), nodesUsageSpin);
    
    QSpinBox *packagesLimitSpin = new QSpinBox();
    packagesLimitSpin->setRange(0, 5000);
    packagesLimitSpin->setValue(config->spiderPackagesLimit());
    packagesLimitSpin->setToolTip(tr("Maximum network packages per second (0 = unlimited)"));
    perfLayout->addRow(tr("Package limit:"), packagesLimitSpin);
    
    mainLayout->addWidget(perfGroup);
    
    // ==========================================================================
    // Filters Settings Group (migrated from legacy/app/filters-page.js)
    // ==========================================================================
    QGroupBox *filtersGroup = new QGroupBox(tr("Content Filters"));
    QVBoxLayout *filtersLayout = new QVBoxLayout(filtersGroup);
    filtersLayout->setSpacing(12);
    
    // Max files per torrent (slider + spinbox, 0=disabled)
    QHBoxLayout *maxFilesRow = new QHBoxLayout();
    QLabel *maxFilesLabel = new QLabel(tr("Max files per torrent:"));
    maxFilesLabel->setToolTip(tr("Maximum number of files in a torrent (0 = disabled)"));
    QSlider *maxFilesSlider = new QSlider(Qt::Horizontal);
    maxFilesSlider->setRange(0, 50000);
    maxFilesSlider->setValue(config->filtersMaxFiles());
    QSpinBox *maxFilesSpin = new QSpinBox();
    maxFilesSpin->setRange(0, 50000);
    maxFilesSpin->setValue(config->filtersMaxFiles());
    maxFilesSpin->setMinimumWidth(80);
    
    // Sync slider and spinbox
    connect(maxFilesSlider, &QSlider::valueChanged, maxFilesSpin, &QSpinBox::setValue);
    connect(maxFilesSpin, QOverload<int>::of(&QSpinBox::valueChanged), maxFilesSlider, &QSlider::setValue);
    
    maxFilesRow->addWidget(maxFilesLabel);
    maxFilesRow->addWidget(maxFilesSlider, 1);
    maxFilesRow->addWidget(maxFilesSpin);
    filtersLayout->addLayout(maxFilesRow);
    
    QLabel *maxFilesHint = new QLabel(tr("* 0 = Disabled (no limit)"));
    maxFilesHint->setStyleSheet("color: #888; font-size: 11px;");
    filtersLayout->addWidget(maxFilesHint);
    
    // Naming regex filter
    QHBoxLayout *regexRow = new QHBoxLayout();
    QLabel *regexLabel = new QLabel(tr("Name filter (regex):"));
    QLineEdit *regexEdit = new QLineEdit(config->filtersNamingRegExp());
    regexEdit->setPlaceholderText(tr("Regular expression pattern..."));
    
    // Examples dropdown
    QComboBox *regexExamples = new QComboBox();
    regexExamples->addItem(tr("Examples..."), "");
    regexExamples->addItem(tr("Russian + English only"), QString::fromUtf8(R"(^[А-Яа-я0-9A-Za-z.!@?#"$%&:;() *\+,\/;\-=[\\\]\^_{|}<>\u0400-\u04FF]+$)"));
    regexExamples->addItem(tr("English only"), R"(^[0-9A-Za-z.!@?#"$%&:;() *\+,\/;\-=[\\\]\^_{|}<>]+$)");
    regexExamples->addItem(tr("Ignore 'badword'"), R"(^((?!badword).)*$)");
    
    connect(regexExamples, QOverload<int>::of(&QComboBox::currentIndexChanged), [regexEdit, regexExamples](int index) {
        QString example = regexExamples->itemData(index).toString();
        if (!example.isEmpty()) {
            regexEdit->setText(example);
        }
    });
    
    regexRow->addWidget(regexLabel);
    regexRow->addWidget(regexEdit, 1);
    regexRow->addWidget(regexExamples);
    filtersLayout->addLayout(regexRow);
    
    // Negative regex toggle
    QCheckBox *regexNegativeCheck = new QCheckBox(tr("Negative regex filter (reject matches)"));
    regexNegativeCheck->setChecked(config->filtersNamingRegExpNegative());
    regexNegativeCheck->setToolTip(tr("When enabled, torrents matching the regex will be rejected"));
    filtersLayout->addWidget(regexNegativeCheck);
    
    QLabel *regexHint = new QLabel(tr("* Empty string = Disabled"));
    regexHint->setStyleSheet("color: #888; font-size: 11px;");
    filtersLayout->addWidget(regexHint);
    
    // Adult content filter
    QCheckBox *adultFilterCheck = new QCheckBox(tr("Adult content filter (ignore XXX content)"));
    adultFilterCheck->setChecked(config->filtersAdultFilter());
    adultFilterCheck->setToolTip(tr("When enabled, adult content will be filtered out"));
    filtersLayout->addWidget(adultFilterCheck);
    
    // Size filter section
    QGroupBox *sizeFilterBox = new QGroupBox(tr("Size Filter"));
    QFormLayout *sizeLayout = new QFormLayout(sizeFilterBox);
    
    QSpinBox *sizeMinSpin = new QSpinBox();
    sizeMinSpin->setRange(0, 999999);
    sizeMinSpin->setSuffix(" MB");
    sizeMinSpin->setValue(static_cast<int>(config->filtersSizeMin() / (1024 * 1024)));
    sizeMinSpin->setToolTip(tr("Minimum torrent size (0 = no minimum)"));
    sizeLayout->addRow(tr("Minimum size:"), sizeMinSpin);
    
    QSpinBox *sizeMaxSpin = new QSpinBox();
    sizeMaxSpin->setRange(0, 999999);
    sizeMaxSpin->setSuffix(" MB");
    sizeMaxSpin->setValue(static_cast<int>(config->filtersSizeMax() / (1024 * 1024)));
    sizeMaxSpin->setToolTip(tr("Maximum torrent size (0 = no maximum)"));
    sizeLayout->addRow(tr("Maximum size:"), sizeMaxSpin);
    
    filtersLayout->addWidget(sizeFilterBox);
    
    // Content type checkboxes
    QGroupBox *contentTypeBox = new QGroupBox(tr("Content Type Filter"));
    QVBoxLayout *contentTypeLayout = new QVBoxLayout(contentTypeBox);
    
    QLabel *contentTypeHint = new QLabel(tr("Uncheck to disable specific content types:"));
    contentTypeHint->setStyleSheet("color: #888; font-size: 11px;");
    contentTypeLayout->addWidget(contentTypeHint);
    
    // Parse current content type filter
    QString currentContentTypes = config->filtersContentType();
    QStringList enabledTypes = currentContentTypes.isEmpty() ? 
        QStringList{"video", "audio", "pictures", "books", "application", "archive", "disc"} :
        currentContentTypes.split(",", Qt::SkipEmptyParts);
    
    QGridLayout *typeGrid = new QGridLayout();
    QCheckBox *videoCheck = new QCheckBox(tr("Video"));
    videoCheck->setChecked(enabledTypes.contains("video"));
    QCheckBox *audioCheck = new QCheckBox(tr("Audio/Music"));
    audioCheck->setChecked(enabledTypes.contains("audio"));
    QCheckBox *picturesCheck = new QCheckBox(tr("Pictures/Images"));
    picturesCheck->setChecked(enabledTypes.contains("pictures"));
    QCheckBox *booksCheck = new QCheckBox(tr("Books"));
    booksCheck->setChecked(enabledTypes.contains("books"));
    QCheckBox *appsCheck = new QCheckBox(tr("Apps/Games"));
    appsCheck->setChecked(enabledTypes.contains("application"));
    QCheckBox *archivesCheck = new QCheckBox(tr("Archives"));
    archivesCheck->setChecked(enabledTypes.contains("archive"));
    QCheckBox *discsCheck = new QCheckBox(tr("Discs/ISO"));
    discsCheck->setChecked(enabledTypes.contains("disc"));
    
    typeGrid->addWidget(videoCheck, 0, 0);
    typeGrid->addWidget(audioCheck, 0, 1);
    typeGrid->addWidget(picturesCheck, 1, 0);
    typeGrid->addWidget(booksCheck, 1, 1);
    typeGrid->addWidget(appsCheck, 2, 0);
    typeGrid->addWidget(archivesCheck, 2, 1);
    typeGrid->addWidget(discsCheck, 3, 0);
    
    contentTypeLayout->addLayout(typeGrid);
    filtersLayout->addWidget(contentTypeBox);
    
    // Cleanup actions section
    QGroupBox *cleanupBox = new QGroupBox(tr("Database Cleanup"));
    QVBoxLayout *cleanupLayout = new QVBoxLayout(cleanupBox);
    
    QLabel *cleanupDesc = new QLabel(tr("Check and remove torrents that don't match the current filters:"));
    cleanupDesc->setWordWrap(true);
    cleanupLayout->addWidget(cleanupDesc);
    
    // Progress display
    QLabel *cleanupProgress = new QLabel("");
    cleanupProgress->setStyleSheet("font-weight: bold;");
    cleanupLayout->addWidget(cleanupProgress);
    
    QProgressBar *cleanupProgressBar = new QProgressBar();
    cleanupProgressBar->setVisible(false);
    cleanupProgressBar->setRange(0, 100);
    cleanupLayout->addWidget(cleanupProgressBar);
    
    // Buttons row
    QHBoxLayout *cleanupBtnRow = new QHBoxLayout();
    
    QPushButton *checkTorrentsBtn = new QPushButton(tr("Check Torrents"));
    checkTorrentsBtn->setToolTip(tr("Count how many torrents would be removed (dry run)"));
    checkTorrentsBtn->setStyleSheet("background: #5a6268; padding: 8px 16px;");
    
    QPushButton *cleanTorrentsBtn = new QPushButton(tr("Clean Torrents"));
    cleanTorrentsBtn->setToolTip(tr("Remove torrents that don't match the current filters"));
    cleanTorrentsBtn->setStyleSheet("background: #dc3545; padding: 8px 16px;");
    
    cleanupBtnRow->addWidget(checkTorrentsBtn);
    cleanupBtnRow->addWidget(cleanTorrentsBtn);
    cleanupBtnRow->addStretch();
    cleanupLayout->addLayout(cleanupBtnRow);
    
    // Connect cleanup buttons to RatsAPI
    connect(checkTorrentsBtn, &QPushButton::clicked, [this, cleanupProgress, cleanupProgressBar, checkTorrentsBtn, cleanTorrentsBtn]() {
        if (!api) return;
        
        cleanupProgress->setText(tr("Checking torrents..."));
        cleanupProgress->setStyleSheet("color: #ffc107; font-weight: bold;");
        cleanupProgressBar->setVisible(true);
        cleanupProgressBar->setValue(0);
        checkTorrentsBtn->setEnabled(false);
        cleanTorrentsBtn->setEnabled(false);
        
        api->removeTorrents(true, [cleanupProgress, cleanupProgressBar, checkTorrentsBtn, cleanTorrentsBtn](const ApiResponse& response) {
            QMetaObject::invokeMethod(qApp, [=]() {
                cleanupProgressBar->setVisible(false);
                checkTorrentsBtn->setEnabled(true);
                cleanTorrentsBtn->setEnabled(true);
                
                if (response.success) {
                    QJsonObject data = response.data.toObject();
                    int found = data["found"].toInt();
                    int checked = data["checked"].toInt();
                    
                    if (found > 0) {
                        cleanupProgress->setText(tr("Found %1 torrents to remove (checked %2)").arg(found).arg(checked));
                        cleanupProgress->setStyleSheet("color: #ffc107; font-weight: bold;");
                    } else {
                        cleanupProgress->setText(tr("All %1 torrents match filters").arg(checked));
                        cleanupProgress->setStyleSheet("color: #28a745; font-weight: bold;");
                    }
                } else {
                    cleanupProgress->setText(tr("Error: %1").arg(response.error));
                    cleanupProgress->setStyleSheet("color: #dc3545; font-weight: bold;");
                }
            });
        });
    });
    
    connect(cleanTorrentsBtn, &QPushButton::clicked, [this, cleanupProgress, cleanupProgressBar, checkTorrentsBtn, cleanTorrentsBtn]() {
        if (!api) return;
        
        // Confirm before cleaning
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr, tr("Confirm Cleanup"),
            tr("This will permanently remove torrents that don't match the current filters.\n\nAre you sure?"),
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply != QMessageBox::Yes) return;
        
        cleanupProgress->setText(tr("Cleaning torrents..."));
        cleanupProgress->setStyleSheet("color: #dc3545; font-weight: bold;");
        cleanupProgressBar->setVisible(true);
        cleanupProgressBar->setValue(0);
        checkTorrentsBtn->setEnabled(false);
        cleanTorrentsBtn->setEnabled(false);
        
        api->removeTorrents(false, [cleanupProgress, cleanupProgressBar, checkTorrentsBtn, cleanTorrentsBtn](const ApiResponse& response) {
            QMetaObject::invokeMethod(qApp, [=]() {
                cleanupProgressBar->setVisible(false);
                checkTorrentsBtn->setEnabled(true);
                cleanTorrentsBtn->setEnabled(true);
                
                if (response.success) {
                    QJsonObject data = response.data.toObject();
                    int removed = data["removed"].toInt();
                    int checked = data["checked"].toInt();
                    
                    cleanupProgress->setText(tr("Removed %1 torrents (checked %2)").arg(removed).arg(checked));
                    cleanupProgress->setStyleSheet("color: #28a745; font-weight: bold;");
                } else {
                    cleanupProgress->setText(tr("Error: %1").arg(response.error));
                    cleanupProgress->setStyleSheet("color: #dc3545; font-weight: bold;");
                }
            });
        });
    });
    
    // Connect cleanup progress signal from API
    if (api) {
        connect(api.get(), &RatsAPI::cleanupProgress, this, [cleanupProgress, cleanupProgressBar](int current, int total, const QString& status) {
            QMetaObject::invokeMethod(qApp, [=]() {
                if (total > 0) {
                    cleanupProgressBar->setMaximum(total);
                    cleanupProgressBar->setValue(current);
                }
                
                if (status == "check") {
                    cleanupProgress->setText(tr("Checking: %1 found...").arg(current));
                } else {
                    cleanupProgress->setText(tr("Cleaning: %1/%2...").arg(current).arg(total));
                }
            });
        });
    }
    
    filtersLayout->addWidget(cleanupBox);
    
    mainLayout->addWidget(filtersGroup);
    
    // Database Settings Group
    QGroupBox *dbGroup = new QGroupBox(tr("Database"));
    QFormLayout *dbLayout = new QFormLayout(dbGroup);
    
    QHBoxLayout *pathLayout = new QHBoxLayout();
    QLineEdit *dataPathEdit = new QLineEdit(dataDirectory_);
    dataPathEdit->setReadOnly(true);
    QPushButton *browseBtn = new QPushButton(tr("Browse..."));
    pathLayout->addWidget(dataPathEdit);
    pathLayout->addWidget(browseBtn);
    dbLayout->addRow(tr("Data Directory:"), pathLayout);
    
    connect(browseBtn, &QPushButton::clicked, [&dataPathEdit, this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Data Directory", dataDirectory_);
        if (!dir.isEmpty()) {
            dataPathEdit->setText(dir);
        }
    });
    
    mainLayout->addWidget(dbGroup);
    
    mainLayout->addStretch();
    
    // Set scroll content and add to scroll area
    scrollContent->setLayout(mainLayout);
    scrollArea->setWidget(scrollContent);
    dialogLayout->addWidget(scrollArea, 1);  // stretch factor 1 to take available space
    
    // Buttons (outside scroll area)
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttonBox->setStyleSheet(R"(
        QPushButton {
            min-width: 80px;
            padding: 8px 16px;
        }
    )");
    dialogLayout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        // Track what changed for restart notification
        int oldP2pPort = config->p2pPort();
        int oldDhtPort = config->dhtPort();
        int oldHttpPort = config->httpPort();
        bool oldRestApi = config->restApiEnabled();
        QString oldLanguage = config->language();
        
        // Save settings via ConfigManager
        QString newLanguage = languageCombo->currentData().toString();
        config->setLanguage(newLanguage);
        
        config->setTrayOnMinimize(minimizeToTrayCheck->isChecked());
        config->setTrayOnClose(closeToTrayCheck->isChecked());
        config->setStartMinimized(startMinimizedCheck->isChecked());
        config->setDarkMode(darkModeCheck->isChecked());
        config->setCheckUpdatesOnStartup(checkUpdatesCheck->isChecked());
        
        config->setP2pPort(p2pPortSpin->value());
        config->setDhtPort(dhtPortSpin->value());
        config->setHttpPort(httpPortSpin->value());
        config->setRestApiEnabled(restApiCheck->isChecked());
        
        config->setIndexerEnabled(indexerCheck->isChecked());
        config->setTrackersEnabled(trackersCheck->isChecked());
        
        // P2P settings
        config->setP2pBootstrap(p2pBootstrapCheck->isChecked());
        config->setP2pConnections(p2pConnectionsSpin->value());
        config->setP2pReplication(p2pReplicationCheck->isChecked());
        config->setP2pReplicationServer(p2pReplicationServerCheck->isChecked());
        
        // Performance settings
        config->setSpiderWalkInterval(walkIntervalSpin->value());
        config->setSpiderNodesUsage(nodesUsageSpin->value());
        config->setSpiderPackagesLimit(packagesLimitSpin->value());
        
        // Filter settings
        config->setFiltersMaxFiles(maxFilesSpin->value());
        config->setFiltersNamingRegExp(regexEdit->text());
        config->setFiltersNamingRegExpNegative(regexNegativeCheck->isChecked());
        config->setFiltersAdultFilter(adultFilterCheck->isChecked());
        config->setFiltersSizeMin(static_cast<qint64>(sizeMinSpin->value()) * 1024 * 1024);
        config->setFiltersSizeMax(static_cast<qint64>(sizeMaxSpin->value()) * 1024 * 1024);
        
        // Build content type filter string
        QStringList contentTypes;
        if (videoCheck->isChecked()) contentTypes << "video";
        if (audioCheck->isChecked()) contentTypes << "audio";
        if (picturesCheck->isChecked()) contentTypes << "pictures";
        if (booksCheck->isChecked()) contentTypes << "books";
        if (appsCheck->isChecked()) contentTypes << "application";
        if (archivesCheck->isChecked()) contentTypes << "archive";
        if (discsCheck->isChecked()) contentTypes << "disc";
        
        // If all types are checked, store empty string (no filter)
        if (contentTypes.size() == 7) {
            config->setFiltersContentType("");
        } else {
            config->setFiltersContentType(contentTypes.join(","));
        }
        
        // Check if network settings changed (require restart)
        bool needsRestart = (p2pPortSpin->value() != oldP2pPort) ||
                           (dhtPortSpin->value() != oldDhtPort) ||
                           (httpPortSpin->value() != oldHttpPort) ||
                           (restApiCheck->isChecked() != oldRestApi);
        
        // Check if language changed (require restart)
        bool languageChanged = (newLanguage != oldLanguage);
        
        if (needsRestart || languageChanged) {
            QString message = tr("Network setting changes will take effect after restarting the application.");
            if (languageChanged) {
                message = tr("Language and other setting changes will take effect after restarting the application.");
            }
            QMessageBox::information(this, tr("Restart Required"), message);
        }
        
        saveSettings();
        logActivity(tr("Settings saved"));
    }
}

void MainWindow::showAbout()
{
    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle("About Rats Search");
    aboutBox.setTextFormat(Qt::RichText);
    aboutBox.setText(
        "<div style='text-align: center;'>"
        "<h2 style='color: #4a9eff;'>🐀 Rats Search 2.0</h2>"
        "<p style='font-size: 14px;'>BitTorrent P2P Search Engine</p>"
        "<hr>"
        "<p>Built with Qt 6.9 and librats</p>"
        "<p>A powerful decentralized torrent search engine<br>"
        "with DHT crawling and full-text search.</p>"
        "<hr>"
        "<p style='color: #888;'>Copyright © 2026</p>"
        "<p><a href='https://github.com/DEgITx/rats-search' style='color: #4a9eff;'>GitHub Repository</a></p>"
        "</div>"
    );
    aboutBox.setStandardButtons(QMessageBox::Ok);
    aboutBox.exec();
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
    headerLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #4a9eff;");
    layout->addWidget(headerLabel);
    
    // Version info
    QLabel* versionLabel = new QLabel(
        tr("A new version of Rats Search is available.\n\n"
           "Current version: %1\n"
           "New version: %2")
        .arg(UpdateManager::currentVersion(), version));
    versionLabel->setStyleSheet("font-size: 14px;");
    layout->addWidget(versionLabel);
    
    // Release notes
    if (!releaseNotes.isEmpty()) {
        QLabel* notesHeaderLabel = new QLabel(tr("What's new:"));
        notesHeaderLabel->setStyleSheet("font-weight: bold; margin-top: 8px;");
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
    statusLabel->setStyleSheet("color: #888888;");
    layout->addWidget(statusLabel);
    
    layout->addStretch();
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    QPushButton* laterBtn = new QPushButton(tr("Remind Me Later"));
    laterBtn->setStyleSheet("background-color: #3c3f41;");
    
    QPushButton* downloadBtn = new QPushButton(tr("Download && Install"));
    downloadBtn->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #27ae60, stop:1 #2ecc71);");
    
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
                        statusLabel->setStyleSheet("color: #e74c3c;");
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
        statusLabel->setStyleSheet("color: #e74c3c;");
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
