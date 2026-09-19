#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

#include <csignal>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include "app/application.h"
#include "app/config_store.h"
#include "bootstrap/legacymigration.h"
#include "bootstrap/single_instance.h"
#include "bootstrap/startupinfo.h"
#include "common/logging.h"
#include "librats/util/logger.h"
#include "mainwindow.h"
#include "migrationprogresswindow.h"
#include "services/migration_service.h"
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
// Parse an on/off style CLI value. Returns nullopt for anything unrecognised so
// the caller can reject it loudly instead of silently picking a default.
static std::optional<bool> parseBoolOption(const QString& value)
{
    const QString v = value.trimmed().toLower();
    if (v == QLatin1String("on") || v == QLatin1String("true") || v == QLatin1String("yes") || v == QLatin1String("1"))
        return true;
    if (v == QLatin1String("off") || v == QLatin1String("false") || v == QLatin1String("no") || v == QLatin1String("0"))
        return false;
    return std::nullopt;
}

static void addCommonOptions(QCommandLineParser& parser, QCommandLineOption& port, QCommandLineOption& dhtPort,
    QCommandLineOption& dataDir, QCommandLineOption& maxPeers, QCommandLineOption& spider, QCommandLineOption& console,
    QCommandLineOption& webuiDir, QCommandLineOption& shareDb)
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
    parser.addOption(webuiDir);
    parser.addOption(shareDb);
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
    // Bounded from the first line on: config lives inside the data directory and
    // is not loaded yet, so start on the default budget. Application::applyConfig()
    // re-applies the stored logMaxSizeMb moments later, and on every later change.
    rats::common::applyLogSizeBudget(rats::common::kDefaultLogMaxSizeMb);
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

// Print the blocking pre-start migrations to stdout. They run inside
// Application::start() and can take minutes on a large index; a daemon that
// prints nothing for that long looks wedged. Only the sync migrations are drawn
// — the background ones report through the same signal, but they run while the
// daemon is serving and would scribble over its regular output.
static void reportMigrationsToConsole(rats::app::Application* application)
{
    auto* migrations = application->migrations();
    if (!migrations)
        return;

    struct DrawState {
        bool active = false;
        int lastPercent = -1;
    };
    auto state = std::make_shared<DrawState>();

    QObject::connect(migrations, &rats::service::MigrationService::syncMigrationStarted, application,
        [state](const QString&, const QString& description) {
            state->active = true;
            state->lastPercent = -1;
            std::cout << "Data migration: " << description.toStdString() << std::endl;
        });

    QObject::connect(migrations, &rats::service::MigrationService::migrationProgress, application,
        [state](const QString&, qint64 current, qint64 total) {
            if (!state->active || total <= 0)
                return;
            const int percent = static_cast<int>((qMin(current, total) * 100) / total);
            if (percent == state->lastPercent)
                return;
            state->lastPercent = percent;
            constexpr int kBarWidth = 30;
            const int filled = percent * kBarWidth / 100;
            std::cout << "\r  [" << std::string(filled, '#') << std::string(kBarWidth - filled, '.') << "] " << percent
                      << "%  " << current << '/' << total << "   " << std::flush;
        });

    QObject::connect(migrations, &rats::service::MigrationService::syncMigrationsFinished, application, [state]() {
        if (state->lastPercent >= 0)
            std::cout << std::endl;
        state->active = false;
        std::cout << "Data migration finished." << std::endl;
    });
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
    reportMigrationsToConsole(application.get());
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
    QCommandLineOption webuiDirOption(QStringList() << "w" << "webui-dir", QStringLiteral("Web UI directory"), "path");
    QCommandLineOption shareDbOption(QStringList() << "share-db",
        QStringLiteral("Serve the whole database to peers that ask: on|off "
                       "(overrides the databaseSharing config key for this run)"),
        "on|off");
    QCommandLineParser parser;
    addCommonOptions(parser, portOption, dhtPortOption, dataDirOption, maxPeersOption, spiderOption, consoleOption,
        webuiDirOption, shareDbOption);
    parser.process(*qapp);

    const QString dataDir = resolveDataDirectory(parser, dataDirOption);
    if (!QDir().mkpath(dataDir)) {
        qCritical() << "Failed to create data directory:" << dataDir;
        return 1;
    }
    // Refuse to run twice on one data directory (shared Manticore, RT tables and
    // rats.json). Must precede configureLogging(): log rotation on startup would
    // otherwise roll the running instance's log file out from under it.
    rats::bootstrap::SingleInstanceGuard instanceGuard(dataDir);
    if (!instanceGuard.tryAcquire()) {
        const QString holder = instanceGuard.runningInstanceInfo();
        qInfo().noquote() << QStringLiteral("Rats Search is already running on %1%2 - activating it")
                                 .arg(dataDir, holder.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(holder));
        if (!instanceGuard.notifyRunningInstance())
            qWarning() << "Could not reach the running instance (it may still be starting up)";
        return 0;
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
    options.webuiDir = parser.isSet(webuiDirOption) ? parser.value(webuiDirOption) : dataDir + "/webui";
    if (parser.isSet(shareDbOption)) {
        options.shareDatabase = parseBoolOption(parser.value(shareDbOption));
        if (!options.shareDatabase) {
            qCritical() << "Invalid --share-db value:" << parser.value(shareDbOption) << "- expected on or off";
            return 1;
        }
    }

    if (consoleMode)
        return runConsoleApplication(*qapp, std::move(options));

    // ---- GUI mode --------------------------------------------------------
    auto application = std::make_unique<rats::app::Application>(std::move(options));
    // Blocking pre-start migrations run inside start(), before MainWindow exists
    // and before the event loop is entered — the splash draws itself from inside
    // that call, and stays hidden when there is no migration to run.
    MigrationProgressWindow migrationSplash(application->migrations(), application->config()->darkMode());
    if (!application->start()) {
        qCritical() << "Failed to start application";
        return 1;
    }

    MainWindow window(application.get());
    // A second launch is the user asking for the window, even when this instance
    // is sitting minimized in the tray.
    QObject::connect(&instanceGuard, &rats::bootstrap::SingleInstanceGuard::secondInstanceStarted, &window,
        &MainWindow::bringToFront);
    if (!application->config()->startMinimized())
        window.show();

    const int rc = qapp->exec();
    application->stop();
    return rc;
}
