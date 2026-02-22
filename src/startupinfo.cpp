#include "startupinfo.h"
#include "librats/src/os.h"
#include "version.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <QSysInfo>
#include <QCoreApplication>

// Helper: human-readable size
static QString humanSize(qint64 bytes)
{
    if (bytes < 0) return "N/A";
    
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = bytes;
    int i = 0;
    while (size >= 1024.0 && i < 4) {
        size /= 1024.0;
        ++i;
    }
    return QString("%1 %2").arg(size, 0, 'f', i == 0 ? 0 : 2).arg(units[i]);
}

// Calculate total size of a directory (non-recursive by default, recursive optionally)
static qint64 directorySize(const QString& path, bool recursive = true)
{
    qint64 total = 0;
    QDir dir(path);
    if (!dir.exists()) return 0;
    
    QDir::Filters filters = QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden;
    
    for (const QFileInfo& fi : dir.entryInfoList(filters)) {
        total += fi.size();
    }
    
    if (recursive) {
        for (const QString& sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
            total += directorySize(path + "/" + sub, true);
        }
    }
    
    return total;
}

void logStartupInfo(const QString& dataDirectory)
{
    qInfo() << "========== Startup Debug Info ==========";
    
    // ---- Application info ----
    qInfo() << "[App] Version:" << RATSSEARCH_VERSION_STRING
            << "(" << RATSSEARCH_GIT_DESCRIBE << ")";
    qInfo() << "[App] Qt version:" << qVersion();
    qInfo() << "[App] Executable:" << QCoreApplication::applicationFilePath();
    qInfo() << "[App] PID:" << QCoreApplication::applicationPid();
    
    // ---- System info via librats ----
    auto sysInfo = librats::get_system_info();
    
    qInfo() << "[System] OS:" << QString::fromStdString(sysInfo.os_name)
            << QString::fromStdString(sysInfo.os_version);
    qInfo() << "[System] Architecture:" << QString::fromStdString(sysInfo.architecture);
    qInfo() << "[System] Hostname:" << QString::fromStdString(sysInfo.hostname);
    qInfo() << "[System] CPU:" << QString::fromStdString(sysInfo.cpu_model)
            << "| Cores:" << sysInfo.cpu_cores
            << "| Logical:" << sysInfo.cpu_logical_cores;
    qInfo() << "[System] Memory: total" << sysInfo.total_memory_mb << "MB"
            << "| available" << sysInfo.available_memory_mb << "MB";
    
    // ---- Data directory info ----
    qInfo() << "[Data] Directory:" << dataDirectory;
    
    QDir dataDir(dataDirectory);
    if (dataDir.exists()) {
        QStorageInfo storage(dataDirectory);
        if (storage.isValid()) {
            qInfo() << "[Data] Volume:" << storage.rootPath()
                    << "| Free:" << humanSize(storage.bytesFree())
                    << "| Total:" << humanSize(storage.bytesTotal());
        }
        
        qint64 dataDirSize = directorySize(dataDirectory);
        qInfo() << "[Data] Data dir size:" << humanSize(dataDirSize);
    } else {
        qInfo() << "[Data] Directory does not exist yet (will be created)";
    }
    
    // ---- Database files info ----
    QString dbPath = dataDirectory + "/database";
    QDir dbDir(dbPath);
    if (dbDir.exists()) {
        qint64 dbSize = directorySize(dbPath);
        qInfo() << "[Database] Path:" << dbPath;
        qInfo() << "[Database] Total size:" << humanSize(dbSize);
        
        // Report sizes of individual index directories
        for (const QString& sub : dbDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            qint64 subSize = directorySize(dbPath + "/" + sub);
            qInfo() << "[Database] Index" << sub << ":" << humanSize(subSize);
        }
    } else {
        qInfo() << "[Database] No database directory yet (first run?)";
    }
    
    // ---- Key config/log files ----
    auto logFileSize = [&](const QString& name, const QString& path) {
        QFileInfo fi(path);
        if (fi.exists()) {
            qInfo() << "[Files]" << name << ":" << humanSize(fi.size())
                    << "| Modified:" << fi.lastModified().toString(Qt::ISODate);
        }
    };
    
    logFileSize("Config", dataDirectory + "/rats.json");
    logFileSize("Sphinx conf", dataDirectory + "/sphinx.conf");
    logFileSize("searchd.log", dataDirectory + "/searchd.log");
    logFileSize("query.log", dataDirectory + "/query.log");
    logFileSize("rats-search.log", dataDirectory + "/rats-search.log");
    
    qInfo() << "========================================";
}
