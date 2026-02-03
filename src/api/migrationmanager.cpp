#include "migrationmanager.h"
#include "../torrentdatabase.h"
#include "../sphinxql.h"
#include "configmanager.h"
#include "version.h"  // Generated in build directory

#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QDateTime>
#include <QDebug>

// ============================================================================
// Private Implementation
// ============================================================================

class MigrationManager::Private {
public:
    QString dataDirectory;
    QString stateFilePath;
    
    TorrentDatabase* database = nullptr;
    ConfigManager* config = nullptr;
    
    // State persistence
    QJsonObject stateJson;
    QStringList completedMigrations;
    MigrationState inProgressMigration;
    QString lastVersion;
    
    // Runtime state
    bool isRunning = false;
    bool stopRequested = false;
    bool isFreshInstall = false;  // True if first run (no existing data)
    QMutex stateMutex;
    
    // Current progress for UI
    Progress currentProgress;
    
    // Migration definitions
    struct MigrationDef {
        QString id;
        QString version;      // Minimum version that requires this migration
        QString description;
        bool isSync;          // true = blocking, false = background
    };
    QVector<MigrationDef> registeredMigrations;
    
    // Background worker
    QFuture<void> asyncFuture;
    QTimer* progressSaveTimer = nullptr;
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

MigrationManager::MigrationManager(const QString& dataDirectory, QObject* parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->dataDirectory = dataDirectory;
    d->stateFilePath = dataDirectory + "/migrations.json";
    
    // Create data directory if needed
    QDir dir(dataDirectory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Timer for periodic state saves during long migrations
    d->progressSaveTimer = new QTimer(this);
    d->progressSaveTimer->setInterval(5000);  // Save every 5 seconds
    connect(d->progressSaveTimer, &QTimer::timeout, this, &MigrationManager::saveState);
    
    loadState();
    registerMigrations();
}

MigrationManager::~MigrationManager()
{
    // Ensure we save state on destruction
    if (d->isRunning) {
        requestStop();
        
        // Wait briefly for async migration to stop
        if (d->asyncFuture.isRunning()) {
            d->asyncFuture.waitForFinished();
        }
    }
    
    saveState();
}

// ============================================================================
// Initialization
// ============================================================================

void MigrationManager::initialize(TorrentDatabase* database, ConfigManager* config)
{
    d->database = database;
    d->config = config;
    
    // Detect fresh install: no migration state file AND no data in database
    // Fresh install = no migrations needed (they're for upgrading existing data)
    bool hasStateFile = QFile::exists(d->stateFilePath);
    
    if (!hasStateFile) {
        // Fresh install - no previous version, no data to migrate
        d->isFreshInstall = true;
        qInfo() << "MigrationManager: Fresh install detected, skipping all migrations";
        
        // Mark all current migrations as completed so they never run
        for (const auto& migration : d->registeredMigrations) {
            if (!d->completedMigrations.contains(migration.id)) {
                d->completedMigrations.append(migration.id);
            }
        }
        
        // Save state to indicate we've initialized
        saveState();
    } else {
        d->isFreshInstall = false;
        qInfo() << "MigrationManager initialized, state file:" << d->stateFilePath;
        qInfo() << "Completed migrations:" << d->completedMigrations.size();
        // Check if there are any torrents in the database
        qint64 torrentCount = database->getTorrentCount();
        qInfo() << "MigrationManager: Database has" << torrentCount << "torrents";
        
        if (!d->inProgressMigration.migrationId.isEmpty()) {
            qInfo() << "Found interrupted migration:" << d->inProgressMigration.migrationId
                    << "last processed ID:" << d->inProgressMigration.lastProcessedId;
        }
    }
}

QString MigrationManager::currentVersion()
{
    return RATSSEARCH_VERSION_STRING;
}

// ============================================================================
// State Persistence
// ============================================================================

void MigrationManager::loadState()
{
    QFile file(d->stateFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qInfo() << "No migration state file found, starting fresh";
        d->stateJson = QJsonObject();
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse migration state:" << error.errorString();
        d->stateJson = QJsonObject();
        return;
    }
    
    d->stateJson = doc.object();
    
    // Load completed migrations
    QJsonArray completedArray = d->stateJson["completedMigrations"].toArray();
    for (const QJsonValue& val : completedArray) {
        d->completedMigrations.append(val.toString());
    }
    
    // Load in-progress migration (if any)
    if (d->stateJson.contains("inProgress")) {
        QJsonObject inProgress = d->stateJson["inProgress"].toObject();
        d->inProgressMigration.migrationId = inProgress["migrationId"].toString();
        d->inProgressMigration.version = inProgress["version"].toString();
        d->inProgressMigration.lastProcessedId = inProgress["lastProcessedId"].toVariant().toLongLong();
        d->inProgressMigration.totalItems = inProgress["totalItems"].toVariant().toLongLong();
        d->inProgressMigration.startedAt = inProgress["startedAt"].toVariant().toLongLong();
        d->inProgressMigration.completed = false;
    }
    
    d->lastVersion = d->stateJson["lastVersion"].toString();
    
    qInfo() << "Migration state loaded, last version:" << d->lastVersion
            << "completed:" << d->completedMigrations.size()
            << "in-progress:" << d->inProgressMigration.migrationId;
}

void MigrationManager::saveState()
{
    QMutexLocker locker(&d->stateMutex);
    
    QJsonObject state;
    
    // Save completed migrations
    QJsonArray completedArray;
    for (const QString& id : d->completedMigrations) {
        completedArray.append(id);
    }
    state["completedMigrations"] = completedArray;
    
    // Save in-progress migration
    if (!d->inProgressMigration.migrationId.isEmpty() && !d->inProgressMigration.completed) {
        QJsonObject inProgress;
        inProgress["migrationId"] = d->inProgressMigration.migrationId;
        inProgress["version"] = d->inProgressMigration.version;
        inProgress["lastProcessedId"] = d->inProgressMigration.lastProcessedId;
        inProgress["totalItems"] = d->inProgressMigration.totalItems;
        inProgress["startedAt"] = d->inProgressMigration.startedAt;
        state["inProgress"] = inProgress;
    }
    
    state["lastVersion"] = currentVersion();
    state["savedAt"] = QDateTime::currentMSecsSinceEpoch();
    
    d->stateJson = state;
    
    // Write to file
    QFile file(d->stateFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(state);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    } else {
        qWarning() << "Failed to save migration state:" << file.errorString();
    }
}

bool MigrationManager::isMigrationCompleted(const QString& migrationId) const
{
    return d->completedMigrations.contains(migrationId);
}

void MigrationManager::markMigrationCompleted(const QString& migrationId)
{
    QMutexLocker locker(&d->stateMutex);
    
    if (!d->completedMigrations.contains(migrationId)) {
        d->completedMigrations.append(migrationId);
    }
    
    // Clear in-progress if this was the running migration
    if (d->inProgressMigration.migrationId == migrationId) {
        d->inProgressMigration = MigrationState();
    }
    
    // Save immediately
    locker.unlock();
    saveState();
    
    qInfo() << "Migration completed:" << migrationId;
    emit migrationCompleted(migrationId);
}

// ============================================================================
// Migration Registration
// ============================================================================

void MigrationManager::registerMigrations()
{
    // Register all known migrations here
    // They will be run in order if not already completed
    
    // v2.0.1 - Recategorize all torrents (background, resumable)
    d->registeredMigrations.append({
        "v2.0.12_recategorize_torrents",
        "2.0.0",
        "Recategorize all torrents with improved content detection",
        false  // async/background
    });
    
    // Add future migrations here:
    // d->registeredMigrations.append({
    //     "v2.1.0_some_migration",
    //     "2.1.0",
    //     "Description of migration",
    //     true/false  // sync/async
    // });
}

// ============================================================================
// Sync Migrations
// ============================================================================

bool MigrationManager::runSyncMigrations()
{
    // Skip if fresh install - no data to migrate
    if (d->isFreshInstall) {
        qInfo() << "Fresh install - skipping sync migrations";
        return true;
    }
    
    qInfo() << "Running synchronous migrations...";
    
    bool allSuccess = true;
    
    for (const auto& migration : d->registeredMigrations) {
        if (!migration.isSync) {
            continue;  // Skip async migrations
        }
        
        if (isMigrationCompleted(migration.id)) {
            qInfo() << "Sync migration already completed:" << migration.id;
            continue;
        }
        
        qInfo() << "Running sync migration:" << migration.id << "-" << migration.description;
        
        bool success = false;
        
        // Dispatch to specific migration implementation
        if (migration.id == "v2.0.12_sync_placeholder") {
            success = migration_v2_0_12_sync_placeholder();
        }
        // Add more sync migrations here
        
        if (success) {
            markMigrationCompleted(migration.id);
        } else {
            qWarning() << "Sync migration FAILED:" << migration.id;
            allSuccess = false;
            emit migrationError(migration.id, "Migration failed");
            break;  // Stop on first failure for sync migrations
        }
    }
    
    if (allSuccess) {
        qInfo() << "All sync migrations completed successfully";
    }
    
    return allSuccess;
}

bool MigrationManager::migration_v2_0_12_sync_placeholder()
{
    // Placeholder for sync migration - always succeeds
    // Add actual sync migration logic here when needed
    return true;
}

// ============================================================================
// Async Migrations
// ============================================================================

void MigrationManager::startAsyncMigrations()
{
    // Skip if fresh install - no data to migrate
    if (d->isFreshInstall) {
        qInfo() << "Fresh install - skipping async migrations";
        emit allMigrationsCompleted();
        return;
    }
    
    if (d->isRunning) {
        qWarning() << "Async migrations already running";
        return;
    }
    
    // Check for interrupted migration first
    if (!d->inProgressMigration.migrationId.isEmpty()) {
        qInfo() << "Resuming interrupted migration:" << d->inProgressMigration.migrationId
                << "from ID:" << d->inProgressMigration.lastProcessedId;
    }
    
    // Find pending async migrations
    QVector<Private::MigrationDef> pendingMigrations;
    
    for (const auto& migration : d->registeredMigrations) {
        if (migration.isSync) {
            continue;  // Skip sync migrations
        }
        
        if (isMigrationCompleted(migration.id)) {
            qInfo() << "Async migration already completed:" << migration.id;
            continue;
        }
        
        pendingMigrations.append(migration);
    }
    
    if (pendingMigrations.isEmpty()) {
        qInfo() << "No pending async migrations";
        emit allMigrationsCompleted();
        return;
    }
    
    d->isRunning = true;
    d->stopRequested = false;
    d->progressSaveTimer->start();
    
    // Run migrations in order
    d->asyncFuture = QtConcurrent::run([this, pendingMigrations]() {
        for (const auto& migration : pendingMigrations) {
            if (d->stopRequested) {
                qInfo() << "Migration stop requested, breaking";
                break;
            }
            
            qInfo() << "Starting async migration:" << migration.id << "-" << migration.description;
            
            // Set up in-progress state if this is a fresh start
            if (d->inProgressMigration.migrationId != migration.id) {
                QMutexLocker locker(&d->stateMutex);
                d->inProgressMigration.migrationId = migration.id;
                d->inProgressMigration.version = migration.version;
                d->inProgressMigration.lastProcessedId = 0;
                d->inProgressMigration.totalItems = 0;
                d->inProgressMigration.startedAt = QDateTime::currentMSecsSinceEpoch();
                d->inProgressMigration.completed = false;
            }
            
            // Update current progress for UI
            d->currentProgress.migrationId = migration.id;
            d->currentProgress.description = migration.description;
            d->currentProgress.isRunning = true;
            
            // Dispatch to specific migration implementation
            if (migration.id == "v2.0.12_recategorize_torrents") {
                migration_v2_0_12_recategorize_torrents();
            }
            // Add more async migrations here
            
            if (!d->stopRequested) {
                markMigrationCompleted(migration.id);
            }
        }
        
        // Cleanup
        QMetaObject::invokeMethod(this, [this]() {
            d->isRunning = false;
            d->progressSaveTimer->stop();
            d->currentProgress.isRunning = false;
            
            if (!d->stopRequested) {
                qInfo() << "All async migrations completed";
                emit allMigrationsCompleted();
            } else {
                qInfo() << "Async migrations stopped (will resume on next startup)";
                saveState();
            }
        }, Qt::QueuedConnection);
    });
}

bool MigrationManager::isRunning() const
{
    return d->isRunning;
}

void MigrationManager::requestStop()
{
    if (!d->isRunning) {
        return;
    }
    
    qInfo() << "Requesting migration stop...";
    d->stopRequested = true;
    
    // Save current state immediately
    saveState();
}

MigrationManager::Progress MigrationManager::currentProgress() const
{
    return d->currentProgress;
}

// ============================================================================
// Migration: Recategorize All Torrents
// ============================================================================

void MigrationManager::migration_v2_0_12_recategorize_torrents()
{
    if (!d->database) {
        qWarning() << "Migration: Database not available";
        return;
    }
    
    qInfo() << "Migration: Starting torrent recategorization...";
    
    // Get starting point (for resume support)
    qint64 startId = d->inProgressMigration.lastProcessedId;
    
    // Get total count for progress (only on fresh start)
    if (d->inProgressMigration.totalItems == 0) {
        SphinxQL::Results countResult = d->database->sphinxQL()->query(
            "SELECT COUNT(*) as cnt FROM torrents");
        
        if (!countResult.isEmpty()) {
            d->inProgressMigration.totalItems = countResult.first()["cnt"].toLongLong();
        }
        
        qInfo() << "Migration: Total torrents to process:" << d->inProgressMigration.totalItems;
    }
    
    const int batchSize = 100;  // Process in batches
    qint64 processedCount = startId > 0 ? startId : 0;  // Approximate starting count
    qint64 lastId = startId;
    int updatedCount = 0;
    
    while (!d->stopRequested) {
        // Query batch of torrents with files
        QString querySql = QString(
            "SELECT id, hash, name, size, files, piecelength, contenttype, contentcategory "
            "FROM torrents WHERE id > %1 ORDER BY id ASC LIMIT %2")
            .arg(lastId).arg(batchSize);
        
        SphinxQL::Results rows = d->database->sphinxQL()->query(querySql);
        
        if (rows.isEmpty()) {
            qInfo() << "Migration: No more torrents to process";
            break;
        }
        
        for (const QVariantMap& row : rows) {
            if (d->stopRequested) {
                break;
            }
            
            qint64 id = row["id"].toLongLong();
            QString hash = row["hash"].toString();
            
            // Get files for this torrent
            QVector<TorrentFile> files = d->database->getFilesForTorrent(hash);
            
            if (!files.isEmpty()) {
                // Build TorrentInfo for content detection
                TorrentInfo torrent;
                torrent.id = id;
                torrent.hash = hash;
                torrent.name = row["name"].toString();
                torrent.size = row["size"].toLongLong();
                torrent.files = row["files"].toInt();
                torrent.piecelength = row["piecelength"].toInt();
                torrent.contentTypeId = row["contenttype"].toInt();
                torrent.contentCategoryId = row["contentcategory"].toInt();
                torrent.filesList = files;
                
                // Store old values
                int oldTypeId = torrent.contentTypeId;
                int oldCategoryId = torrent.contentCategoryId;
                
                // Re-detect content type
                TorrentDatabase::detectContentType(torrent);
                
                // Update if changed
                if (torrent.contentTypeId != oldTypeId || torrent.contentCategoryId != oldCategoryId) {
                    d->database->updateTorrentContentType(hash, torrent.contentTypeId, torrent.contentCategoryId);
                    updatedCount++;
                    
                    if (updatedCount % 100 == 0) {
                        qInfo() << "Migration: Updated" << updatedCount << "torrents so far";
                    }
                }
            }
            
            lastId = id;
            processedCount++;
            
            // Update progress
            {
                QMutexLocker locker(&d->stateMutex);
                d->inProgressMigration.lastProcessedId = lastId;
                d->currentProgress.current = processedCount;
                d->currentProgress.total = d->inProgressMigration.totalItems;
            }
        }
        
        // Emit progress update
        emit migrationProgress(d->inProgressMigration.migrationId, 
                               processedCount, d->inProgressMigration.totalItems);
        
        // Small delay to avoid overwhelming the system
        if (!d->stopRequested) {
            QThread::msleep(10);
        }
        
        // Check if we've processed all
        if (rows.size() < batchSize) {
            qInfo() << "Migration: Last batch processed";
            break;
        }
    }
    
    if (!d->stopRequested) {
        qInfo() << "Migration: Recategorization complete. Updated" << updatedCount 
                << "out of" << processedCount << "torrents";
    } else {
        qInfo() << "Migration: Stopped at ID" << lastId << ". Will resume on next startup.";
    }
}

void MigrationManager::recategorizeTorrentsBatch(qint64 startId, int batchSize)
{
    Q_UNUSED(startId);
    Q_UNUSED(batchSize);
    // This method is available for future use if we need to expose batch processing
}
