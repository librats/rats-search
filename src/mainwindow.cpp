#include "mainwindow.h"
#include "searchengine.h"
#include "torrentdatabase.h"
#include "torrentspider.h"
#include "p2pnetwork.h"
#include "searchresultmodel.h"

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

MainWindow::MainWindow(int p2pPort, int dhtPort, const QString& dataDirectory, QWidget *parent)
    : QMainWindow(parent)
    , ui(nullptr)
    , p2pPort_(p2pPort)
    , dhtPort_(dhtPort)
    , dataDirectory_(dataDirectory)
    , servicesStarted_(false)
{
    setWindowTitle("Rats Search - BitTorrent P2P Search Engine");
    resize(1200, 800);
    
    setupUi();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    
    // Initialize core components
    torrentDatabase = std::make_unique<TorrentDatabase>(dataDirectory_);
    searchEngine = std::make_unique<SearchEngine>(torrentDatabase.get());
    p2pNetwork = std::make_unique<P2PNetwork>(p2pPort_, dataDirectory_);
    torrentSpider = std::make_unique<TorrentSpider>(torrentDatabase.get(), dhtPort_);
    
    connectSignals();
    startServices();
}

MainWindow::~MainWindow()
{
    stopServices();
}

void MainWindow::setupUi()
{
    // Central widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Search bar
    QHBoxLayout *searchLayout = new QHBoxLayout();
    
    searchLineEdit = new QLineEdit(this);
    searchLineEdit->setPlaceholderText("Search torrents...");
    searchLineEdit->setMinimumHeight(40);
    QFont searchFont = searchLineEdit->font();
    searchFont.setPointSize(12);
    searchLineEdit->setFont(searchFont);
    
    searchButton = new QPushButton("Search", this);
    searchButton->setMinimumSize(100, 40);
    searchButton->setDefault(true);
    
    searchLayout->addWidget(searchLineEdit, 1);
    searchLayout->addWidget(searchButton);
    
    mainLayout->addLayout(searchLayout);
    
    // Tab widget for different views
    tabWidget = new QTabWidget(this);
    
    // Search results tab
    QWidget *searchTab = new QWidget();
    QVBoxLayout *searchTabLayout = new QVBoxLayout(searchTab);
    
    resultsTableView = new QTableView(this);
    searchResultModel = new SearchResultModel(this);
    resultsTableView->setModel(searchResultModel);
    resultsTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultsTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    resultsTableView->setAlternatingRowColors(true);
    resultsTableView->setSortingEnabled(true);
    resultsTableView->horizontalHeader()->setStretchLastSection(true);
    resultsTableView->verticalHeader()->setVisible(false);
    resultsTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    // Set column widths
    resultsTableView->setColumnWidth(0, 500);  // Name
    resultsTableView->setColumnWidth(1, 100);  // Size
    resultsTableView->setColumnWidth(2, 80);   // Seeders
    resultsTableView->setColumnWidth(3, 80);   // Leechers
    resultsTableView->setColumnWidth(4, 150);  // Date
    
    searchTabLayout->addWidget(resultsTableView);
    tabWidget->addTab(searchTab, "Search Results");
    
    // Activity tab
    QWidget *activityTab = new QWidget();
    QVBoxLayout *activityTabLayout = new QVBoxLayout(activityTab);
    
    QTextEdit *activityLog = new QTextEdit();
    activityLog->setReadOnly(true);
    activityLog->setPlaceholderText("Activity log will appear here...");
    activityTabLayout->addWidget(activityLog);
    
    tabWidget->addTab(activityTab, "Activity");
    
    // Statistics tab
    QWidget *statsTab = new QWidget();
    QVBoxLayout *statsTabLayout = new QVBoxLayout(statsTab);
    
    QGroupBox *p2pGroup = new QGroupBox("P2P Network");
    QVBoxLayout *p2pLayout = new QVBoxLayout(p2pGroup);
    QLabel *p2pStatsLabel = new QLabel("Connected peers: 0\nTotal data exchanged: 0 MB");
    p2pLayout->addWidget(p2pStatsLabel);
    
    QGroupBox *dbGroup = new QGroupBox("Database");
    QVBoxLayout *dbLayout = new QVBoxLayout(dbGroup);
    QLabel *dbStatsLabel = new QLabel("Indexed torrents: 0\nDatabase size: 0 MB");
    dbLayout->addWidget(dbStatsLabel);
    
    statsTabLayout->addWidget(p2pGroup);
    statsTabLayout->addWidget(dbGroup);
    statsTabLayout->addStretch();
    
    tabWidget->addTab(statsTab, "Statistics");
    
    mainLayout->addWidget(tabWidget);
}

void MainWindow::setupMenuBar()
{
    // File menu
    QMenu *fileMenu = menuBar()->addMenu("&File");
    
    QAction *settingsAction = fileMenu->addAction("&Settings");
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    
    fileMenu->addSeparator();
    
    QAction *quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QMainWindow::close);
    
    // View menu
    QMenu *viewMenu = menuBar()->addMenu("&View");
    
    QAction *statsAction = viewMenu->addAction("&Statistics");
    connect(statsAction, &QAction::triggered, this, &MainWindow::showStatistics);
    
    // Help menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    
    QAction *aboutAction = helpMenu->addAction("&About");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar("Main Toolbar");
    toolBar->setMovable(false);
    
    // Add actions to toolbar
    QAction *refreshAction = toolBar->addAction("Refresh");
    connect(refreshAction, &QAction::triggered, [this]() {
        if (!searchLineEdit->text().isEmpty()) {
            performSearch(searchLineEdit->text());
        }
    });
}

void MainWindow::setupStatusBar()
{
    p2pStatusLabel = new QLabel("P2P: Disconnected");
    peerCountLabel = new QLabel("Peers: 0");
    torrentCountLabel = new QLabel("Torrents: 0");
    spiderStatusLabel = new QLabel("Spider: Idle");
    
    statusBar()->addWidget(p2pStatusLabel);
    statusBar()->addWidget(peerCountLabel);
    statusBar()->addWidget(torrentCountLabel);
    statusBar()->addWidget(spiderStatusLabel);
    statusBar()->addPermanentWidget(new QLabel("Ready"));
}

void MainWindow::connectSignals()
{
    // Search signals
    connect(searchButton, &QPushButton::clicked, this, &MainWindow::onSearchButtonClicked);
    connect(searchLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSearchButtonClicked);
    connect(searchLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    
    // Table view signals
    connect(resultsTableView, &QTableView::doubleClicked, this, &MainWindow::onTorrentDoubleClicked);
    
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
    
    // Initialize database
    if (!torrentDatabase->initialize()) {
        QMessageBox::critical(this, "Error", "Failed to initialize database!");
        return;
    }
    
    // Start P2P network
    if (!p2pNetwork->start()) {
        QMessageBox::warning(this, "Warning", "Failed to start P2P network. Some features may be limited.");
    }
    
    // Start torrent spider
    if (!torrentSpider->start()) {
        QMessageBox::warning(this, "Warning", "Failed to start torrent spider. Automatic indexing disabled.");
    }
    
    servicesStarted_ = true;
    updateStatusBar();
}

void MainWindow::stopServices()
{
    if (!servicesStarted_) {
        return;
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

void MainWindow::performSearch(const QString &query)
{
    if (query.isEmpty()) {
        return;
    }
    
    statusBar()->showMessage("Searching...", 2000);
    
    // Perform search in database
    auto results = searchEngine->search(query);
    searchResultModel->setResults(results);
    
    // Also perform P2P search if connected
    if (p2pNetwork->isConnected()) {
        p2pNetwork->searchTorrents(query);
    }
    
    statusBar()->showMessage(QString("Found %1 results").arg(results.size()), 3000);
}

void MainWindow::updateStatusBar()
{
    if (torrentDatabase) {
        int count = torrentDatabase->getTorrentCount();
        torrentCountLabel->setText(QString("Torrents: %1").arg(count));
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
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
    
    stopServices();
    event->accept();
}

// Slots implementation
void MainWindow::onSearchButtonClicked()
{
    performSearch(searchLineEdit->text());
}

void MainWindow::onSearchTextChanged(const QString &text)
{
    searchButton->setEnabled(!text.isEmpty());
}

void MainWindow::onTorrentDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    
    // Get torrent info from model
    QString name = searchResultModel->data(searchResultModel->index(index.row(), 0)).toString();
    QString infoHash = searchResultModel->getInfoHash(index.row());
    
    QMessageBox::information(this, "Torrent Details", 
        QString("Name: %1\nInfo Hash: %2").arg(name, infoHash));
}

void MainWindow::onP2PStatusChanged(const QString &status)
{
    p2pStatusLabel->setText("P2P: " + status);
}

void MainWindow::onPeerCountChanged(int count)
{
    peerCountLabel->setText(QString("Peers: %1").arg(count));
}

void MainWindow::onSpiderStatusChanged(const QString &status)
{
    spiderStatusLabel->setText("Spider: " + status);
}

void MainWindow::onTorrentIndexed(const QString &infoHash, const QString &name)
{
    Q_UNUSED(infoHash);
    statusBar()->showMessage(QString("Indexed: %1").arg(name), 2000);
    updateStatusBar();
}

void MainWindow::showSettings()
{
    QMessageBox::information(this, "Settings", "Settings dialog coming soon!");
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "About Rats Search",
        "<h2>Rats Search 2.0</h2>"
        "<p>BitTorrent P2P Search Engine</p>"
        "<p>Built with Qt 6.9 and librats</p>"
        "<p>Copyright © 2025</p>"
        "<p><a href='https://github.com/DEgITx/rats-search'>GitHub</a></p>");
}

void MainWindow::showStatistics()
{
    tabWidget->setCurrentIndex(2);  // Switch to Statistics tab
}

