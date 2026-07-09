#ifndef RATS_DATA_MANTICORE_H
#define RATS_DATA_MANTICORE_H

#include <QObject>
#include <QProcess>
#include <QSqlDatabase>
#include <QString>
#include <QTimer>
#include <memory>

namespace rats::data {

/**
 * @brief Manticore - Manages the Manticore Search (searchd) process
 *
 * This class is responsible for:
 * - Starting/stopping the searchd process
 * - Generating the sphinx.conf configuration file
 * - Monitoring the process health
 * - Providing a MySQL connection to the database
 */
class Manticore : public QObject {
    Q_OBJECT

public:
    enum class Status { Stopped, Starting, Running, Error };
    Q_ENUM(Status)

    explicit Manticore(const QString& dataDirectory, QObject* parent = nullptr);
    ~Manticore();

    /**
     * @brief Start the Manticore Search daemon
     * @return true if started successfully or already running
     */
    bool start();

    /**
     * @brief Stop the Manticore Search daemon
     */
    void stop();

    /**
     * @brief Check if Manticore is running
     */
    bool isRunning() const;

    /**
     * @brief Get current status
     */
    Status status() const { return status_; }

    /**
     * @brief Get the port number
     */
    int port() const { return port_; }

    /**
     * @brief Set custom port
     */
    void setPort(int port) { port_ = port; }

    /**
     * @brief Get database path
     */
    QString databasePath() const { return databasePath_; }

    /**
     * @brief Get the Manticore version
     */
    QString version() const { return version_; }

    /**
     * @brief Get QSqlDatabase connection to Manticore
     * @note Must call start() first
     */
    QSqlDatabase getDatabase() const;

    /**
     * @brief Wait for Manticore to be ready (blocking)
     * @param timeoutMs Maximum time to wait in milliseconds
     * @return true if ready, false if timeout
     */
    bool waitForReady(int timeoutMs = 30000);

signals:
    void started();
    void stopped();
    void statusChanged(Status status);
    void error(const QString& message);

private slots:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onProcessReadyRead();
    void checkConnection();

private:
    bool generateConfig();
    bool createDatabaseDirectories();
    QString findSearchdPath();
    bool testConnection();
    void setStatus(Status status);
    bool isPortAvailable(int port);
    int findAvailablePort(int startPort, int maxAttempts = 10);

    // start() helpers — each returns true on success and, on failure, emits an
    // error()/sets Status::Error before returning false.
    bool attachToExternalInstance(qint64 startupElapsedMs);
    bool resolveSearchdPath();
    bool ensureAvailablePort();
    bool prepareDatabaseAndConfig();
    bool verifyDriverAvailable();
    bool launchSearchdProcess();

    // waitForReady() helpers.
    void sleepWithEventLoop(int intervalMs);

    /**
     * @brief Compute total size of the Manticore database directory in bytes.
     * @return Size in bytes, or 0 if directory doesn't exist / on error.
     */
    qint64 computeDatabaseSizeBytes() const;

    /**
     * @brief Calculate a startup timeout proportional to the database size.
     *
     * Manticore needs to precache every RT table on startup; the bigger the
     * on-disk data, the longer it takes before `searchd` starts accepting
     * connections. Base timeout is used for empty/small databases; for larger
     * ones additional milliseconds are added per MB of data.
     *
     * @return Timeout in milliseconds, clamped between a sane min and max.
     */
    int computeStartupTimeoutMs() const;

    QString dataDirectory_;
    QString databasePath_;
    QString configPath_;
    QString pidFilePath_;
    QString searchdPath_;
    int port_;
    Status status_;
    QString version_;

    std::unique_ptr<QProcess> process_;
    std::unique_ptr<QTimer> connectionCheckTimer_;
    bool isExternalInstance_;
    bool isWindowsDaemonMode_;
    QString connectionName_;
};

} // namespace rats::data

#endif // RATS_DATA_MANTICORE_H
