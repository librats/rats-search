#include "data/manticore.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#ifdef Q_OS_WIN
// windows.h must precede tlhelp32.h — the latter depends on types it declares.
// The blank line keeps clang-format from sorting them back together.
#include <windows.h>

#include <tlhelp32.h>
#else
#include <signal.h>
#endif

namespace rats::data {

namespace {

// Default MySQL (mysql41) port that Manticore listens on.
constexpr int kDefaultPort = 9306;

// How often the health-check timer probes the live connection.
constexpr int kConnectionCheckIntervalMs = 1000;

// How long to wait for QProcess::start() to actually launch searchd.
constexpr int kProcessStartTimeoutMs = 5000;

// Windows: how long to wait for the `--stopwait` helper process to return.
constexpr int kStopWaitTimeoutMs = 10000;

// Windows daemon mode: grace period for the parent QProcess to reach
// NotRunning.
constexpr int kWindowsProcessCleanupTimeoutMs = 1000;

// Unix: graceful terminate window, then a shorter hard-kill window.
constexpr int kUnixTerminateTimeoutMs = 5000;
constexpr int kUnixKillTimeoutMs = 2000;

// If the QMYSQL driver is missing, how long to wait when tearing the process
// down.
constexpr int kDriverFailureTerminateTimeoutMs = 3000;

// Port search: number of alternative ports to probe when the default is busy.
constexpr int kPortSearchAttempts = 20;

// Connection timeout (seconds) used by the lightweight readiness probe.
constexpr int kTestConnectTimeoutSec = 1;

// waitForReady() polling cadence.
constexpr int kFastPollIntervalMs = 50; // initial tight polling for fast startups
constexpr int kSlowPollIntervalMs = 200; // relaxed polling to reduce CPU usage
constexpr int kPollSlowdownAfterMs = 1000; // switch to slow polling after this long
constexpr int kEventPumpBudgetMs = 10; // max time spent pumping the event loop per iteration
constexpr int kWindowsPidWarnMs = 5000; // warn if the PID file is missing this long into startup
constexpr int kProgressLogIntervalSec = 5; // emit a "still waiting" log every N seconds

// Helper: check whether a process is running by PID.
bool isProcessRunning(qint64 pid)
{
    if (pid <= 0)
        return false;

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

} // namespace

Manticore::Manticore(const QString& dataDirectory, QObject* parent)
    : QObject(parent)
    , dataDirectory_(dataDirectory)
    , port_(kDefaultPort)
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
    connectionCheckTimer_->setInterval(kConnectionCheckIntervalMs);

    connect(process_.get(), &QProcess::started, this, &Manticore::onProcessStarted);
    connect(process_.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        &Manticore::onProcessFinished);
    connect(process_.get(), &QProcess::errorOccurred, this, &Manticore::onProcessError);
    connect(process_.get(), &QProcess::readyReadStandardOutput, this, &Manticore::onProcessReadyRead);
    connect(process_.get(), &QProcess::readyReadStandardError, this, &Manticore::onProcessReadyRead);
    connect(connectionCheckTimer_.get(), &QTimer::timeout, this, &Manticore::checkConnection);
}

Manticore::~Manticore()
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
    QString threadConnName
        = connectionName_ + "_" + QString::number(reinterpret_cast<quintptr>(QThread::currentThread()));
    if (QSqlDatabase::contains(threadConnName)) {
        QSqlDatabase::removeDatabase(threadConnName);
    }
}

bool Manticore::attachToExternalInstance(qint64 startupElapsedMs)
{
    // Check if an external instance is running via the PID file (fast check
    // first).
    if (!QFile::exists(pidFilePath_)) {
        return false;
    }

    QFile pidFile(pidFilePath_);
    if (!pidFile.open(QIODevice::ReadOnly)) {
        return false;
    }

    qint64 pid = pidFile.readAll().trimmed().toLongLong();
    pidFile.close();

    if (!isProcessRunning(pid)) {
        return false;
    }

    qInfo() << "Found existing Manticore instance via PID file (pid:" << pid << ")";

    // Verify it's responding with a quick timeout.
    if (!testConnection()) {
        return false;
    }

    isExternalInstance_ = true;
    setStatus(Status::Running);
    qInfo() << "Manticore startup (existing instance):" << startupElapsedMs << "ms";
    return true;
}

bool Manticore::resolveSearchdPath()
{
    searchdPath_ = findSearchdPath();

    if (searchdPath_.isEmpty()) {
        fail("Cannot find searchd executable");
        return false;
    }

    qInfo() << "Found searchd at:" << searchdPath_;
    return true;
}

bool Manticore::ensureAvailablePort()
{
    if (isPortAvailable(port_)) {
        return true;
    }

    qInfo() << "Port" << port_ << "is already in use, searching for available port...";
    int newPort = findAvailablePort(port_ + 1, kPortSearchAttempts);
    if (newPort < 0) {
        fail(QString("No available port found (tried %1-%2)").arg(port_).arg(port_ + kPortSearchAttempts));
        return false;
    }
    port_ = newPort;
    qInfo() << "Using alternative port:" << port_;
    return true;
}

bool Manticore::prepareDatabaseAndConfig()
{
    if (!createDatabaseDirectories()) {
        fail("Failed to create database directories");
        return false;
    }

    if (!generateConfig()) {
        fail("Failed to generate configuration");
        return false;
    }
    return true;
}

bool Manticore::verifyDriverAvailable()
{
    if (QSqlDatabase::isDriverAvailable("QMYSQL")) {
        return true;
    }

    qCritical() << "Available SQL drivers:" << QSqlDatabase::drivers();
    fail("QMYSQL driver not available");
    // Stop the already-started process, if any.
    if (process_ && process_->state() != QProcess::NotRunning) {
        process_->terminate();
        process_->waitForFinished(kDriverFailureTerminateTimeoutMs);
    }
    return false;
}

bool Manticore::launchSearchdProcess()
{
    setStatus(Status::Starting);

    QStringList args;
    args << "--config" << configPath_;

#ifdef Q_OS_WIN
    // On Windows, searchd runs as a daemon (forks itself and the parent exits).
    isWindowsDaemonMode_ = true;
#else
    // On Linux/macOS, use --nodetach to keep the process in the foreground.
    args << "--nodetach";
#endif

    qInfo() << "Starting searchd with args:" << args;

    process_->start(searchdPath_, args);

    if (!process_->waitForStarted(kProcessStartTimeoutMs)) {
        fail("Failed to start searchd process");
        return false;
    }
    return true;
}

bool Manticore::start()
{
    if (status_ == Status::Running) {
        return true;
    }

    QElapsedTimer startupTimer;
    startupTimer.start();

    isWindowsDaemonMode_ = false;

    // Fast path: attach to an already-running external instance.
    if (attachToExternalInstance(startupTimer.elapsed())) {
        return true;
    }

    // Find searchd executable FIRST (before the slow port/network checks).
    qint64 findStart = startupTimer.elapsed();
    if (!resolveSearchdPath()) {
        return false;
    }
    qInfo() << "findSearchdPath took:" << (startupTimer.elapsed() - findStart) << "ms";

    // Check that the configured port is available, find an alternative if not.
    if (!ensureAvailablePort()) {
        return false;
    }

    // Create directories and config.
    qint64 configStart = startupTimer.elapsed();
    if (!prepareDatabaseAndConfig()) {
        return false;
    }
    qInfo() << "Config generation took:" << (startupTimer.elapsed() - configStart) << "ms";

    // Verify the QMYSQL driver is available before starting/waiting.
    if (!verifyDriverAvailable()) {
        return false;
    }

    // Start the process.
    qint64 processStart = startupTimer.elapsed();
    if (!launchSearchdProcess()) {
        return false;
    }
    qInfo() << "Process start took:" << (startupTimer.elapsed() - processStart) << "ms";

    // Wait for ready with optimized polling.
    // Timeout scales with database size — bigger databases take longer to
    // precache on startup before searchd accepts connections.
    qint64 waitStart = startupTimer.elapsed();
    const int readyTimeoutMs = computeStartupTimeoutMs();
    bool ready = waitForReady(readyTimeoutMs);
    qInfo() << "waitForReady took:" << (startupTimer.elapsed() - waitStart) << "ms"
            << "(timeout was" << readyTimeoutMs << "ms)";
    qInfo() << "Total Manticore startup:" << startupTimer.elapsed() << "ms";

    return ready;
}

void Manticore::stop()
{
    connectionCheckTimer_->stop();

    // Guard against double-stop calls
    if (status_ == Status::Stopped) {
        return;
    }

    if (isExternalInstance_) {
        qInfo() << "Not stopping external Manticore instance";
        setStatus(Status::Stopped);
        return;
    }

    qInfo() << "Stopping Manticore...";

    // Disconnect error signals to prevent spurious "crashed" messages during
    // shutdown
    disconnect(process_.get(), &QProcess::errorOccurred, this, &Manticore::onProcessError);
    disconnect(process_.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        &Manticore::onProcessFinished);

#ifdef Q_OS_WIN
    // On Windows, searchd runs as a daemon, so we need to use the stopwait
    // command regardless of QProcess state.
    if (!searchdPath_.isEmpty() && QFile::exists(configPath_)) {
        QProcess stopProcess;
        QStringList args;
        args << "--config" << configPath_ << "--stopwait";
        qInfo() << "Executing stopwait:" << searchdPath_ << args;
        stopProcess.start(searchdPath_, args);
        if (!stopProcess.waitForFinished(kStopWaitTimeoutMs)) {
            qWarning() << "stopwait command timed out";
        } else {
            qInfo() << "stopwait finished with code" << stopProcess.exitCode();
        }
    }

    // Clean up the original QProcess object to prevent a "destroyed while
    // running" warning. In Windows daemon mode, the parent process has already
    // exited, but QProcess may not have properly detected this. We need to ensure
    // it's in the NotRunning state.
    if (process_) {
        if (process_->state() != QProcess::NotRunning) {
            // Try to wait for the process to finish (should be instant since the
            // parent already exited).
            if (!process_->waitForFinished(kWindowsProcessCleanupTimeoutMs)) {
                // Force cleanup if needed.
                process_->kill();
                process_->waitForFinished(kWindowsProcessCleanupTimeoutMs);
            }
        }
    }
#else
    // On Unix, the process runs in the foreground with --nodetach.
    if (process_ && process_->state() != QProcess::NotRunning) {
        process_->terminate();
        if (!process_->waitForFinished(kUnixTerminateTimeoutMs)) {
            qWarning() << "Process did not terminate gracefully, killing...";
            process_->kill();
            process_->waitForFinished(kUnixKillTimeoutMs);
        }
    }
#endif

    setStatus(Status::Stopped);
}

bool Manticore::isRunning() const
{
    return status_ == Status::Running;
}

QSqlDatabase Manticore::getDatabase() const
{
    // Each thread needs its own connection - include the thread ID in the
    // connection name
    QString threadConnName
        = connectionName_ + "_" + QString::number(reinterpret_cast<quintptr>(QThread::currentThread()));

    if (!QSqlDatabase::contains(threadConnName)) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL", threadConnName);
        db.setHostName("127.0.0.1");
        db.setPort(port_);
        db.setDatabaseName("");
        // Auto-reconnect transparently after searchd drops the connection
        // (restart, idle timeout). Without this a stale connection reports
        // isOpen()==true, so the lazy `!isOpen() && !open()` guard never
        // re-opens it and queries fail hard instead of self-healing.
        db.setConnectOptions("MYSQL_OPT_RECONNECT=1");
    }

    return QSqlDatabase::database(threadConnName);
}

void Manticore::sleepWithEventLoop(int intervalMs)
{
    // Event-loop-friendly sleep: keep processing events (including QProcess
    // ready-read signals) while we wait for the next check. This replaces
    // QThread::msleep() which would block the event loop and cause Manticore
    // log output to be flushed only at the end.
    QEventLoop waitLoop;
    QTimer sleepTimer;
    sleepTimer.setSingleShot(true);
    QObject::connect(&sleepTimer, &QTimer::timeout, &waitLoop, &QEventLoop::quit);
    sleepTimer.start(intervalMs);
    waitLoop.exec();
}

bool Manticore::waitForReady(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();

    // Start with fast polling, then slow down.
    int checkInterval = kFastPollIntervalMs;
    int lastProgressLogSec = 0;

    qInfo() << "Waiting for Manticore to become ready...";

    QPointer<Manticore> self(this);

    while (timer.elapsed() < timeoutMs) {
        // Pump the event loop so that QProcess::readyReadStandard{Output,Error}
        // signals are delivered to onProcessReadyRead() and Manticore's log
        // messages are emitted in real time (in parallel with connection checks),
        // instead of being buffered until this function returns.
        QCoreApplication::processEvents(QEventLoop::AllEvents, kEventPumpBudgetMs);
        if (!self) {
            return false; // destroyed while waiting
        }

        // Check the PID file first (faster than a network connection).
        bool pidExists = QFile::exists(pidFilePath_);

        if (pidExists && testConnection()) {
            setStatus(Status::Running);
            qInfo() << "Manticore is ready on port" << port_ << "(took" << timer.elapsed() << "ms)";

            // Flush any remaining output from the process before returning.
            QCoreApplication::processEvents(QEventLoop::AllEvents, kEventPumpBudgetMs);

            // Start connection monitoring.
            connectionCheckTimer_->start();
            return true;
        }

        sleepWithEventLoop(checkInterval);
        if (!self) {
            return false;
        }

        // Increase the interval after the first second to reduce CPU usage.
        if (timer.elapsed() > kPollSlowdownAfterMs && checkInterval < kSlowPollIntervalMs) {
            checkInterval = kSlowPollIntervalMs;
        }

        // On Windows with daemon mode, we can't check the process state
        // as the original process has already exited.
        if (!isWindowsDaemonMode_) {
            // Check if the process crashed (only for non-daemon mode).
            if (process_ && process_->state() == QProcess::NotRunning) {
                QString output = process_->readAllStandardError();
                qWarning() << "Manticore stderr:" << output;
                fail("Manticore process exited unexpectedly");
                return false;
            }
        } else {
            // On Windows, check if the PID file was created as a sign of startup
            // progress.
            if (timer.elapsed() > kWindowsPidWarnMs && !pidExists) {
                qWarning() << "PID file not created after 5 seconds, searchd may have "
                              "failed to start";
            }
        }

        // Log progress every few seconds.
        int elapsedSec = static_cast<int>(timer.elapsed() / 1000);
        if (elapsedSec > 0 && elapsedSec != lastProgressLogSec && elapsedSec % kProgressLogIntervalSec == 0) {
            qInfo() << "Still waiting for Manticore..." << elapsedSec << "seconds elapsed";
            lastProgressLogSec = elapsedSec;
        }
    }

    fail("Timeout waiting for Manticore to start");
    return false;
}

void Manticore::onProcessStarted()
{
    qInfo() << "Manticore process started";
}

void Manticore::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qInfo() << "Manticore process finished with code" << exitCode << "status"
            << (exitStatus == QProcess::NormalExit ? "NormalExit" : "CrashExit");

#ifdef Q_OS_WIN
    // On Windows, searchd forks into the background and the parent exits with
    // code 0. This is normal behavior, not an error.
    if (isWindowsDaemonMode_ && exitCode == 0 && status_ == Status::Starting) {
        qInfo() << "Windows: searchd forked to background, waiting for connection...";
        return; // Don't change status, waitForReady() will handle it
    }
#endif

    if (status_ == Status::Running && exitStatus != QProcess::NormalExit) {
        qCritical() << "Manticore crashed with exit code" << exitCode;
    }

    // Only set stopped if we were actually running (not starting in daemon mode).
    if (status_ == Status::Running || (status_ == Status::Starting && !isWindowsDaemonMode_)) {
        setStatus(Status::Stopped);
    }
}

void Manticore::onProcessError(QProcess::ProcessError processError)
{
    switch (processError) {
    case QProcess::FailedToStart:
        fail("Failed to start searchd");
        break;
    case QProcess::Crashed:
        fail("searchd crashed");
        break;
    case QProcess::Timedout:
        fail("searchd timed out");
        break;
    default:
        fail("Unknown error with searchd");
    }
}

void Manticore::onProcessReadyRead()
{
    QString output = process_->readAllStandardOutput() + process_->readAllStandardError();

    for (const QString& line : output.split('\n', Qt::SkipEmptyParts)) {
        qDebug().noquote() << "[searchd]" << line.trimmed();

        // Parse version.
        QRegularExpression versionRx("Manticore ([0-9\\.]+)");
        QRegularExpressionMatch match = versionRx.match(line);
        if (match.hasMatch() && version_.isEmpty()) {
            version_ = match.captured(1);
            qInfo() << "Manticore version:" << version_;
        }

        // Check for "accepting connections".
        if (line.contains("accepting connections")) {
            qInfo() << "Manticore accepting connections";
        }
    }
}

void Manticore::checkConnection()
{
    if (!testConnection() && status_ == Status::Running) {
        fail("Lost connection to Manticore");
    }
}

bool Manticore::generateConfig()
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
)")
                         .arg(databasePath_)
                         .arg(port_)
                         .arg(dataDirectory_);

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

bool Manticore::createDatabaseDirectories()
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

QString Manticore::findSearchdPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();

    // Determine the platform-specific executable name, OS and architecture.
#ifdef Q_OS_WIN
    const QString execName = QStringLiteral("searchd.exe");
    const QString platform = QStringLiteral("win");
#elif defined(Q_OS_MACOS)
    const QString execName = QStringLiteral("searchd");
    const QString platform = QStringLiteral("darwin");
#else
    const QString execName = QStringLiteral("searchd");
    const QString platform = QStringLiteral("linux");
#endif

#if defined(Q_PROCESSOR_ARM)
    const QString arch = QStringLiteral("arm64");
#elif defined(Q_PROCESSOR_X86_64)
    const QString arch = QStringLiteral("x64");
#else
    const QString arch = QStringLiteral("ia32");
#endif

    const QString importsRelPath = QString("imports/%1/%2/%3").arg(platform, arch, execName);

    QStringList searchPaths;

    // Check near the executable first (for deployed builds).
    searchPaths << appDir + "/" + execName;

    // Check the imports directory from various relative positions
    // (covers both deployed and development builds from build/bin).
    const QStringList prefixes = {
        appDir + "/",
        appDir + "/../",
        appDir + "/../../",
        appDir + "/../../../",
        QString(),
        QStringLiteral("../"),
        QStringLiteral("../../"),
    };

    for (const QString& prefix : prefixes) {
        searchPaths << prefix + importsRelPath;
    }

    // System paths.
    searchPaths << QStringLiteral("/usr/bin/searchd") << QStringLiteral("/usr/local/bin/searchd");

    qInfo() << "Searching for searchd in:" << searchPaths;

    for (const QString& path : searchPaths) {
        if (QFile::exists(path)) {
            QString absPath = QFileInfo(path).absoluteFilePath();
            qInfo() << "Found searchd at:" << absPath;
            return absPath;
        }
    }

    // Try the PATH environment variable.
    QString found = QStandardPaths::findExecutable("searchd");
    if (!found.isEmpty()) {
        qInfo() << "Found searchd in PATH:" << found;
        return found;
    }

    qWarning() << "searchd not found! Searched paths:" << searchPaths;
    return QString();
}

bool Manticore::testConnection()
{
    // Use a temporary connection for testing with a short timeout.
    QString testConnName = connectionName_ + "_test";
    bool success = false;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL", testConnName);
        db.setHostName("127.0.0.1");
        db.setPort(port_);
        db.setDatabaseName("");
        // Set a short connection timeout (1 second instead of the default ~30s).
        db.setConnectOptions(QStringLiteral("MYSQL_OPT_CONNECT_TIMEOUT=%1").arg(kTestConnectTimeoutSec));

        if (db.open()) {
            QSqlQuery query(db);
            success = query.exec("SHOW STATUS");
            query.clear();
            db.close();
        } else {
            // Log the error once to help diagnose connection issues.
            static bool errorLogged = false;
            if (!errorLogged) {
                qWarning() << "testConnection: db.open() failed:" << db.lastError().text();
                qWarning() << "Available SQL drivers:" << QSqlDatabase::drivers();
                errorLogged = true;
            }
        }
    }

    // Remove the connection outside the scope where db was used.
    QSqlDatabase::removeDatabase(testConnName);
    return success;
}

void Manticore::setStatus(Status status)
{
    status_ = status;
}

void Manticore::fail(const QString& message)
{
    qCritical() << "Manticore:" << message;
    setStatus(Status::Error);
}

bool Manticore::isPortAvailable(int port)
{
    QTcpServer server;
    bool available = server.listen(QHostAddress::LocalHost, static_cast<quint16>(port));
    if (available) {
        server.close();
    }
    return available;
}

int Manticore::findAvailablePort(int startPort, int maxAttempts)
{
    for (int i = 0; i < maxAttempts; ++i) {
        int port = startPort + i;
        if (port > 65535)
            break;

        if (isPortAvailable(port)) {
            if (port != startPort) {
                qInfo() << "Port" << startPort << "is busy, using alternative port" << port;
            }
            return port;
        } else {
            qWarning() << "Port" << port << "is not available, trying next...";
        }
    }
    return -1; // No available port found
}

qint64 Manticore::computeDatabaseSizeBytes() const
{
    QFileInfo rootInfo(databasePath_);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        return 0;
    }

    qint64 total = 0;
    QDirIterator it(databasePath_, QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Hidden,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        if (fi.isFile()) {
            total += fi.size();
        }
    }
    return total;
}

int Manticore::computeStartupTimeoutMs() const
{
    // Tunable constants.
    //
    // Base is what an empty Manticore instance needs to come up. Per-MB is
    // a rough estimate of how much precaching slows down initial startup on
    // a typical SSD. Cap prevents pathological waits if the data directory
    // happens to be huge or on slow storage.
    constexpr int baseTimeoutMs = 30 * 1000; // 30 s
    constexpr int minTimeoutMs = 30 * 1000; // never wait less
    constexpr int maxTimeoutMs = 10 * 60 * 1000; // 10 min cap
    constexpr double msPerMB = 100.0; // +100 ms per MB of data

    const qint64 sizeBytes = computeDatabaseSizeBytes();
    const double sizeMB = static_cast<double>(sizeBytes) / (1024.0 * 1024.0);

    const double extraMs = sizeMB * msPerMB;
    qint64 timeoutMs = static_cast<qint64>(baseTimeoutMs) + static_cast<qint64>(extraMs);

    if (timeoutMs < minTimeoutMs)
        timeoutMs = minTimeoutMs;
    if (timeoutMs > maxTimeoutMs)
        timeoutMs = maxTimeoutMs;

    qInfo().nospace() << "Manticore startup timeout: " << timeoutMs
                      << " ms (database size: " << QString::number(sizeMB, 'f', 2) << " MB)";

    return static_cast<int>(timeoutMs);
}

} // namespace rats::data
