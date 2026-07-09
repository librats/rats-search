#include "services/migration_service.h"

#include "app/config_store.h"
#include "data/database.h"
#include "data/torrent_repository.h"
#include "domain/content.h"
#include "domain/content_classifier.h"
#include "domain/torrent.h"
#include "version.h" // Generated in build directory

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>
#include <QVersionNumber>
#include <QtConcurrent>

namespace rats::service {

// ============================================================================
// Construction
// ============================================================================

MigrationService::MigrationService(const QString& dataDirectory, data::Database* db,
    data::TorrentRepository* repository, app::ConfigStore* config, QObject* parent)
    : QObject(parent)
    , dataDirectory_(dataDirectory)
    , stateFilePath_(dataDirectory + "/migrations.json")
    , db_(db)
    , repository_(repository)
    , config_(config)
{
    QDir dir(dataDirectory_);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Periodic state saves during long-running migrations.
    progressSaveTimer_ = new QTimer(this);
    progressSaveTimer_->setInterval(5000);
    connect(progressSaveTimer_, &QTimer::timeout, this, &MigrationService::saveState);

    loadState();
    registerMigrations();

    // Detect a fresh install: no migration state file means there is no upgraded
    // data to migrate, so mark every known migration as completed up front.
    const bool hasStateFile = QFile::exists(stateFilePath_);
    if (!hasStateFile) {
        isFreshInstall_ = true;
        qInfo() << "[Migration] Fresh install detected, skipping all migrations";

        {
            QMutexLocker locker(&stateMutex_);
            for (const auto& migration : registeredMigrations_) {
                if (!completedMigrations_.contains(migration.id)) {
                    completedMigrations_.append(migration.id);
                }
            }
        }
        saveState();
    } else {
        isFreshInstall_ = false;
        qInfo() << "[Migration] initialized, state file:" << stateFilePath_;
        qInfo() << "[Migration] completed migrations:" << completedMigrations_.size();

        QMutexLocker locker(&stateMutex_);
        if (!inProgressMigration_.migrationId.isEmpty()) {
            qInfo() << "[Migration] found interrupted migration:" << inProgressMigration_.migrationId
                    << "last processed ID:" << inProgressMigration_.lastProcessedId;
        }
    }
}

MigrationService::~MigrationService()
{
    if (isRunning_.load()) {
        requestStop();
        if (asyncFuture_.isRunning()) {
            asyncFuture_.waitForFinished();
        }
    }
    saveState();
}

QString MigrationService::currentVersion()
{
    return RATSSEARCH_VERSION_STRING;
}

// ============================================================================
// Version gating
// ============================================================================

bool MigrationService::shouldRunMigration(const MigrationDef& migration) const
{
    const QVersionNumber currentVer = QVersionNumber::fromString(RATSSEARCH_VERSION_STRING);

    if (!migration.minVersion.isEmpty()) {
        const QVersionNumber minVer = QVersionNumber::fromString(migration.minVersion);
        if (currentVer < minVer) {
            return false;
        }
    }

    if (!migration.maxVersion.isEmpty()) {
        const QVersionNumber maxVer = QVersionNumber::fromString(migration.maxVersion);
        if (currentVer > maxVer) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// Registration
// ============================================================================

void MigrationService::registerMigrations()
{
    // The registry: one self-describing entry per migration, each carrying its
    // own body. Adding a migration is a single append — there is no separate
    // string-matched dispatch to keep in sync.
    //
    // Fields: {id, minVersion, maxVersion, description, isSync, syncBody,
    // asyncBody}
    //   - minVersion / maxVersion: inclusive app-version bounds (empty = no
    //   bound)
    //   - isSync: true = blocking (before start), false = background (resumable)

    // v2.0.12 - Cleanup feed storage (blocking).
    registeredMigrations_.append({
        "v2.0.12_sync_cleanup_feed_storage",
        "2.0.0",
        "",
        "Cleanup feed storage",
        true,
        [this] { return syncCleanupFeedStorage(); },
        nullptr,
    });

    // v2.0.12 - Recategorize all torrents with improved content detection
    // (background, resumable). Applies to 2.0.0 and above.
    registeredMigrations_.append({
        "v2.0.12_recategorize_torrents",
        "2.0.0",
        "",
        "Recategorize all torrents with improved content detection",
        false,
        nullptr,
        [this] { recategorizeTorrents(); },
    });

    // v2.0.12 - Remove all torrents with Unknown content type (background,
    // resumable). These failed content detection and are likely low quality.
    registeredMigrations_.append({
        "v2.0.12_remove_unknown_torrents",
        "2.0.0",
        "",
        "Remove torrents with Unknown content type",
        false,
        nullptr,
        [this] { removeUnknownTorrents(); },
    });

    // v2.0.19 - Update spider walkInterval to the new default (100ms). The old
    // default of 5ms was too aggressive; bump existing configs.
    registeredMigrations_.append({
        "v2.0.19_sync_update_walk_interval",
        "2.0.0",
        "",
        "Update spider walk interval to 100ms",
        true,
        [this] { return syncUpdateWalkInterval(); },
        nullptr,
    });
}

// ============================================================================
// State persistence
// ============================================================================

void MigrationService::loadState()
{
    QFile file(stateFilePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        qInfo() << "[Migration] no state file found, starting fresh";
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "[Migration] failed to parse state:" << error.errorString();
        return;
    }

    const QJsonObject stateJson = doc.object();

    QMutexLocker locker(&stateMutex_);

    const QJsonArray completedArray = stateJson["completedMigrations"].toArray();
    for (const QJsonValue& val : completedArray) {
        completedMigrations_.append(val.toString());
    }

    if (stateJson.contains("inProgress")) {
        const QJsonObject inProgress = stateJson["inProgress"].toObject();
        inProgressMigration_.migrationId = inProgress["migrationId"].toString();
        inProgressMigration_.minVersion = inProgress["minVersion"].toString();
        inProgressMigration_.maxVersion = inProgress["maxVersion"].toString();
        inProgressMigration_.lastProcessedId = inProgress["lastProcessedId"].toVariant().toLongLong();
        inProgressMigration_.totalItems = inProgress["totalItems"].toVariant().toLongLong();
        inProgressMigration_.startedAt = inProgress["startedAt"].toVariant().toLongLong();
        inProgressMigration_.completed = false;
    }

    lastVersion_ = stateJson["lastVersion"].toString();

    qInfo() << "[Migration] state loaded, last version:" << lastVersion_ << "completed:" << completedMigrations_.size()
            << "in-progress:" << inProgressMigration_.migrationId;
}

void MigrationService::saveState()
{
    QMutexLocker locker(&stateMutex_);

    QJsonObject state;

    QJsonArray completedArray;
    for (const QString& id : completedMigrations_) {
        completedArray.append(id);
    }
    state["completedMigrations"] = completedArray;

    if (!inProgressMigration_.migrationId.isEmpty() && !inProgressMigration_.completed) {
        QJsonObject inProgress;
        inProgress["migrationId"] = inProgressMigration_.migrationId;
        inProgress["minVersion"] = inProgressMigration_.minVersion;
        inProgress["maxVersion"] = inProgressMigration_.maxVersion;
        inProgress["lastProcessedId"] = inProgressMigration_.lastProcessedId;
        inProgress["totalItems"] = inProgressMigration_.totalItems;
        inProgress["startedAt"] = inProgressMigration_.startedAt;
        state["inProgress"] = inProgress;
    }

    state["lastVersion"] = RATSSEARCH_VERSION_STRING;
    state["savedAt"] = QDateTime::currentMSecsSinceEpoch();

    QFile file(stateFilePath_);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QJsonDocument doc(state);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    } else {
        qWarning() << "[Migration] failed to save state:" << file.errorString();
    }
}

bool MigrationService::isMigrationCompleted(const QString& migrationId) const
{
    QMutexLocker locker(&stateMutex_);
    return completedMigrations_.contains(migrationId);
}

void MigrationService::markMigrationCompleted(const QString& migrationId)
{
    {
        QMutexLocker locker(&stateMutex_);
        if (!completedMigrations_.contains(migrationId)) {
            completedMigrations_.append(migrationId);
        }
        // Clear the in-progress record if it was the migration that just finished.
        if (inProgressMigration_.migrationId == migrationId) {
            inProgressMigration_ = MigrationState();
        }
    }

    saveState();
    qInfo() << "[Migration] completed:" << migrationId;
}

// ============================================================================
// Sync migrations
// ============================================================================

bool MigrationService::runSyncMigrations()
{
    if (isFreshInstall_) {
        qInfo() << "[Migration] fresh install - skipping sync migrations";
        return true;
    }

    qInfo() << "[Migration] running synchronous migrations...";

    bool allSuccess = true;

    for (const auto& migration : registeredMigrations_) {
        if (!migration.isSync) {
            continue;
        }
        if (isMigrationCompleted(migration.id)) {
            qInfo() << "[Migration] sync already completed:" << migration.id;
            continue;
        }
        if (!shouldRunMigration(migration)) {
            qInfo() << "[Migration] sync skipped (version mismatch):" << migration.id
                    << "minVersion:" << migration.minVersion << "maxVersion:" << migration.maxVersion;
            continue;
        }

        qInfo() << "[Migration] running sync:" << migration.id << "-" << migration.description;

        const bool success = migration.syncBody ? migration.syncBody() : false;

        if (success) {
            markMigrationCompleted(migration.id);
        } else {
            qWarning() << "[Migration] sync FAILED:" << migration.id;
            allSuccess = false;
            emit migrationError(migration.id, "Migration failed");
            break; // Stop on first failure for sync migrations.
        }
    }

    if (allSuccess) {
        qInfo() << "[Migration] all sync migrations completed successfully";
    }
    return allSuccess;
}

bool MigrationService::syncCleanupFeedStorage()
{
    if (!db_) {
        qWarning() << "[Migration] database not available";
        return false;
    }

    qInfo() << "[Migration] v2.0.12: starting cleanup...";

    // Step 1: clear the feed table.
    qInfo() << "[Migration] clearing feed table...";
    db_->execute(QStringLiteral("TRUNCATE TABLE feed"));

    // Step 2: delete the P2P distributed-storage folder in the data directory.
    const QString storagePath = dataDirectory_ + "/storage";
    QDir storageDir(storagePath);
    if (storageDir.exists()) {
        qInfo() << "[Migration] removing storage folder:" << storagePath;
        if (storageDir.removeRecursively()) {
            qInfo() << "[Migration] storage folder removed successfully";
        } else {
            qWarning() << "[Migration] failed to remove storage folder";
        }
    } else {
        qInfo() << "[Migration] storage folder does not exist, skipping";
    }

    return true;
}

bool MigrationService::syncUpdateWalkInterval()
{
    if (!config_) {
        qWarning() << "[Migration] config not available";
        return false;
    }

    const int currentInterval = config_->spiderWalkInterval();
    qInfo() << "[Migration] v2.0.19: current spider walkInterval:" << currentInterval << "ms";

    // Bump to the 100ms default if still on the earlier, more aggressive value.
    if (currentInterval < 100) {
        config_->setSpiderWalkInterval(100);
        config_->save();
        qInfo() << "[Migration] v2.0.19: updated spider walkInterval from" << currentInterval << "ms to 100ms";
    } else {
        qInfo() << "[Migration] v2.0.19: walkInterval already >= 100ms, no change "
                   "needed";
    }

    return true;
}

// ============================================================================
// Async migrations
// ============================================================================

void MigrationService::startAsyncMigrations()
{
    if (isFreshInstall_) {
        qInfo() << "[Migration] fresh install - skipping async migrations";
        emit allMigrationsCompleted();
        return;
    }

    if (isRunning_.load()) {
        qWarning() << "[Migration] async migrations already running";
        return;
    }

    {
        QMutexLocker locker(&stateMutex_);
        if (!inProgressMigration_.migrationId.isEmpty()) {
            qInfo() << "[Migration] resuming interrupted migration:" << inProgressMigration_.migrationId
                    << "from ID:" << inProgressMigration_.lastProcessedId;
        }
    }

    // Collect the pending async migrations.
    QVector<MigrationDef> pendingMigrations;
    for (const auto& migration : registeredMigrations_) {
        if (migration.isSync) {
            continue;
        }
        if (isMigrationCompleted(migration.id)) {
            qInfo() << "[Migration] async already completed:" << migration.id;
            continue;
        }
        if (!shouldRunMigration(migration)) {
            qInfo() << "[Migration] async skipped (version mismatch):" << migration.id
                    << "minVersion:" << migration.minVersion << "maxVersion:" << migration.maxVersion;
            continue;
        }
        pendingMigrations.append(migration);
    }

    if (pendingMigrations.isEmpty()) {
        qInfo() << "[Migration] no pending async migrations";
        emit allMigrationsCompleted();
        return;
    }

    isRunning_.store(true);
    stopRequested_.store(false);
    progressSaveTimer_->start();

    asyncFuture_ = QtConcurrent::run([this, pendingMigrations]() {
        for (const auto& migration : pendingMigrations) {
            if (stopRequested_.load()) {
                qInfo() << "[Migration] stop requested, breaking";
                break;
            }

            qInfo() << "[Migration] starting async:" << migration.id << "-" << migration.description;

            // Start a fresh in-progress record unless we are resuming this exact
            // migration (in which case its saved lastProcessedId is preserved).
            {
                QMutexLocker locker(&stateMutex_);
                if (inProgressMigration_.migrationId != migration.id) {
                    inProgressMigration_.migrationId = migration.id;
                    inProgressMigration_.minVersion = migration.minVersion;
                    inProgressMigration_.maxVersion = migration.maxVersion;
                    inProgressMigration_.lastProcessedId = 0;
                    inProgressMigration_.totalItems = 0;
                    inProgressMigration_.startedAt = QDateTime::currentMSecsSinceEpoch();
                    inProgressMigration_.completed = false;
                }

                currentProgress_.migrationId = migration.id;
                currentProgress_.description = migration.description;
                currentProgress_.isRunning = true;
            }

            if (migration.asyncBody) {
                migration.asyncBody();
            }

            if (!stopRequested_.load()) {
                markMigrationCompleted(migration.id);
            }
        }

        QMetaObject::invokeMethod(
            this,
            [this]() {
                isRunning_.store(false);
                progressSaveTimer_->stop();
                {
                    QMutexLocker locker(&stateMutex_);
                    currentProgress_.isRunning = false;
                }

                if (!stopRequested_.load()) {
                    qInfo() << "[Migration] all async migrations completed";
                    emit allMigrationsCompleted();
                } else {
                    qInfo() << "[Migration] async migrations stopped (will resume on "
                               "next startup)";
                    saveState();
                }
            },
            Qt::QueuedConnection);
    });
}

bool MigrationService::isRunning() const
{
    return isRunning_.load();
}

void MigrationService::requestStop()
{
    if (!isRunning_.load()) {
        return;
    }
    qInfo() << "[Migration] requesting stop...";
    stopRequested_.store(true);
    saveState();
}

MigrationService::Progress MigrationService::currentProgress() const
{
    QMutexLocker locker(&stateMutex_);
    return currentProgress_;
}

// ============================================================================
// Async migration: recategorize all torrents
// ============================================================================

void MigrationService::recategorizeTorrents()
{
    if (!db_ || !repository_) {
        qWarning() << "[Migration] database/repository not available";
        return;
    }

    qInfo() << "[Migration] starting torrent recategorization...";

    qint64 startId = 0;
    {
        QMutexLocker locker(&stateMutex_);
        startId = inProgressMigration_.lastProcessedId;
        // Compute the total once, on a fresh start, for progress reporting.
        if (inProgressMigration_.totalItems == 0) {
            inProgressMigration_.totalItems = db_->count(QStringLiteral("torrents"));
            qInfo() << "[Migration] total torrents to process:" << inProgressMigration_.totalItems;
        }
    }

    const int batchSize = 100;
    qint64 processedCount = startId > 0 ? startId : 0; // approximate resume count
    qint64 lastId = startId;
    int updatedCount = 0;

    while (!stopRequested_.load()) {
        const QString querySql = QStringLiteral("SELECT id, hash, name, contenttype, contentcategory "
                                                "FROM torrents WHERE id > %1 ORDER BY id ASC LIMIT %2")
                                     .arg(lastId)
                                     .arg(batchSize);

        const data::Database::Rows rows = db_->query(querySql);
        if (rows.isEmpty()) {
            qInfo() << "[Migration] no more torrents to process";
            break;
        }

        for (const QVariantMap& row : rows) {
            if (stopRequested_.load()) {
                break;
            }

            const qint64 id = row["id"].toLongLong();
            const QString hash = row["hash"].toString();
            const QString name = row["name"].toString();
            const int oldTypeId = row["contenttype"].toInt();
            const int oldCategoryId = row["contentcategory"].toInt();

            const QVector<domain::File> files = repository_->filesOf(hash);
            if (!files.isEmpty()) {
                const domain::Classification classification = domain::ContentClassifier::classify(name, files);
                const int newTypeId = domain::toId(classification.type);
                const int newCategoryId = domain::toId(classification.category);

                if (newTypeId != oldTypeId || newCategoryId != oldCategoryId) {
                    repository_->updateClassification(hash, classification.type, classification.category);
                    updatedCount++;
                    if (updatedCount % 100 == 0) {
                        qInfo() << "[Migration] updated" << updatedCount << "torrents so far";
                    }
                }
            }

            lastId = id;
            processedCount++;

            QMutexLocker locker(&stateMutex_);
            inProgressMigration_.lastProcessedId = lastId;
            currentProgress_.current = processedCount;
            currentProgress_.total = inProgressMigration_.totalItems;
        }

        QString migrationId;
        qint64 total = 0;
        {
            QMutexLocker locker(&stateMutex_);
            migrationId = inProgressMigration_.migrationId;
            total = inProgressMigration_.totalItems;
        }
        emit migrationProgress(migrationId, processedCount, total);

        if (!stopRequested_.load()) {
            QThread::msleep(10);
        }

        if (rows.size() < batchSize) {
            qInfo() << "[Migration] last batch processed";
            break;
        }
    }

    if (!stopRequested_.load()) {
        qInfo() << "[Migration] recategorization complete. Updated" << updatedCount << "out of" << processedCount
                << "torrents";
    } else {
        qInfo() << "[Migration] stopped at ID" << lastId << ". Will resume on next startup.";
    }
}

// ============================================================================
// Async migration: remove Unknown-type torrents
// ============================================================================

void MigrationService::removeUnknownTorrents()
{
    if (!db_ || !repository_) {
        qWarning() << "[Migration] database/repository not available";
        return;
    }

    qInfo() << "[Migration] starting removal of Unknown-type torrents...";

    qint64 startId = 0;
    {
        QMutexLocker locker(&stateMutex_);
        startId = inProgressMigration_.lastProcessedId;
        // ContentType::Unknown == 0.
        if (inProgressMigration_.totalItems == 0) {
            inProgressMigration_.totalItems = db_->count(QStringLiteral("torrents"), QStringLiteral("contenttype = 0"));
            qInfo() << "[Migration] total Unknown-type torrents to remove:" << inProgressMigration_.totalItems;
            if (inProgressMigration_.totalItems == 0) {
                qInfo() << "[Migration] no Unknown-type torrents found, nothing to remove";
                return;
            }
        }
    }

    const int batchSize = 100;
    qint64 removedCount = 0;
    qint64 lastId = startId;

    while (!stopRequested_.load()) {
        const QString querySql = QStringLiteral("SELECT id, hash, name FROM torrents "
                                                "WHERE contenttype = 0 AND id > %1 ORDER BY id ASC LIMIT %2")
                                     .arg(lastId)
                                     .arg(batchSize);

        const data::Database::Rows rows = db_->query(querySql);
        if (rows.isEmpty()) {
            qInfo() << "[Migration] no more Unknown-type torrents to remove";
            break;
        }

        for (const QVariantMap& row : rows) {
            if (stopRequested_.load()) {
                break;
            }

            const qint64 id = row["id"].toLongLong();
            const QString hash = row["hash"].toString();
            const QString name = row["name"].toString();

            if (repository_->remove(hash)) {
                removedCount++;
                if (removedCount % 100 == 0) {
                    qInfo() << "[Migration] removed" << removedCount << "Unknown-type torrents so far";
                }
            } else {
                qWarning() << "[Migration] failed to remove torrent" << hash.left(16) << name.left(40);
            }

            lastId = id;

            QMutexLocker locker(&stateMutex_);
            inProgressMigration_.lastProcessedId = lastId;
            currentProgress_.current = removedCount;
            currentProgress_.total = inProgressMigration_.totalItems;
        }

        QString migrationId;
        qint64 total = 0;
        {
            QMutexLocker locker(&stateMutex_);
            migrationId = inProgressMigration_.migrationId;
            total = inProgressMigration_.totalItems;
        }
        emit migrationProgress(migrationId, removedCount, total);

        if (!stopRequested_.load()) {
            QThread::msleep(10);
        }

        // No rows.size() < batchSize check: removing rows shifts the result set,
        // so we loop until the query returns nothing.
    }

    if (!stopRequested_.load()) {
        qInfo() << "[Migration] removal complete. Removed" << removedCount
                << "Unknown-type torrents (including their files)";
    } else {
        qInfo() << "[Migration] stopped at ID" << lastId << ". Will resume on next startup.";
    }
}

} // namespace rats::service
