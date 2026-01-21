#include "manticoremanager.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QSqlError>
#include <QThread>
#include <QDebug>
#include <QRegularExpression>
#include <QElapsedTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#else
#include <signal.h>
#endif

// Helper function to check if a process is running by PID
static bool isProcessRunning(qint64 pid)
{
    if (pid <= 0) return false;
    
#ifdef Q_OS_WIN
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (hProcess == NULL) {
        return false;
    }
    DWORD exitCode;
    bool running = GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(hProcess);
    return running;
#else
    // Unix: check if process exists by sending signal 0
    return kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

ManticoreManager::ManticoreManager(const QString& dataDirectory, QObject *parent)
    : QObject(parent)
    , dataDirectory_(dataDirectory)
    , port_(9306)  // Default MySQL port for Manticore
    , status_(Status::Stopped)
    , isExternalInstance_(false)
    , isWindowsDaemonMode_(false)
{
    databasePath_ = dataDirectory_ + "/database";
    configPath_ = dataDirectory_ + "/sphinx.conf";
    pidFilePath_ = dataDirectory_ + "/searchd.pid";
    connectionName_ = "manticore_" + QString::number(reinterpret_cast<quintptr>(this));
    
    process_ = std::make_unique<QProcess>();
    connectionCheckTimer_ = std::make_unique<QTimer>();
    connectionCheckTimer_->setInterval(1000);
    
    connect(process_.get(), &QProcess::started, this, &ManticoreManager::onProcessStarted);
    connect(process_.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ManticoreManager::onProcessFinished);
    connect(process_.get(), &QProcess::errorOccurred, this, &ManticoreManager::onProcessError);
    connect(process_.get(), &QProcess::readyReadStandardOutput, this, &ManticoreManager::onProcessReadyRead);
    connect(process_.get(), &QProcess::readyReadStandardError, this, &ManticoreManager::onProcessReadyRead);
    connect(connectionCheckTimer_.get(), &QTimer::timeout, this, &ManticoreManager::checkConnection);
}

ManticoreManager::~ManticoreManager()
{
    // Disconnect all signals before stopping to prevent spurious error messages
    if (process_) {
        disconnect(process_.get(), nullptr, this, nullptr);
    }
    if (connectionCheckTimer_) {
        connectionCheckTimer_->stop();
        disconnect(connectionCheckTimer_.get(), nullptr, this, nullptr);
    }
    
    // Stop if not already stopped (stop() has its own guard)
    stop();
    
    // Remove database connection for current thread
    QString threadConnName = connectionName_ + "_" + QString::number(reinterpret_cast<quintptr>(QThread::currentThread()));
    if (QSqlDatabase::contains(threadConnName)) {
        QSqlDatabase::removeDatabase(threadConnName);
    }
}

bool ManticoreManager::start()
{
    if (status_ == Status::Running) {
        return true;
    }
    
    QElapsedTimer startupTimer;
    startupTimer.start();
    
    isWindowsDaemonMode_ = false;
    
    // Check if external instance is running via PID file (fast check first)
    if (QFile::exists(pidFilePath_)) {
        QFile pidFile(pidFilePath_);
        if (pidFile.open(QIODevice::ReadOnly)) {
            qint64 pid = pidFile.readAll().trimmed().toLongLong();
            pidFile.close();
            
            if (isProcessRunning(pid)) {
                qInfo() << "Found existing Manticore instance via PID file (pid:" << pid << ")";
                
                // Verify it's responding with quick timeout
                if (testConnection()) {
                    isExternalInstance_ = true;
                    setStatus(Status::Running);
                    emit started();
                    qInfo() << "Manticore startup (existing instance):" << startupTimer.elapsed() << "ms";
                    return true;
                }
            }
        }
    }
    
    // Find searchd executable FIRST (before slow network check)
    qint64 findStart = startupTimer.elapsed();
    searchdPath_ = findSearchdPath();
    qInfo() << "findSearchdPath took:" << (startupTimer.elapsed() - findStart) << "ms";
    
    if (searchdPath_.isEmpty()) {
        emit error("Cannot find searchd executable");
        setStatus(Status::Error);
        return false;
    }
    
    qInfo() << "Found searchd at:" << searchdPath_;
    
    // Create directories and config
    qint64 configStart = startupTimer.elapsed();
    if (!createDatabaseDirectories()) {
        emit error("Failed to create database directories");
        setStatus(Status::Error);
        return false;
    }
    
    if (!generateConfig()) {
        emit error("Failed to generate configuration");
        setStatus(Status::Error);
        return false;
    }
    qInfo() << "Config generation took:" << (startupTimer.elapsed() - configStart) << "ms";
    
    // Start process
    setStatus(Status::Starting);
    
    QStringList args;
    args << "--config" << configPath_;
    
#ifdef Q_OS_WIN
    // On Windows, searchd runs as a daemon (forks itself and parent exits)
    isWindowsDaemonMode_ = true;
#else
    // On Linux/macOS, use --nodetach to keep process in foreground
    args << "--nodetach";
#endif
    
    qInfo() << "Starting searchd with args:" << args;
    
    qint64 processStart = startupTimer.elapsed();
    process_->start(searchdPath_, args);
    
    if (!process_->waitForStarted(5000)) {
        emit error("Failed to start searchd process");
        setStatus(Status::Error);
        return false;
    }
    qInfo() << "Process start took:" << (startupTimer.elapsed() - processStart) << "ms";
    
#ifdef Q_OS_WIN
    // On Windows, the process forks into background immediately
    // Don't wait for full finish - just give it a moment to fork
    // The actual readiness will be detected in waitForReady()
    process_->waitForFinished(100);  // Reduced from 3000ms to 100ms
    qInfo() << "Windows daemon mode: searchd forking to background";
#endif
    
    // Wait for ready with optimized polling
    qint64 waitStart = startupTimer.elapsed();
    bool ready = waitForReady(30000);
    qInfo() << "waitForReady took:" << (startupTimer.elapsed() - waitStart) << "ms";
    qInfo() << "Total Manticore startup:" << startupTimer.elapsed() << "ms";
    
    return ready;
}

void ManticoreManager::stop()
{
    connectionCheckTimer_->stop();
    
    // Guard against double-stop calls
    if (status_ == Status::Stopped) {
        return;
    }
    
    if (isExternalInstance_) {
        qInfo() << "Not stopping external Manticore instance";
        setStatus(Status::Stopped);
        emit stopped();
        return;
    }
    
    qInfo() << "Stopping Manticore...";
    
    // Disconnect error signals to prevent spurious "crashed" messages during shutdown
    disconnect(process_.get(), &QProcess::errorOccurred, this, &ManticoreManager::onProcessError);
    disconnect(process_.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
               this, &ManticoreManager::onProcessFinished);
    
#ifdef Q_OS_WIN
    // On Windows, searchd runs as a daemon, so we need to use stopwait command
    // regardless of QProcess state
    if (!searchdPath_.isEmpty() && QFile::exists(configPath_)) {
        QProcess stopProcess;
        QStringList args;
        args << "--config" << configPath_ << "--stopwait";
        qInfo() << "Executing stopwait:" << searchdPath_ << args;
        stopProcess.start(searchdPath_, args);
        if (!stopProcess.waitForFinished(10000)) {
            qWarning() << "stopwait command timed out";
        } else {
            qInfo() << "stopwait finished with code" << stopProcess.exitCode();
        }
    }
    
    // Clean up the original QProcess object to prevent "destroyed while running" warning
    // On Windows daemon mode, the parent process has already exited, but QProcess
    // may not have properly detected this. We need to ensure it's in NotRunning state.
    if (process_) {
        if (process_->state() != QProcess::NotRunning) {
            // Try to wait for the process to finish (should be instant since parent already exited)
            if (!process_->waitForFinished(1000)) {
                // Force cleanup if needed
                process_->kill();
                process_->waitForFinished(1000);
            }
        }
    }
#else
    // On Unix, process runs in foreground with --nodetach
    if (process_ && process_->state() != QProcess::NotRunning) {
        process_->terminate();
        if (!process_->waitForFinished(5000)) {
            qWarning() << "Process did not terminate gracefully, killing...";
            process_->kill();
            process_->waitForFinished(2000);
        }
    }
#endif
    
    setStatus(Status::Stopped);
    emit stopped();
}

bool ManticoreManager::isRunning() const
{
    return status_ == Status::Running;
}

QSqlDatabase ManticoreManager::getDatabase() const
{
    // Each thread needs its own connection - include thread ID in connection name
    QString threadConnName = connectionName_ + "_" + QString::number(reinterpret_cast<quintptr>(QThread::currentThread()));
    
    if (!QSqlDatabase::contains(threadConnName)) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL", threadConnName);
        db.setHostName("127.0.0.1");
        db.setPort(port_);
        db.setDatabaseName("");
    }
    
    return QSqlDatabase::database(threadConnName);
}

bool ManticoreManager::waitForReady(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    
    // Start with fast polling, then slow down
    int checkInterval = 50;  // Start with 50ms checks for fast startup detection
    
    qInfo() << "Waiting for Manticore to become ready...";
    
    while (timer.elapsed() < timeoutMs) {
        // Check PID file first (faster than network connection)
        bool pidExists = QFile::exists(pidFilePath_);
        
        if (pidExists && testConnection()) {
            setStatus(Status::Running);
            emit started();
            qInfo() << "Manticore is ready on port" << port_ << "(took" << timer.elapsed() << "ms)";
            
            // Start connection monitoring
            connectionCheckTimer_->start();
            return true;
        }
        
        QThread::msleep(checkInterval);
        
        // Increase interval after first second to reduce CPU usage
        if (timer.elapsed() > 1000 && checkInterval < 200) {
            checkInterval = 200;
        }
        
        // On Windows with daemon mode, we can't check process state
        // as the original process has already exited
        if (!isWindowsDaemonMode_) {
            // Check if process crashed (only for non-daemon mode)
            if (process_ && process_->state() == QProcess::NotRunning) {
                QString output = process_->readAllStandardError();
                qWarning() << "Manticore stderr:" << output;
                emit error("Manticore process exited unexpectedly");
                setStatus(Status::Error);
                return false;
            }
        } else {
            // On Windows, check if PID file was created as a sign of startup progress
            if (timer.elapsed() > 5000 && !pidExists) {
                qWarning() << "PID file not created after 5 seconds, searchd may have failed to start";
            }
        }
        
        // Log progress every 5 seconds
        if (timer.elapsed() > 0 && (timer.elapsed() / 1000) % 5 == 0 && timer.elapsed() % 1000 < checkInterval) {
            qInfo() << "Still waiting for Manticore..." << (timer.elapsed() / 1000) << "seconds elapsed";
        }
    }
    
    emit error("Timeout waiting for Manticore to start");
    setStatus(Status::Error);
    return false;
}

void ManticoreManager::onProcessStarted()
{
    qInfo() << "Manticore process started";
}

void ManticoreManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qInfo() << "Manticore process finished with code" << exitCode 
            << "status" << (exitStatus == QProcess::NormalExit ? "NormalExit" : "CrashExit");
    
#ifdef Q_OS_WIN
    // On Windows, searchd forks into background and parent exits with code 0
    // This is normal behavior, not an error
    if (isWindowsDaemonMode_ && exitCode == 0 && status_ == Status::Starting) {
        qInfo() << "Windows: searchd forked to background, waiting for connection...";
        return;  // Don't change status, waitForReady() will handle it
    }
#endif
    
    if (status_ == Status::Running && exitStatus != QProcess::NormalExit) {
        emit error(QString("Manticore crashed with exit code %1").arg(exitCode));
    }
    
    // Only set stopped if we were actually running (not starting in daemon mode)
    if (status_ == Status::Running || (status_ == Status::Starting && !isWindowsDaemonMode_)) {
        setStatus(Status::Stopped);
        emit stopped();
    }
}

void ManticoreManager::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = "Failed to start searchd";
        break;
    case QProcess::Crashed:
        errorMsg = "searchd crashed";
        break;
    case QProcess::Timedout:
        errorMsg = "searchd timed out";
        break;
    default:
        errorMsg = "Unknown error with searchd";
    }
    
    qCritical() << "Manticore error:" << errorMsg;
    emit this->error(errorMsg);
    setStatus(Status::Error);
}

void ManticoreManager::onProcessReadyRead()
{
    QString output = process_->readAllStandardOutput() + process_->readAllStandardError();
    
    for (const QString& line : output.split('\n', Qt::SkipEmptyParts)) {
        emit logMessage(line.trimmed());
        
        // Parse version
        QRegularExpression versionRx("Manticore ([0-9\\.]+)");
        QRegularExpressionMatch match = versionRx.match(line);
        if (match.hasMatch() && version_.isEmpty()) {
            version_ = match.captured(1);
            qInfo() << "Manticore version:" << version_;
        }
        
        // Check for "accepting connections"
        if (line.contains("accepting connections")) {
            qInfo() << "Manticore accepting connections";
        }
    }
}

void ManticoreManager::checkConnection()
{
    if (!testConnection() && status_ == Status::Running) {
        qWarning() << "Lost connection to Manticore";
        setStatus(Status::Error);
        emit error("Lost connection to Manticore");
    }
}

bool ManticoreManager::generateConfig()
{
    QString config = QString(R"(
index torrents
{
    type = rt
    path = %1/torrents
    
    min_prefix_len = 3
    expand_keywords = 1
    
    rt_attr_string = hash
    rt_attr_string = name
    rt_field = nameIndex
    rt_attr_bigint = size
    rt_attr_uint = files
    rt_attr_uint = piecelength
    rt_attr_timestamp = added
    rt_field = ipv4
    rt_attr_uint = port
    rt_attr_uint = contentType
    rt_attr_uint = contentCategory
    rt_attr_uint = seeders
    rt_attr_uint = leechers
    rt_attr_uint = completed
    rt_attr_timestamp = trackersChecked
    rt_attr_uint = good
    rt_attr_uint = bad
    rt_attr_json = info

    stored_only_fields = ipv4
}

index files
{
    type = rt
    path = %1/files
    
    rt_field = path
    rt_attr_string = hash
    rt_field = size

    stored_fields = path
    stored_only_fields = size
}

index version
{
    type = rt
    path = %1/version
    
    rt_attr_uint = version
    rt_field = versionIndex
}

index store
{
    type = rt
    path = %1/store
    
    rt_field = storeIndex
    rt_attr_json = data
    rt_attr_string = hash
    rt_attr_string = peerId
}

index feed
{
    type = rt
    path = %1/feed

    rt_field = feedIndex
    rt_attr_json = data
}

searchd
{
    listen = 127.0.0.1:%2:mysql41
    seamless_rotate = 1
    preopen_indexes = 1
    unlink_old = 1
    pid_file = %3/searchd.pid
    log = %3/searchd.log
    query_log = %3/query.log
    binlog_path = %3
}
)").arg(databasePath_).arg(port_).arg(dataDirectory_);

    QFile file(configPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "Failed to write config file:" << configPath_;
        return false;
    }
    
    QTextStream out(&file);
    out << config;
    file.close();
    
    qInfo() << "Generated Manticore config at:" << configPath_;
    return true;
}

bool ManticoreManager::createDatabaseDirectories()
{
    QDir dir;
    
    if (!dir.mkpath(dataDirectory_)) {
        qCritical() << "Failed to create data directory:" << dataDirectory_;
        return false;
    }
    
    if (!dir.mkpath(databasePath_)) {
        qCritical() << "Failed to create database directory:" << databasePath_;
        return false;
    }
    
    return true;
}

QString ManticoreManager::findSearchdPath()
{
    // Possible paths for searchd
    QStringList searchPaths;
    
    // Get application directory
    QString appDir = QCoreApplication::applicationDirPath();
    
    // Check near executable first (for deployed builds)
    searchPaths << appDir + "/searchd"
                << appDir + "/searchd.exe";
    
    // Check imports directory based on platform
    // For both deployed and development builds (from build/bin directory)
#ifdef Q_OS_WIN
    #ifdef Q_PROCESSOR_X86_64
    searchPaths << appDir + "/../imports/win/x64/searchd.exe"
                << appDir + "/imports/win/x64/searchd.exe"
                << appDir + "/../../imports/win/x64/searchd.exe"
                << appDir + "/../../../imports/win/x64/searchd.exe"
                << "imports/win/x64/searchd.exe"
                << "../imports/win/x64/searchd.exe"
                << "../../imports/win/x64/searchd.exe";
    #else
    searchPaths << appDir + "/../imports/win/ia32/searchd.exe"
                << appDir + "/imports/win/ia32/searchd.exe"
                << appDir + "/../../imports/win/ia32/searchd.exe"
                << appDir + "/../../../imports/win/ia32/searchd.exe"
                << "imports/win/ia32/searchd.exe"
                << "../imports/win/ia32/searchd.exe"
                << "../../imports/win/ia32/searchd.exe";
    #endif
#elif defined(Q_OS_MACOS)
    #ifdef Q_PROCESSOR_ARM
    searchPaths << appDir + "/../imports/darwin/arm64/searchd"
                << appDir + "/../../imports/darwin/arm64/searchd"
                << appDir + "/../../../imports/darwin/arm64/searchd"
                << "imports/darwin/arm64/searchd"
                << "../imports/darwin/arm64/searchd"
                << "../../imports/darwin/arm64/searchd";
    #else
    searchPaths << appDir + "/../imports/darwin/x64/searchd"
                << appDir + "/../../imports/darwin/x64/searchd"
                << appDir + "/../../../imports/darwin/x64/searchd"
                << "imports/darwin/x64/searchd"
                << "../imports/darwin/x64/searchd"
                << "../../imports/darwin/x64/searchd";
    #endif
#else  // Linux
    #ifdef Q_PROCESSOR_X86_64
    searchPaths << appDir + "/../imports/linux/x64/searchd"
                << appDir + "/../../imports/linux/x64/searchd"
                << appDir + "/../../../imports/linux/x64/searchd"
                << "imports/linux/x64/searchd"
                << "../imports/linux/x64/searchd"
                << "../../imports/linux/x64/searchd";
    #else
    searchPaths << appDir + "/../imports/linux/ia32/searchd"
                << appDir + "/../../imports/linux/ia32/searchd"
                << appDir + "/../../../imports/linux/ia32/searchd"
                << "imports/linux/ia32/searchd"
                << "../imports/linux/ia32/searchd"
                << "../../imports/linux/ia32/searchd";
    #endif
#endif
    
    // System paths
    searchPaths << "/usr/bin/searchd"
                << "/usr/local/bin/searchd";
    
    qDebug() << "Searching for searchd in:" << searchPaths;
    
    for (const QString& path : searchPaths) {
        if (QFile::exists(path)) {
            QString absPath = QFileInfo(path).absoluteFilePath();
            qInfo() << "Found searchd at:" << absPath;
            return absPath;
        }
    }
    
    // Try PATH environment variable
    QString found = QStandardPaths::findExecutable("searchd");
    if (!found.isEmpty()) {
        qInfo() << "Found searchd in PATH:" << found;
        return found;
    }
    
    qWarning() << "searchd not found! Searched paths:" << searchPaths;
    return QString();
}

bool ManticoreManager::testConnection()
{
    // Use a temporary connection for testing with short timeout
    QString testConnName = connectionName_ + "_test";
    bool success = false;
    
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL", testConnName);
        db.setHostName("127.0.0.1");
        db.setPort(port_);
        db.setDatabaseName("");
        // Set short connection timeout (1 second instead of default ~30s)
        db.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=1");
        
        if (db.open()) {
            QSqlQuery query(db);
            success = query.exec("SHOW STATUS");
            query.clear();
            db.close();
        }
    }
    
    // Remove connection outside of scope where db was used
    QSqlDatabase::removeDatabase(testConnName);
    return success;
}

void ManticoreManager::setStatus(Status status)
{
    if (status_ != status) {
        status_ = status;
        emit statusChanged(status);
    }
}

