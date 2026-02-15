#ifndef MIGRATIONMANAGER_H
#define MIGRATIONMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QVersionNumber>
#include <memory>
#include <functional>

class TorrentDatabase;
class ConfigManager;

/**
 * MigrationManager - Handles post-update migrations
 * 
 * Runs one-time migration tasks after application updates.
 * Supports both synchronous (blocking) and asynchronous (background) migrations.
 * Background migrations are resumable - if interrupted (crash, close), they
 * continue from the last processed item on next startup.
 * 
 * Migration state is persisted to migrations.json in the data directory.
 */
class MigrationManager : public QObject
{
    Q_OBJECT

public:
    explicit MigrationManager(const QString& dataDirectory, QObject* parent = nullptr);
    ~MigrationManager();

    /**
     * Initialize with dependencies
     */
    void initialize(TorrentDatabase* database, ConfigManager* config);

    /**
     * Run synchronous (blocking) migrations.
     * These MUST complete before the application can start.
     * Called from MainWindow constructor before services start.
     * 
     * @return true if all sync migrations succeeded, false if any failed
     */
    bool runSyncMigrations();

    /**
     * Start asynchronous (background) migrations.
     * These run in the background and can be interrupted/resumed.
     * Called after services are started.
     */
    void startAsyncMigrations();

    /**
     * Check if any async migration is currently running
     */
    bool isRunning() const;

    /**
     * Request graceful stop of async migrations.
     * Saves current progress before stopping.
     */
    void requestStop();

    /**
     * Save current migration state to disk.
     * Called automatically on progress and when stopping.
     */
    void saveState();

    /**
     * Get current migration progress for UI display
     */
    struct Progress {
        QString migrationId;
        QString description;
        qint64 current;
        qint64 total;
        bool isRunning;
    };
    Progress currentProgress() const;

    /**
     * Get the current application version for migration tracking
     */
    static QString currentVersion();

signals:
    /**
     * Emitted when migration progress updates
     */
    void migrationProgress(const QString& migrationId, qint64 current, qint64 total);

    /**
     * Emitted when a single migration completes
     */
    void migrationCompleted(const QString& migrationId);

    /**
     * Emitted when all pending migrations complete
     */
    void allMigrationsCompleted();

    /**
     * Emitted when a migration encounters an error
     */
    void migrationError(const QString& migrationId, const QString& error);

private:
    // Migration function types
    using SyncMigrationFunc = std::function<bool()>;
    using AsyncMigrationFunc = std::function<void()>;

    // Internal migration state
    struct MigrationState {
        QString migrationId;
        QString minVersion;
        QString maxVersion;
        qint64 lastProcessedId = 0;
        qint64 totalItems = 0;
        bool completed = false;
        qint64 startedAt = 0;
    };

    // Load/save state
    void loadState();
    bool isMigrationCompleted(const QString& migrationId) const;
    void markMigrationCompleted(const QString& migrationId);

    // Register all migrations
    void registerMigrations();

    // Specific migration implementations
    // Sync migrations (blocking, must succeed)
    bool migration_v2_0_12_sync_cleanup_feed_storage();
    bool migration_v2_0_19_sync_update_walk_interval();

    // Async migrations (background, resumable)
    void migration_v2_0_12_recategorize_torrents();
    void migration_v2_0_12_remove_unknown_torrents();

    void recategorizeTorrentsBatch(qint64 startId, int batchSize);

    class Private;
    std::unique_ptr<Private> d;
};

#endif // MIGRATIONMANAGER_H
