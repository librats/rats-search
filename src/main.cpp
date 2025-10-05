#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <iostream>
#include "mainwindow.h"
#include "librats/src/logger.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

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

// Custom Qt message handler for stdout logging
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context);
    QByteArray localMsg = msg.toLocal8Bit();
    
    std::string typeStr;
    switch (type) {
    case QtDebugMsg:
        typeStr = "[DEBUG]";
        break;
    case QtInfoMsg:
        typeStr = "[INFO ]";
        break;
    case QtWarningMsg:
        typeStr = "[WARN ]";
        break;
    case QtCriticalMsg:
        typeStr = "[ERROR]";
        break;
    case QtFatalMsg:
        typeStr = "[FATAL]";
        break;
    }
    
    // Output to stdout/stderr
    if (type == QtFatalMsg || type == QtCriticalMsg) {
        std::cerr << typeStr << " [Qt] " << localMsg.constData() << std::endl;
        std::cerr.flush();
    } else {
        std::cout << typeStr << " [Qt] " << localMsg.constData() << std::endl;
        std::cout.flush();
    }
    
    // Abort on fatal
    if (type == QtFatalMsg) {
        abort();
    }
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    // Attach console on Windows to see stdout/stderr
    attachConsoleOnWindows();
#endif
    
    // Install custom message handler for Qt logging
    qInstallMessageHandler(customMessageHandler);
    
    QApplication app(argc, argv);
    
    // Set application information
    QApplication::setApplicationName("Rats Search");
    QApplication::setOrganizationName("Rats Search");
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
    
    qInfo() << "Rats Search starting...";
    qInfo() << "Data directory:" << dataDir;
    qInfo() << "P2P port:" << p2pPort;
    qInfo() << "DHT port:" << dhtPort;
    
    // Create and show main window
    MainWindow mainWindow(p2pPort, dhtPort, dataDir);
    mainWindow.show();
    
    return app.exec();
}

