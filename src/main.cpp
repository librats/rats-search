#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
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

