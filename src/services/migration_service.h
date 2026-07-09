#ifndef RATS_SERVICE_MIGRATION_SERVICE_H
#define RATS_SERVICE_MIGRATION_SERVICE_H

#include <QFuture>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <functional>

class QTimer;

namespace rats::data {
class Database;
class TorrentRepository;
} // namespace rats::data

namespace rats::app {
class ConfigStore;
} // namespace rats::app

namespace rats::service {

// Handles post-update data migrations.
//
// Runs one-time migration tasks after application updates. Supports both
// synchronous (blocking, must finish before services start) and asynchronous
// (background, resumable) migrations. Background migrations are resumable: if
// interrupted (crash, close) they continue from the last processed id on the
// next startup. Migration state is persisted to migrations.json in the data
// directory.
//
// Migration ids and the on-disk state format are a compatibility contract: an
// existing migrations.json must keep resolving, so never rename or renumber a
// migration that has shipped. The internals:
//   - migrations are a data-driven registry: each definition carries its own
//     std::function body, so registering one is a single table entry instead of
//     a string-matched if/else duplicated in the sync and async dispatchers;
//   - the cross-thread run flags are std::atomic<bool>;
//   - migrations operate on the data layer (data::Database /
//     data::TorrentRepository) and reclassify via domain::ContentClassifier.
class MigrationService : public QObject {
    Q_OBJECT

public:
    MigrationService(const QString& dataDirectory, data::Database* db, data::TorrentRepository* repository,
        app::ConfigStore* config, QObject* parent = nullptr);
    ~MigrationService();

    // Run synchronous (blocking) migrations. These MUST complete before the
    // application can start. Returns true if all sync migrations succeeded.
    bool runSyncMigrations();

    // Start asynchronous (background) migrations. These run in the background and
    // can be interrupted and resumed.
    void startAsyncMigrations();

    // Whether an async migration is currently running.
    bool isRunning() const;

    // Request a graceful stop of the async migrations. Progress is saved before
    // stopping so it can resume on the next startup.
    void requestStop();

    // Persist the current migration state to disk. Called automatically on
    // progress and when stopping.
    void saveState();

    // Current migration progress for UI display.
    struct Progress {
        QString migrationId;
        QString description;
        qint64 current = 0;
        qint64 total = 0;
        bool isRunning = false;
    };
    Progress currentProgress() const;

    // Current application version used for migration tracking.
    static QString currentVersion();

signals:
    void migrationProgress(const QString& migrationId, qint64 current, qint64 total);
    void allMigrationsCompleted();
    void migrationError(const QString& migrationId, const QString& error);

private:
    // A sync migration returns success; an async one runs to completion (or until
    // stopRequested_) and reports progress through the shared state.
    using SyncMigrationFunc = std::function<bool()>;
    using AsyncMigrationFunc = std::function<void()>;

    // A single self-describing migration. `body` holds the work: exactly one of
    // sync/async is populated according to `isSync`. Registering a migration is a
    // single append to registeredMigrations_ — no separate dispatch table.
    struct MigrationDef {
        QString id;
        QString minVersion; // inclusive; empty = no lower bound
        QString maxVersion; // inclusive; empty = no upper bound
        QString description;
        bool isSync = false;
        SyncMigrationFunc syncBody;
        AsyncMigrationFunc asyncBody;
    };

    // Persisted state for an interrupted/resumable async migration.
    struct MigrationState {
        QString migrationId;
        QString minVersion;
        QString maxVersion;
        qint64 lastProcessedId = 0;
        qint64 totalItems = 0;
        bool completed = false;
        qint64 startedAt = 0;
    };

    // Registration --------------------------------------------------------------
    void registerMigrations();

    // State persistence ---------------------------------------------------------
    void loadState();
    bool isMigrationCompleted(const QString& migrationId) const;
    void markMigrationCompleted(const QString& migrationId);

    // Whether a migration applies to the current app version.
    bool shouldRunMigration(const MigrationDef& migration) const;

    // Migration bodies ----------------------------------------------------------
    // Sync (blocking, must succeed).
    bool syncCleanupFeedStorage();
    bool syncUpdateWalkInterval();
    // Async (background, resumable).
    void recategorizeTorrents();
    void removeUnknownTorrents();

    QString dataDirectory_;
    QString stateFilePath_;

    data::Database* db_ = nullptr;
    data::TorrentRepository* repository_ = nullptr;
    app::ConfigStore* config_ = nullptr;

    QVector<MigrationDef> registeredMigrations_;

    // Persisted state (guarded by stateMutex_).
    QStringList completedMigrations_;
    MigrationState inProgressMigration_;
    QString lastVersion_;
    mutable QMutex stateMutex_;

    // Runtime state shared with the background worker.
    std::atomic<bool> isRunning_ { false };
    std::atomic<bool> stopRequested_ { false };
    bool isFreshInstall_ = false;

    Progress currentProgress_;

    QFuture<void> asyncFuture_;
    QTimer* progressSaveTimer_ = nullptr;
};

} // namespace rats::service

#endif // RATS_SERVICE_MIGRATION_SERVICE_H
