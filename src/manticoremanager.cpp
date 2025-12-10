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

ManticoreManager::ManticoreManager(const QString& dataDirectory, QObject *parent)
    : QObject(parent)
    , dataDirectory_(dataDirectory)
    , port_(9306)  // Default MySQL port for Manticore
    , status_(Status::Stopped)
    , isExternalInstance_(false)
{
    databasePath_ = dataDirectory_ + "/database";
    configPath_ = dataDirectory_ + "/sphinx.conf";
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
    stop();
    
    // Remove database connection
    if (QSqlDatabase::contains(connectionName_)) {
        QSqlDatabase::removeDatabase(connectionName_);
    }
}

bool ManticoreManager::start()
{
    if (status_ == Status::Running) {
        return true;
    }
    
    // Check if external instance is running
    if (testConnection()) {
        qInfo() << "Found existing Manticore instance on port" << port_;
        isExternalInstance_ = true;
        setStatus(Status::Running);
        emit started();
        return true;
    }
    
    // Find searchd executable
    searchdPath_ = findSearchdPath();
    if (searchdPath_.isEmpty()) {
        emit error("Cannot find searchd executable");
        setStatus(Status::Error);
        return false;
    }
    
    qInfo() << "Found searchd at:" << searchdPath_;
    
    // Create directories and config
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
    
    // Start process
    setStatus(Status::Starting);
    
    QStringList args;
    args << "--config" << configPath_;
    
#ifndef Q_OS_WIN
    args << "--nodetach";
#endif
    
    process_->start(searchdPath_, args);
    
    if (!process_->waitForStarted(5000)) {
        emit error("Failed to start searchd process");
        setStatus(Status::Error);
        return false;
    }
    
    // Wait for ready
    return waitForReady(30000);
}

void ManticoreManager::stop()
{
    connectionCheckTimer_->stop();
    
    if (isExternalInstance_) {
        qInfo() << "Not stopping external Manticore instance";
        setStatus(Status::Stopped);
        emit stopped();
        return;
    }
    
    if (process_ && process_->state() != QProcess::NotRunning) {
        qInfo() << "Stopping Manticore...";
        
#ifdef Q_OS_WIN
        // On Windows, use stopwait command
        QProcess stopProcess;
        QStringList args;
        args << "--config" << configPath_ << "--stopwait";
        stopProcess.start(searchdPath_, args);
        stopProcess.waitForFinished(10000);
#else
        process_->terminate();
        if (!process_->waitForFinished(5000)) {
            process_->kill();
            process_->waitForFinished(2000);
        }
#endif
    }
    
    setStatus(Status::Stopped);
    emit stopped();
}

bool ManticoreManager::isRunning() const
{
    return status_ == Status::Running;
}

QSqlDatabase ManticoreManager::getDatabase() const
{
    if (!QSqlDatabase::contains(connectionName_)) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL", connectionName_);
        db.setHostName("127.0.0.1");
        db.setPort(port_);
        db.setDatabaseName("");
    }
    
    return QSqlDatabase::database(connectionName_);
}

bool ManticoreManager::waitForReady(int timeoutMs)
{
    int elapsed = 0;
    const int checkInterval = 100;
    
    while (elapsed < timeoutMs) {
        if (testConnection()) {
            setStatus(Status::Running);
            emit started();
            qInfo() << "Manticore is ready on port" << port_;
            return true;
        }
        QThread::msleep(checkInterval);
        elapsed += checkInterval;
        
        // Check if process crashed
        if (process_ && process_->state() == QProcess::NotRunning) {
            emit error("Manticore process exited unexpectedly");
            setStatus(Status::Error);
            return false;
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
    qInfo() << "Manticore process finished with code" << exitCode;
    
    if (status_ == Status::Running && exitStatus != QProcess::NormalExit) {
        emit error(QString("Manticore crashed with exit code %1").arg(exitCode));
    }
    
    setStatus(Status::Stopped);
    emit stopped();
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
    
    // Check near executable first
    QString appDir = QCoreApplication::applicationDirPath();
    searchPaths << appDir + "/searchd"
                << appDir + "/searchd.exe"
                << appDir + "/../imports/searchd"
                << appDir + "/../imports/searchd.exe";
    
    // Check imports directory based on platform
#ifdef Q_OS_WIN
    #ifdef Q_PROCESSOR_X86_64
    searchPaths << appDir + "/../imports/win/x64/searchd.exe"
                << "imports/win/x64/searchd.exe";
    #else
    searchPaths << appDir + "/../imports/win/ia32/searchd.exe"
                << "imports/win/ia32/searchd.exe";
    #endif
#elif defined(Q_OS_MACOS)
    #ifdef Q_PROCESSOR_ARM
    searchPaths << appDir + "/../imports/darwin/arm64/searchd"
                << "imports/darwin/arm64/searchd";
    #else
    searchPaths << appDir + "/../imports/darwin/x64/searchd"
                << "imports/darwin/x64/searchd";
    #endif
#else  // Linux
    #ifdef Q_PROCESSOR_X86_64
    searchPaths << appDir + "/../imports/linux/x64/searchd"
                << "imports/linux/x64/searchd";
    #else
    searchPaths << appDir + "/../imports/linux/ia32/searchd"
                << "imports/linux/ia32/searchd";
    #endif
#endif
    
    // System paths
    searchPaths << "/usr/bin/searchd"
                << "/usr/local/bin/searchd";
    
    for (const QString& path : searchPaths) {
        if (QFile::exists(path)) {
            return QFileInfo(path).absoluteFilePath();
        }
    }
    
    // Try PATH
    QString found = QStandardPaths::findExecutable("searchd");
    if (!found.isEmpty()) {
        return found;
    }
    
    return QString();
}

bool ManticoreManager::testConnection()
{
    QSqlDatabase db = getDatabase();
    
    if (!db.open()) {
        return false;
    }
    
    QSqlQuery query(db);
    if (!query.exec("SHOW STATUS")) {
        db.close();
        return false;
    }
    
    return true;
}

void ManticoreManager::setStatus(Status status)
{
    if (status_ != status) {
        status_ = status;
        emit statusChanged(status);
    }
}

