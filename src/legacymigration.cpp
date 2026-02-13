#include "legacymigration.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

/**
 * Get possible legacy (Electron v1.x) data directory paths per platform.
 */
static QStringList getLegacyDataDirectories()
{
    QStringList paths;
    
    // Electron v1.x used productName "Rats on The Boat" from package.json
    // for app.getPath("userData"), so the directory is "Rats on The Boat".
    // Also check "rats-search" (the package name) as a fallback.
    
#ifdef Q_OS_WIN
    // Windows: Electron used %APPDATA% (Roaming), Qt uses %LOCALAPPDATA% (Local)
    QString roaming = QDir::homePath() + "/AppData/Roaming";
    paths << roaming + "/Rats on The Boat";
    paths << roaming + "/rats-search";
#elif defined(Q_OS_MACOS)
    // macOS: ~/Library/Application Support/<name>
    QString appSupport = QDir::homePath() + "/Library/Application Support";
    paths << appSupport + "/Rats on The Boat";
    paths << appSupport + "/rats-search";
#else
    // Linux: Electron used ~/.config/<name>
    QString config = QDir::homePath() + "/.config";
    paths << config + "/Rats on The Boat";
    paths << config + "/rats-search";
#endif
    
    return paths;
}

bool migrateLegacyDatabase(const QString& newDataDir)
{
    // Check if new data directory already has a database - no migration needed
    QDir newDbDir(newDataDir + "/database");
    if (newDbDir.exists()) {
        QStringList files = newDbDir.entryList(QDir::Files);
        if (!files.isEmpty()) {
            qInfo() << "New database directory already exists, no migration needed";
            return false;  // New database already has files
        }
    }
    
    // Collect all possible legacy directories
    QStringList legacyPaths = getLegacyDataDirectories();
    
    // Also check if any legacy rats.json has a custom dbPath
    for (const QString& legacyDir : QStringList(legacyPaths)) {  // iterate copy
        QString legacyConfig = legacyDir + "/rats.json";
        if (QFile::exists(legacyConfig)) {
            QFile configFile(legacyConfig);
            if (configFile.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
                configFile.close();
                QString dbPath = doc.object()["dbPath"].toString();
                if (!dbPath.isEmpty() && dbPath != legacyDir && !legacyPaths.contains(dbPath)) {
                    legacyPaths << dbPath;
                }
            }
        }
    }
    
    // Try to find a legacy directory with an actual database
    for (const QString& legacyDir : legacyPaths) {
        QDir legacyDbDir(legacyDir + "/database");
        if (!legacyDbDir.exists()) {
            continue;
        }
        
        QStringList dbFiles = legacyDbDir.entryList(QDir::Files);
        if (dbFiles.isEmpty()) {
            continue;
        }
        
        qInfo() << "=== Legacy Database Migration ===";
        qInfo() << "Found legacy v1.x database at:" << legacyDir;
        qInfo() << "Database files:" << dbFiles.size();
        qInfo() << "Migrating to new data directory:" << newDataDir;
        
        // Ensure new database directory exists
        QDir newDir(newDataDir);
        if (!newDir.mkpath("database")) {
            qWarning() << "Failed to create new database directory:" << (newDataDir + "/database");
            return false;
        }
        
        // Copy database files
        int filesCopied = 0;
        int filesFailed = 0;
        qint64 totalBytes = 0;
        
        for (const QFileInfo& fileInfo : legacyDbDir.entryInfoList(QDir::Files)) {
            QString destPath = newDataDir + "/database/" + fileInfo.fileName();
            if (QFile::copy(fileInfo.absoluteFilePath(), destPath)) {
                filesCopied++;
                totalBytes += fileInfo.size();
            } else {
                qWarning() << "Failed to copy database file:" << fileInfo.fileName();
                filesFailed++;
            }
        }
        
        qInfo() << "Copied" << filesCopied << "database files (" 
                << (totalBytes / (1024 * 1024)) << "MB),"
                << filesFailed << "failed";
        
        // Copy rats.json config if exists in legacy dir and not yet in new dir
        QString legacyConfig = legacyDir + "/rats.json";
        QString newConfig = newDataDir + "/rats.json";
        if (QFile::exists(legacyConfig) && !QFile::exists(newConfig)) {
            if (QFile::copy(legacyConfig, newConfig)) {
                qInfo() << "Copied legacy config (rats.json)";
            } else {
                qWarning() << "Failed to copy legacy config";
            }
        }
        
        // Copy binlog files (important for Manticore data consistency)
        QDir legacyRootDir(legacyDir);
        int binlogsCopied = 0;
        for (const QFileInfo& fileInfo : legacyRootDir.entryInfoList(QDir::Files)) {
            if (fileInfo.fileName().startsWith("binlog.")) {
                QString destPath = newDataDir + "/" + fileInfo.fileName();
                if (!QFile::exists(destPath)) {
                    if (QFile::copy(fileInfo.absoluteFilePath(), destPath)) {
                        binlogsCopied++;
                    }
                }
            }
        }
        if (binlogsCopied > 0) {
            qInfo() << "Copied" << binlogsCopied << "binlog files";
        }
        
        if (filesFailed == 0) {
            qInfo() << "Legacy database migration completed successfully!";
        } else {
            qWarning() << "Legacy database migration completed with" << filesFailed << "errors";
        }
        qInfo() << "=================================";
        
        return true;  // Migration attempted (found legacy data)
    }
    
    // No legacy database found
    return false;
}
