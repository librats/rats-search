#include <QApplication>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QTextStream>
#include <QTimer>
#include <QElapsedTimer>
#include <iostream>
#include <csignal>
#include "mainwindow.h"
#include "torrentdatabase.h"
#include "torrentspider.h"
#include "p2pnetwork.h"
#include "api/ratsapi.h"
#include "api/configmanager.h"
#include "api/apiserver.h"
#include "librats/src/logger.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

// Global pointers for signal handling
static TorrentDatabase* g_database = nullptr;
static TorrentSpider* g_spider = nullptr;
static P2PNetwork* g_p2p = nullptr;
static QCoreApplication* g_app = nullptr;
static bool g_shutdownRequested = false;

#ifdef _WIN32
// Allocate and attach a console on Windows for stdout/stderr
void attachConsoleOnWindows()
{
    // Try to attach to parent console first (if run from cmd.exe)
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        // If no parent console, allocate a new one
        AllocConsole();
    }
    
    // Redirect stdout
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);
    
    // Make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog point to console as well
    std::ios::sync_with_stdio();
    
    // Clear the error state for each of the C++ standard stream objects
    std::cout.clear();
    std::cerr.clear();
    std::cin.clear();
    
    std::wcout.clear();
    std::wcerr.clear();
    std::wcin.clear();
}
#endif

// Custom Qt message handler using librats logger
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context);
    QByteArray localMsg = msg.toLocal8Bit();
    std::string message = localMsg.constData();
    
    // Get librats logger instance
    auto& logger = librats::Logger::getInstance();
    
    // Map Qt message types to librats log levels
    switch (type) {
    case QtDebugMsg:
        logger.log(librats::LogLevel::DEBUG, "RatsSearch", message);
        break;
    case QtInfoMsg:
        logger.log(librats::LogLevel::INFO, "RatsSearch", message);
        break;
    case QtWarningMsg:
        logger.log(librats::LogLevel::WARN, "RatsSearch", message);
        break;
    case QtCriticalMsg:
        logger.log(librats::LogLevel::ERROR, "RatsSearch", message);
        break;
    case QtFatalMsg:
        logger.log(librats::LogLevel::ERROR, "RatsSearch", "[FATAL] " + message);
        abort();
        break;
    }
}

// Signal handler for graceful shutdown
void signalHandler(int signum)
{
    if (g_shutdownRequested) {
        std::cerr << "Force shutdown..." << std::endl;
        exit(1);
    }
    
    g_shutdownRequested = true;
    std::cout << "\nShutting down..." << std::endl;
    
    // Schedule shutdown on Qt event loop
    if (g_app) {
        QMetaObject::invokeMethod(g_app, []() {
            if (g_spider) {
                g_spider->stop();
            }
            if (g_p2p) {
                g_p2p->stop();
            }
            if (g_database) {
                g_database->close();
            }
            QCoreApplication::quit();
        }, Qt::QueuedConnection);
    }
    
    Q_UNUSED(signum);
}

// Console mode main loop - uses the existing QCoreApplication from main()
int runConsoleMode(QCoreApplication& app, int p2pPort, int dhtPort, const QString& dataDir, bool enableSpider)
{
    g_app = &app;
    
    qInfo() << "Rats Search Console Mode";
    qInfo() << "Data directory:" << dataDir;
    qInfo() << "P2P port:" << p2pPort;
    qInfo() << "DHT port:" << dhtPort;
    
    // Initialize database
    TorrentDatabase database(dataDir);
    g_database = &database;
    
    if (!database.initialize()) {
        qCritical() << "Failed to initialize database";
        return 1;
    }
    
    // Initialize P2P network (single owner of RatsClient)
    P2PNetwork p2p(p2pPort, dhtPort, dataDir);
    g_p2p = &p2p;
    
    if (!p2p.start()) {
        qWarning() << "Failed to start P2P network";
    }
    
    // Initialize spider - uses RatsClient from P2PNetwork
    TorrentSpider spider(&database, &p2p);
    g_spider = &spider;
    
    if (enableSpider) {
        if (!spider.start()) {
            qWarning() << "Failed to start spider";
        }
    }
    
    // Create configuration manager
    ConfigManager config(dataDir + "/rats.json");
    config.load();
    
    // Create RatsAPI
    RatsAPI api;
    api.initialize(&database, &p2p, nullptr, &config);
    
    // Start API server if enabled
    std::unique_ptr<ApiServer> apiServer;
    if (config.restApiEnabled()) {
        apiServer = std::make_unique<ApiServer>(&api);
        if (apiServer->start(config.httpPort())) {
            qInfo() << "API server started on port" << config.httpPort();
        }
    }
    
    // Print statistics periodically
    QTimer statsTimer;
    QObject::connect(&statsTimer, &QTimer::timeout, [&]() {
        auto stats = database.getStatistics();
        qInfo() << "Stats - Torrents:" << stats.totalTorrents 
                << "Files:" << stats.totalFiles
                << "Size:" << (stats.totalSize / (1024*1024*1024)) << "GB"
                << "Peers:" << p2p.getPeerCount()
                << "DHT nodes:" << p2p.getDhtNodeCount();
    });
    statsTimer.start(30000);  // Every 30 seconds
    
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    qInfo() << "Console mode running. Press Ctrl+C to stop.";
    
    // Interactive command loop (optional)
    QTextStream in(stdin);
    QTimer commandTimer;
    
    QObject::connect(&commandTimer, &QTimer::timeout, [&]() {
        if (in.atEnd()) return;
        
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) return;
        
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        QString cmd = parts.isEmpty() ? "" : parts[0].toLower();
        
        if (cmd == "quit" || cmd == "exit") {
            signalHandler(0);
        }
        else if (cmd == "stats") {
            auto stats = database.getStatistics();
            std::cout << "Torrents: " << stats.totalTorrents << std::endl;
            std::cout << "Files: " << stats.totalFiles << std::endl;
            std::cout << "Size: " << (stats.totalSize / (1024*1024*1024)) << " GB" << std::endl;
            std::cout << "Peers: " << p2p.getPeerCount() << std::endl;
            std::cout << "DHT nodes: " << p2p.getDhtNodeCount() << std::endl;
            if (enableSpider) {
                std::cout << "Indexed: " << spider.getIndexedCount() << std::endl;
                std::cout << "Pending: " << spider.getPendingCount() << std::endl;
            }
        }
        else if (cmd == "search" && parts.size() > 1) {
            QString query = parts.mid(1).join(' ');
            std::cout << "Searching for: " << query.toStdString() << std::endl;
            
            SearchOptions options;
            options.query = query;
            options.limit = 10;
            
            auto results = database.searchTorrents(options);
            std::cout << "Found " << results.size() << " results:" << std::endl;
            
            for (const TorrentInfo& t : results) {
                std::cout << "  " << t.hash.left(8).toStdString() << " "
                          << t.name.toStdString() << " (" 
                          << (t.size / (1024*1024)) << " MB, "
                          << t.seeders << " seeders)" << std::endl;
            }
        }
        else if (cmd == "recent") {
            int limit = (parts.size() > 1) ? parts[1].toInt() : 10;
            auto results = database.getRecentTorrents(limit);
            
            std::cout << "Recent torrents:" << std::endl;
            for (const TorrentInfo& t : results) {
                std::cout << "  " << t.hash.left(8).toStdString() << " "
                          << t.name.toStdString() << std::endl;
            }
        }
        else if (cmd == "top") {
            QString type = (parts.size() > 1) ? parts[1] : "";
            auto results = database.getTopTorrents(type, "", 0, 10);
            
            std::cout << "Top torrents:" << std::endl;
            for (const TorrentInfo& t : results) {
                std::cout << "  " << t.seeders << " seeders - "
                          << t.name.toStdString() << std::endl;
            }
        }
        else if (cmd == "spider") {
            if (parts.size() > 1 && parts[1] == "start") {
                if (!spider.isRunning()) {
                    spider.start();
                    std::cout << "Spider started" << std::endl;
                }
            }
            else if (parts.size() > 1 && parts[1] == "stop") {
                spider.stop();
                std::cout << "Spider stopped" << std::endl;
            }
            else {
                std::cout << "Spider is " << (spider.isRunning() ? "running" : "stopped") << std::endl;
                std::cout << "Indexed: " << spider.getIndexedCount() << std::endl;
            }
        }
        else if (cmd == "help") {
            std::cout << "Commands:" << std::endl;
            std::cout << "  stats        - Show statistics" << std::endl;
            std::cout << "  search <q>   - Search torrents" << std::endl;
            std::cout << "  recent [n]   - Show recent torrents" << std::endl;
            std::cout << "  top [type]   - Show top torrents" << std::endl;
            std::cout << "  spider start - Start spider" << std::endl;
            std::cout << "  spider stop  - Stop spider" << std::endl;
            std::cout << "  quit         - Exit" << std::endl;
        }
        else if (!cmd.isEmpty()) {
            std::cout << "Unknown command: " << cmd.toStdString() << ". Type 'help' for commands." << std::endl;
        }
    });
    commandTimer.start(100);
    
    return app.exec();
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    // Attach console on Windows to see stdout/stderr
    attachConsoleOnWindows();
#endif
    
    // Install custom message handler for Qt logging
    qInstallMessageHandler(customMessageHandler);
    
    // Parse command line before creating QApplication
    // to check if we need console mode
    bool consoleMode = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--console" || QString(argv[i]) == "-c") {
            consoleMode = true;
            break;
        }
    }
    
    // Create appropriate application type
    if (consoleMode) {
        QCoreApplication app(argc, argv);
        
        // Set application information
        QCoreApplication::setApplicationName("Rats Search");
        QCoreApplication::setOrganizationName("");  // Empty to avoid nested folder
        QCoreApplication::setApplicationVersion("2.0.0");
        
        // Command line parser
        QCommandLineParser parser;
        parser.setApplicationDescription("Rats Search - BitTorrent P2P Search Engine (Console Mode)");
        parser.addHelpOption();
        parser.addVersionOption();
        
        QCommandLineOption consoleOption(QStringList() << "c" << "console",
            "Run in console mode (no GUI)");
        parser.addOption(consoleOption);
        
        QCommandLineOption portOption(QStringList() << "p" << "port",
            "P2P listen port (default: 8080)", "port", "8080");
        parser.addOption(portOption);
        
        QCommandLineOption dhtPortOption(QStringList() << "d" << "dht-port",
            "DHT port (default: 6881)", "dht-port", "6881");
        parser.addOption(dhtPortOption);
        
        QCommandLineOption dataDirectoryOption(QStringList() << "data-dir",
            "Data directory for database and config", "path");
        parser.addOption(dataDirectoryOption);
        
        QCommandLineOption spiderOption(QStringList() << "s" << "spider",
            "Enable torrent spider (default: disabled in console mode)");
        parser.addOption(spiderOption);
        
        parser.process(app);
        
        // Get command line options
        int p2pPort = parser.value(portOption).toInt();
        int dhtPort = parser.value(dhtPortOption).toInt();
        bool enableSpider = parser.isSet(spiderOption);
        
        QString dataDir;
        if (parser.isSet(dataDirectoryOption)) {
            dataDir = parser.value(dataDirectoryOption);
        } else {
            dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        }
        
        // Create data directory if it doesn't exist
        QDir dir;
        if (!dir.mkpath(dataDir)) {
            qCritical() << "Failed to create data directory:" << dataDir;
            return 1;
        }
        
        // Enable file logging
        auto& logger = librats::Logger::getInstance();
        QString logFilePath = dataDir + "/rats-search.log";
        logger.set_log_file_path(logFilePath.toStdString());
        logger.set_log_rotation_size(0);  // No rotation
        logger.set_file_logging_enabled(true);
        logger.set_log_level(librats::LogLevel::DEBUG);  // Log everything to file
        
        qInfo() << "Log file:" << logFilePath;
        
        return runConsoleMode(app, p2pPort, dhtPort, dataDir, enableSpider);
    }
    else {
        // GUI mode
        QElapsedTimer startupTimer;
        startupTimer.start();
        
        QApplication app(argc, argv);
        qInfo() << "QApplication created:" << startupTimer.elapsed() << "ms";
        
        // Set application information
        QApplication::setApplicationName("Rats Search");
        QApplication::setOrganizationName("");  // Empty to avoid nested folder
        QApplication::setApplicationVersion("2.0.0");
        
        // Command line parser
        QCommandLineParser parser;
        parser.setApplicationDescription("Rats Search - BitTorrent P2P Search Engine");
        parser.addHelpOption();
        parser.addVersionOption();
        
        QCommandLineOption portOption(QStringList() << "p" << "port",
            "P2P listen port (default: 8080)", "port", "8080");
        parser.addOption(portOption);
        
        QCommandLineOption dhtPortOption(QStringList() << "d" << "dht-port",
            "DHT port (default: 6881)", "dht-port", "6881");
        parser.addOption(dhtPortOption);
        
        QCommandLineOption dataDirectoryOption(QStringList() << "data-dir",
            "Data directory for database and config", "path");
        parser.addOption(dataDirectoryOption);
        
        parser.process(app);
        
        // Get command line options
        int p2pPort = parser.value(portOption).toInt();
        int dhtPort = parser.value(dhtPortOption).toInt();
        
        QString dataDir;
        if (parser.isSet(dataDirectoryOption)) {
            dataDir = parser.value(dataDirectoryOption);
        } else {
            // Use standard location
            dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        }
        
        // Create data directory if it doesn't exist
        QDir dir;
        if (!dir.mkpath(dataDir)) {
            qCritical() << "Failed to create data directory:" << dataDir;
            return 1;
        }
        
        // Enable file logging
        auto& logger = librats::Logger::getInstance();
        QString logFilePath = dataDir + "/rats-search.log";
        logger.set_log_file_path(logFilePath.toStdString());
        logger.set_log_rotation_size(0);  // No rotation
        logger.set_file_logging_enabled(true);
        logger.set_log_level(librats::LogLevel::DEBUG);  // Log everything to file
        
        qInfo() << "Rats Search starting...";
        qInfo() << "Data directory:" << dataDir;
        qInfo() << "Log file:" << logFilePath;
        qInfo() << "P2P port:" << p2pPort;
        qInfo() << "DHT port:" << dhtPort;
        
        // Create main window (UI setup only, services deferred)
        qint64 windowStart = startupTimer.elapsed();
        MainWindow mainWindow(p2pPort, dhtPort, dataDir);
        qInfo() << "MainWindow created:" << (startupTimer.elapsed() - windowStart) << "ms";
        
        // Show window immediately (services start in background)
        mainWindow.show();
        qInfo() << "Window shown, total startup:" << startupTimer.elapsed() << "ms";
        qInfo() << "Heavy initialization continues in background...";
        
        return app.exec();
    }
}
