#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <csignal>
#include <iostream>
#include <memory>

#include "app/application.h"
#include "app/config_store.h"
#include "bootstrap/legacymigration.h"
#include "bootstrap/startupinfo.h"
#include "mainwindow.h"
#include "util/logger.h"
#include "version.h"

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// Data directory persistence (QSettings).
//
// Chicken-and-egg: the config lives inside the data directory, but we need to
// know the data directory before we can read config. So the chosen path is
// stored separately in QSettings.
// ============================================================================
static QString getSavedDataDirectory()
{
    return QSettings(QStringLiteral("RatsSearch"), QStringLiteral("RatsSearch"))
        .value(QStringLiteral("dataDirectory"))
        .toString();
}

static void saveDataDirectory(const QString& path)
{
    QSettings(QStringLiteral("RatsSearch"), QStringLiteral("RatsSearch"))
        .setValue(QStringLiteral("dataDirectory"), path);
}

#if defined(_WIN32) && !defined(NDEBUG)
// Attach a console on Windows so stdout/stderr are visible in Debug builds.
static void attachConsoleOnWindows()
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);
    std::ios::sync_with_stdio();
}
#endif

// Route Qt logging through the librats logger (single log file/sink).
static void customMessageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    const std::string message = msg.toLocal8Bit().constData();
    auto& logger = librats::Logger::getInstance();
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
    }
}

// ---- graceful Ctrl+C shutdown (console) ------------------------------------
static QCoreApplication* g_app = nullptr;
static bool g_shutdownRequested = false;

static void signalHandler(int)
{
    if (g_shutdownRequested)
        std::exit(1);
    g_shutdownRequested = true;
    std::cout << "\nShutting down..." << std::endl;
    if (g_app)
        QMetaObject::invokeMethod(g_app, []() { QCoreApplication::quit(); }, Qt::QueuedConnection);
}

// ============================================================================
// Shared startup, used identically by console and GUI modes.
// ============================================================================
static void addCommonOptions(QCommandLineParser& parser, QCommandLineOption& port, QCommandLineOption& dhtPort,
    QCommandLineOption& dataDir, QCommandLineOption& maxPeers, QCommandLineOption& spider, QCommandLineOption& console)
{
    parser.setApplicationDescription(QStringLiteral("Rats Search - BitTorrent P2P Search Engine"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(console);
    parser.addOption(port);
    parser.addOption(dhtPort);
    parser.addOption(dataDir);
    parser.addOption(maxPeers);
    parser.addOption(spider);
}

static QString resolveDataDirectory(QCommandLineParser& parser, const QCommandLineOption& dataDirOption)
{
    QString dataDir;
    if (parser.isSet(dataDirOption)) {
        dataDir = parser.value(dataDirOption);
        saveDataDirectory(dataDir); // remember an explicit override for next launch
    } else {
        dataDir = getSavedDataDirectory();
        if (dataDir.isEmpty())
            dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    return dataDir;
}

static void configureLogging(const QString& dataDir)
{
    auto& logger = librats::Logger::getInstance();
    const QString logFilePath = dataDir + QStringLiteral("/rats-search.log");
    logger.set_log_file_path(logFilePath.toStdString());
    logger.set_log_rotation_size(0);
    logger.set_log_retention_count(2);
    logger.set_rotate_on_startup(true); // must precede set_file_logging_enabled()
    logger.set_file_logging_enabled(true);
#ifdef NDEBUG
    logger.set_log_level(librats::LogLevel::INFO);
#else
    logger.set_log_level(librats::LogLevel::DEBUG);
#endif
    qInfo() << "Log file:" << logFilePath;
    logStartupInfo(dataDir);
}

// Console mode on the new rats:: architecture: build the composition root,
// start it, and run the event loop until interrupted. No widgets.
static int runConsoleApplication(QCoreApplication& app, rats::app::Application::Options options)
{
    options.headless = true;
    g_app = &app;
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    auto application = std::make_unique<rats::app::Application>(std::move(options));
    if (!application->start()) {
        qCritical() << "Failed to start application";
        return 1;
    }
    qInfo() << "Rats Search (console) running. Press Ctrl+C to quit.";
    QObject::connect(&app, &QCoreApplication::aboutToQuit, application.get(), [a = application.get()]() { a->stop(); });
    return app.exec();
}

int main(int argc, char* argv[])
{
#if defined(_WIN32) && !defined(NDEBUG)
    attachConsoleOnWindows();
#endif
    qInstallMessageHandler(customMessageHandler);

    // QApplication vs QCoreApplication must be chosen before construction, so we
    // pre-scan argv for the console flag.
    bool consoleMode = false;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QLatin1String("--console") || arg == QLatin1String("-c")) {
            consoleMode = true;
            break;
        }
    }

    std::unique_ptr<QCoreApplication> qapp;
    if (consoleMode)
        qapp = std::make_unique<QCoreApplication>(argc, argv);
    else
        qapp = std::make_unique<QApplication>(argc, argv);

    QCoreApplication::setApplicationName(QStringLiteral("Rats Search"));
    QCoreApplication::setOrganizationName(QString()); // empty avoids a nested folder
    QCoreApplication::setApplicationVersion(QStringLiteral(RATSSEARCH_VERSION_STRING));

    QCommandLineOption consoleOption(QStringList() << "c" << "console", QStringLiteral("Run without a GUI"));
    QCommandLineOption portOption(QStringList() << "p" << "port", QStringLiteral("P2P listen port"), "port");
    QCommandLineOption dhtPortOption(QStringList() << "d" << "dht-port", QStringLiteral("DHT port"), "dht-port");
    QCommandLineOption dataDirOption(QStringList() << "data-dir", QStringLiteral("Data directory"), "path");
    QCommandLineOption maxPeersOption(QStringList() << "m" << "max-peers", QStringLiteral("Max P2P connections"), "n");
    QCommandLineOption spiderOption(QStringList() << "s" << "spider", QStringLiteral("Force-enable the DHT spider"));
    QCommandLineParser parser;
    addCommonOptions(parser, portOption, dhtPortOption, dataDirOption, maxPeersOption, spiderOption, consoleOption);
    parser.process(*qapp);

    const QString dataDir = resolveDataDirectory(parser, dataDirOption);
    if (!QDir().mkpath(dataDir)) {
        qCritical() << "Failed to create data directory:" << dataDir;
        return 1;
    }
    migrateLegacyDatabase(dataDir); // one-time v1.x -> v2.0 import
    configureLogging(dataDir);

    rats::app::Application::Options options;
    options.dataDirectory = dataDir;
    options.clientVersion = QCoreApplication::applicationVersion();
    options.p2pPort = parser.isSet(portOption) ? parser.value(portOption).toInt() : 0;
    options.dhtPort = parser.isSet(dhtPortOption) ? parser.value(dhtPortOption).toInt() : 0;
    options.maxPeers = parser.isSet(maxPeersOption) ? parser.value(maxPeersOption).toInt() : 0;
    options.forceSpider = parser.isSet(spiderOption);

    if (consoleMode)
        return runConsoleApplication(*qapp, std::move(options));

    // ---- GUI mode --------------------------------------------------------
    auto application = std::make_unique<rats::app::Application>(std::move(options));
    if (!application->start()) {
        qCritical() << "Failed to start application";
        return 1;
    }

    MainWindow window(application.get());
    if (!application->config()->startMinimized())
        window.show();

    const int rc = qapp->exec();
    application->stop();
    return rc;
}
