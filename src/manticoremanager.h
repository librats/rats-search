#ifndef MANTICOREMANAGER_H
#define MANTICOREMANAGER_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QString>
#include <QSqlDatabase>
#include <memory>

/**
 * @brief ManticoreManager - Manages the Manticore Search (searchd) process
 * 
 * This class is responsible for:
 * - Starting/stopping the searchd process
 * - Generating the sphinx.conf configuration file
 * - Monitoring the process health
 * - Providing MySQL connection to the database
 */
class ManticoreManager : public QObject
{
    Q_OBJECT

public:
    enum class Status {
        Stopped,
        Starting,
        Running,
        Error
    };
    Q_ENUM(Status)

    explicit ManticoreManager(const QString& dataDirectory, QObject *parent = nullptr);
    ~ManticoreManager();

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
    void logMessage(const QString& message);

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

#endif // MANTICOREMANAGER_H

