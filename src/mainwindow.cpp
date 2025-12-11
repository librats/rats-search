#include "mainwindow.h"
#include "searchengine.h"
#include "torrentdatabase.h"
#include "torrentspider.h"
#include "p2pnetwork.h"
#include "searchresultmodel.h"
#include "searchapi.h"
#include "torrentitemdelegate.h"
#include "torrentdetailspanel.h"

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
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QTimer>

MainWindow::MainWindow(int p2pPort, int dhtPort, const QString& dataDirectory, QWidget *parent)
    : QMainWindow(parent)
    , ui(nullptr)
    , p2pPort_(p2pPort)
    , dhtPort_(dhtPort)
    , dataDirectory_(dataDirectory)
    , servicesStarted_(false)
    , currentSortField_("seeders")
    , currentSortDesc_(true)
    , trayIcon(nullptr)
    , trayMenu(nullptr)
    , settings_(nullptr)
    , minimizeToTray_(true)
    , closeToTray_(false)
    , startMinimized_(false)
{
    // Load settings first
    settings_ = new QSettings("RatsSearch", "RatsSearch", this);
    loadSettings();
    
    setWindowTitle("Rats Search - BitTorrent P2P Search Engine");
    resize(1400, 900);
    
    // Set application icon
    setWindowIcon(QIcon(":/images/icon.png"));
    
    applyDarkTheme();
    setupUi();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupSystemTray();
    
    // Initialize core components
    torrentDatabase = std::make_unique<TorrentDatabase>(dataDirectory_);
    searchEngine = std::make_unique<SearchEngine>(torrentDatabase.get());
    
    // P2PNetwork is the single owner of RatsClient
    p2pNetwork = std::make_unique<P2PNetwork>(p2pPort_, dhtPort_, dataDirectory_);
    
    // TorrentSpider uses RatsClient from P2PNetwork (doesn't own it)
    torrentSpider = std::make_unique<TorrentSpider>(torrentDatabase.get(), p2pNetwork.get());
    
    // Initialize search API after database
    searchAPI = std::make_unique<SearchAPI>(torrentDatabase.get(), p2pNetwork.get());
    
    connectSignals();
    startServices();
    
    logActivity("🚀 Rats Search started");
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
    searchLineEdit->setPlaceholderText("🔍 Search for torrents...");
    searchLineEdit->setMinimumHeight(44);
    searchLineEdit->setMinimumWidth(400);
    QFont searchFont = searchLineEdit->font();
    searchFont.setPointSize(12);
    searchLineEdit->setFont(searchFont);
    
    searchButton = new QPushButton("Search", this);
    searchButton->setMinimumSize(120, 44);
    searchButton->setDefault(true);
    searchButton->setCursor(Qt::PointingHandCursor);
    
    // Sort combo box
    sortComboBox = new QComboBox(this);
    sortComboBox->addItem("Sort: Seeders ↓", "seeders_desc");
    sortComboBox->addItem("Sort: Seeders ↑", "seeders_asc");
    sortComboBox->addItem("Sort: Size ↓", "size_desc");
    sortComboBox->addItem("Sort: Size ↑", "size_asc");
    sortComboBox->addItem("Sort: Date ↓", "added_desc");
    sortComboBox->addItem("Sort: Date ↑", "added_asc");
    sortComboBox->addItem("Sort: Name A-Z", "name_asc");
    sortComboBox->addItem("Sort: Name Z-A", "name_desc");
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
    tabWidget->addTab(searchTab, "🔍 Search Results");
    
    // Activity tab
    QWidget *activityTab = new QWidget();
    QVBoxLayout *activityTabLayout = new QVBoxLayout(activityTab);
    activityTabLayout->setContentsMargins(8, 8, 8, 8);
    
    activityLog = new QTextEdit();
    activityLog->setReadOnly(true);
    activityLog->setPlaceholderText("Activity log will appear here...");
    activityTabLayout->addWidget(activityLog);
    
    tabWidget->addTab(activityTab, "📋 Activity");
    
    // Statistics tab
    QWidget *statsTab = new QWidget();
    QVBoxLayout *statsTabLayout = new QVBoxLayout(statsTab);
    statsTabLayout->setContentsMargins(16, 16, 16, 16);
    statsTabLayout->setSpacing(16);
    
    // P2P Stats
    QGroupBox *p2pGroup = new QGroupBox("🌐 P2P Network");
    QVBoxLayout *p2pLayout = new QVBoxLayout(p2pGroup);
    QLabel *p2pStatsLabel = new QLabel("Connected peers: 0\nDHT nodes: 0\nTotal data exchanged: 0 MB");
    p2pStatsLabel->setStyleSheet("color: #cccccc;");
    p2pLayout->addWidget(p2pStatsLabel);
    
    // Database Stats
    QGroupBox *dbGroup = new QGroupBox("💾 Database");
    QVBoxLayout *dbLayout = new QVBoxLayout(dbGroup);
    QLabel *dbStatsLabel = new QLabel("Indexed torrents: 0\nTotal files: 0\nDatabase size: 0 MB");
    dbStatsLabel->setStyleSheet("color: #cccccc;");
    dbLayout->addWidget(dbStatsLabel);
    
    statsTabLayout->addWidget(p2pGroup);
    statsTabLayout->addWidget(dbGroup);
    statsTabLayout->addStretch();
    
    tabWidget->addTab(statsTab, "📊 Statistics");
    
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
    QMenu *fileMenu = menuBar()->addMenu("&File");
    
    QAction *settingsAction = fileMenu->addAction("⚙️ &Settings");
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    
    fileMenu->addSeparator();
    
    QAction *quitAction = fileMenu->addAction("🚪 &Quit");
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QMainWindow::close);
    
    // View menu
    QMenu *viewMenu = menuBar()->addMenu("&View");
    
    QAction *statsAction = viewMenu->addAction("📊 &Statistics");
    connect(statsAction, &QAction::triggered, this, &MainWindow::showStatistics);
    
    // Help menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    
    QAction *aboutAction = helpMenu->addAction("ℹ️ &About");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar("Main Toolbar");
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(20, 20));
    
    // Add actions to toolbar
    QAction *refreshAction = toolBar->addAction("🔄 Refresh");
    connect(refreshAction, &QAction::triggered, [this]() {
        if (!currentSearchQuery_.isEmpty()) {
            performSearch(currentSearchQuery_);
        }
    });
    
    toolBar->addSeparator();
    
    QAction *clearAction = toolBar->addAction("🗑️ Clear");
    connect(clearAction, &QAction::triggered, [this]() {
        searchResultModel->clearResults();
        detailsPanel->clear();
        detailsPanel->hide();
    });
}

void MainWindow::setupStatusBar()
{
    p2pStatusLabel = new QLabel("🔌 P2P: Starting...");
    peerCountLabel = new QLabel("👥 Peers: 0");
    torrentCountLabel = new QLabel("📦 Torrents: 0");
    spiderStatusLabel = new QLabel("🕷️ Spider: Idle");
    
    statusBar()->addWidget(p2pStatusLabel);
    statusBar()->addWidget(peerCountLabel);
    statusBar()->addWidget(torrentCountLabel);
    statusBar()->addWidget(spiderStatusLabel);
    statusBar()->addPermanentWidget(new QLabel("✅ Ready"));
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
    
    logActivity("🔧 Initializing services...");
    
    // Initialize database
    if (!torrentDatabase->initialize()) {
        QMessageBox::critical(this, "Error", "Failed to initialize database!");
        logActivity("❌ Database initialization failed");
        return;
    }
    logActivity("✅ Database initialized");
    
    // Start P2P network
    if (!p2pNetwork->start()) {
        QMessageBox::warning(this, "Warning", "Failed to start P2P network. Some features may be limited.");
        logActivity("⚠️ P2P network failed to start");
    } else {
        logActivity("✅ P2P network started");
    }
    
    // Start torrent spider
    if (!torrentSpider->start()) {
        QMessageBox::warning(this, "Warning", "Failed to start torrent spider. Automatic indexing disabled.");
        logActivity("⚠️ Torrent spider failed to start");
    } else {
        logActivity("✅ Torrent spider started");
    }
    
    servicesStarted_ = true;
    updateStatusBar();
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
    
    // Use SearchAPI for searching
    QJsonObject navigation;
    navigation["limit"] = 100;
    navigation["safeSearch"] = false;
    navigation["orderBy"] = orderBy;
    navigation["orderDesc"] = orderDesc;
    
    searchAPI->searchTorrent(query, navigation, [this, query](const QJsonArray& torrents) {
        // Convert JSON to TorrentInfo for model
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
    if (torrentDatabase) {
        int count = torrentDatabase->getTorrentCount();
        torrentCountLabel->setText(QString("📦 Torrents: %1").arg(count));
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
    if (closeToTray_ && trayIcon && trayIcon->isVisible()) {
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
            "Confirm Exit",
            "Are you sure you want to exit Rats Search?",
            QMessageBox::Yes | QMessageBox::No);
            
        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }
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
    // TODO: Implement download via TorrentClient
    QMessageBox::information(this, "Download", 
        "Download feature coming soon!\n\nFor now, use the magnet link to download with your preferred torrent client.");
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
    
    QAction *magnetAction = contextMenu.addAction("🧲 Open Magnet Link");
    connect(magnetAction, &QAction::triggered, [this, torrent]() {
        QString magnetLink = QString("magnet:?xt=urn:btih:%1&dn=%2")
            .arg(torrent.hash)
            .arg(QUrl::toPercentEncoding(torrent.name));
        QDesktopServices::openUrl(QUrl(magnetLink));
        logActivity(QString("🧲 Opened magnet link for: %1").arg(torrent.name));
    });
    
    QAction *copyHashAction = contextMenu.addAction("📋 Copy Info Hash");
    connect(copyHashAction, &QAction::triggered, [this, torrent]() {
        QApplication::clipboard()->setText(torrent.hash);
        statusBar()->showMessage("✅ Hash copied to clipboard", 2000);
    });
    
    QAction *copyMagnetAction = contextMenu.addAction("📋 Copy Magnet Link");
    connect(copyMagnetAction, &QAction::triggered, [this, torrent]() {
        QString magnetLink = QString("magnet:?xt=urn:btih:%1&dn=%2")
            .arg(torrent.hash)
            .arg(QUrl::toPercentEncoding(torrent.name));
        QApplication::clipboard()->setText(magnetLink);
        statusBar()->showMessage("✅ Magnet link copied to clipboard", 2000);
    });
    
    contextMenu.addSeparator();
    
    QAction *detailsAction = contextMenu.addAction("ℹ️ Show Details");
    connect(detailsAction, &QAction::triggered, [this, index]() {
        onTorrentSelected(index);
    });
    
    contextMenu.exec(resultsTableView->viewport()->mapToGlobal(pos));
}

void MainWindow::onP2PStatusChanged(const QString &status)
{
    p2pStatusLabel->setText("🔌 P2P: " + status);
    logActivity(QString("🔌 P2P status: %1").arg(status));
}

void MainWindow::onPeerCountChanged(int count)
{
    peerCountLabel->setText(QString("👥 Peers: %1").arg(count));
}

void MainWindow::onSpiderStatusChanged(const QString &status)
{
    spiderStatusLabel->setText("🕷️ Spider: " + status);
}

void MainWindow::onTorrentIndexed(const QString &infoHash, const QString &name)
{
    Q_UNUSED(infoHash);
    statusBar()->showMessage(QString("📥 Indexed: %1").arg(name), 2000);
    updateStatusBar();
}

void MainWindow::showSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle("⚙️ Settings");
    dialog.setMinimumSize(500, 400);
    dialog.setStyleSheet(this->styleSheet());
    
    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    
    // Title
    QLabel *titleLabel = new QLabel("🐀 Rats Search Settings");
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #4a9eff;");
    mainLayout->addWidget(titleLabel);
    
    // General Settings Group
    QGroupBox *generalGroup = new QGroupBox("General");
    QFormLayout *generalLayout = new QFormLayout(generalGroup);
    generalLayout->setSpacing(12);
    
    QCheckBox *minimizeToTrayCheck = new QCheckBox("Hide to tray on minimize");
    minimizeToTrayCheck->setChecked(minimizeToTray_);
    generalLayout->addRow(minimizeToTrayCheck);
    
    QCheckBox *closeToTrayCheck = new QCheckBox("Hide to tray on close");
    closeToTrayCheck->setChecked(closeToTray_);
    generalLayout->addRow(closeToTrayCheck);
    
    QCheckBox *startMinimizedCheck = new QCheckBox("Start minimized");
    startMinimizedCheck->setChecked(startMinimized_);
    generalLayout->addRow(startMinimizedCheck);
    
    mainLayout->addWidget(generalGroup);
    
    // Network Settings Group
    QGroupBox *networkGroup = new QGroupBox("Network");
    QFormLayout *networkLayout = new QFormLayout(networkGroup);
    networkLayout->setSpacing(12);
    
    QSpinBox *p2pPortSpin = new QSpinBox();
    p2pPortSpin->setRange(1024, 65535);
    p2pPortSpin->setValue(p2pPort_);
    networkLayout->addRow("P2P Port:", p2pPortSpin);
    
    QSpinBox *dhtPortSpin = new QSpinBox();
    dhtPortSpin->setRange(1024, 65535);
    dhtPortSpin->setValue(dhtPort_);
    networkLayout->addRow("DHT Port:", dhtPortSpin);
    
    mainLayout->addWidget(networkGroup);
    
    // Database Settings Group
    QGroupBox *dbGroup = new QGroupBox("Database");
    QFormLayout *dbLayout = new QFormLayout(dbGroup);
    
    QHBoxLayout *pathLayout = new QHBoxLayout();
    QLineEdit *dataPathEdit = new QLineEdit(dataDirectory_);
    dataPathEdit->setReadOnly(true);
    QPushButton *browseBtn = new QPushButton("Browse...");
    pathLayout->addWidget(dataPathEdit);
    pathLayout->addWidget(browseBtn);
    dbLayout->addRow("Data Directory:", pathLayout);
    
    connect(browseBtn, &QPushButton::clicked, [&dataPathEdit, this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Data Directory", dataDirectory_);
        if (!dir.isEmpty()) {
            dataPathEdit->setText(dir);
        }
    });
    
    mainLayout->addWidget(dbGroup);
    
    mainLayout->addStretch();
    
    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttonBox->setStyleSheet(R"(
        QPushButton {
            min-width: 80px;
            padding: 8px 16px;
        }
    )");
    mainLayout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        // Save settings
        minimizeToTray_ = minimizeToTrayCheck->isChecked();
        closeToTray_ = closeToTrayCheck->isChecked();
        startMinimized_ = startMinimizedCheck->isChecked();
        
        // Port changes require restart
        int newP2pPort = p2pPortSpin->value();
        int newDhtPort = dhtPortSpin->value();
        
        if (newP2pPort != p2pPort_ || newDhtPort != dhtPort_) {
            QMessageBox::information(this, "Restart Required",
                "Port changes will take effect after restarting the application.");
        }
        
        saveSettings();
        logActivity("✅ Settings saved");
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
        "<p style='color: #888;'>Copyright © 2025</p>"
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
    
    QAction *showAction = trayMenu->addAction("🔍 Show Window");
    connect(showAction, &QAction::triggered, this, &MainWindow::toggleWindowVisibility);
    
    trayMenu->addSeparator();
    
    QAction *statsAction = trayMenu->addAction("📊 Statistics");
    connect(statsAction, &QAction::triggered, [this]() {
        show();
        activateWindow();
        showStatistics();
    });
    
    QAction *settingsAction = trayMenu->addAction("⚙️ Settings");
    connect(settingsAction, &QAction::triggered, [this]() {
        show();
        activateWindow();
        showSettings();
    });
    
    trayMenu->addSeparator();
    
    QAction *quitAction = trayMenu->addAction("🚪 Quit");
    connect(quitAction, &QAction::triggered, [this]() {
        closeToTray_ = false;  // Force actual close
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
        if (isMinimized() && minimizeToTray_ && trayIcon && trayIcon->isVisible()) {
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
    if (!settings_) return;
    
    minimizeToTray_ = settings_->value("general/minimizeToTray", true).toBool();
    closeToTray_ = settings_->value("general/closeToTray", false).toBool();
    startMinimized_ = settings_->value("general/startMinimized", false).toBool();
    
    // Window geometry
    if (settings_->contains("window/geometry")) {
        restoreGeometry(settings_->value("window/geometry").toByteArray());
    }
    if (settings_->contains("window/state")) {
        restoreState(settings_->value("window/state").toByteArray());
    }
    
    qInfo() << "Settings loaded";
}

void MainWindow::saveSettings()
{
    if (!settings_) return;
    
    settings_->setValue("general/minimizeToTray", minimizeToTray_);
    settings_->setValue("general/closeToTray", closeToTray_);
    settings_->setValue("general/startMinimized", startMinimized_);
    
    // Window geometry
    settings_->setValue("window/geometry", saveGeometry());
    settings_->setValue("window/state", saveState());
    
    settings_->sync();
    qInfo() << "Settings saved";
}
