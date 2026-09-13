#include "single_instance.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>

#ifdef _WIN32
#include <windows.h>
#endif

namespace rats::bootstrap {

namespace {

// "activate" is the only message on this channel; the payload exists purely
// so the primary can tell a real ping from a probe that connects and dies.
const char kActivateMessage[] = "activate\n";

// A local-socket name has to be short and filesystem-safe (on Unix it becomes
// a socket file in the temp dir), so the data directory is hashed rather than
// embedded. Canonical path first, so /data and /data/ key the same instance.
QString serverNameFor(const QString& dataDirectory)
{
    const QDir dir(dataDirectory);
    QString path = dir.canonicalPath();
    if (path.isEmpty())
        path = dir.absolutePath();
#ifdef _WIN32
    path = path.toLower(); // Windows paths are case-insensitive
#endif
    const QByteArray digest = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex().left(16);
    return QStringLiteral("rats-search-") + QString::fromLatin1(digest);
}

} // namespace

SingleInstanceGuard::SingleInstanceGuard(const QString& dataDirectory, QObject* parent)
    : QObject(parent)
    , lockFilePath_(QDir(dataDirectory).absoluteFilePath(QStringLiteral("rats-search.lock")))
    , serverName_(serverNameFor(dataDirectory))
    , lockFile_(std::make_unique<QLockFile>(lockFilePath_))
{
}

SingleInstanceGuard::~SingleInstanceGuard()
{
    if (server_) {
        server_->close();
        QLocalServer::removeServer(serverName_);
    }
    // ~QLockFile unlocks; an abrupt kill leaves the file behind, and the next
    // launch reclaims it because the recorded pid is no longer running.
}

bool SingleInstanceGuard::tryAcquire()
{
    if (primary_)
        return true;

    if (!lockFile_->tryLock(0)) {
        // LockFailedError means a live owner. Any other error (permissions, an
        // unwritable data directory) says nothing about a second instance, so
        // we let the app start rather than block it on an environment problem.
        if (lockFile_->error() == QLockFile::LockFailedError)
            return false;
        qWarning() << "Single-instance lock unavailable at" << lockFilePath_ << "(error" << lockFile_->error()
                   << ") - continuing without the guard";
    }

    primary_ = true;
    startActivationServer();
    return true;
}

QString SingleInstanceGuard::runningInstanceInfo() const
{
    qint64 pid = 0;
    QString hostname;
    QString appname;
    if (!lockFile_->getLockInfo(&pid, &hostname, &appname))
        return QString();
    QString info = QStringLiteral("pid %1").arg(pid);
    if (!appname.isEmpty())
        info += QStringLiteral(", %1").arg(appname);
    return info;
}

bool SingleInstanceGuard::notifyRunningInstance(int timeoutMs)
{
#ifdef _WIN32
    // Let the running instance steal focus from us; without this Windows only
    // flashes its taskbar button when it calls activateWindow().
    AllowSetForegroundWindow(ASFW_ANY);
#endif
    QLocalSocket socket;
    socket.connectToServer(serverName_);
    if (!socket.waitForConnected(timeoutMs))
        return false;
    socket.write(kActivateMessage);
    const bool sent = socket.waitForBytesWritten(timeoutMs);
    socket.disconnectFromServer();
    return sent;
}

void SingleInstanceGuard::startActivationServer()
{
    server_ = new QLocalServer(this);
    // Holding the lock file proves any leftover socket belongs to a dead
    // instance (Unix does not clean those up), so removing it is safe here.
    QLocalServer::removeServer(serverName_);
    if (!server_->listen(serverName_)) {
        qWarning() << "Single-instance activation channel unavailable:" << server_->errorString();
        delete server_;
        server_ = nullptr;
        return;
    }

    connect(server_, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket* socket = server_->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
            socket->waitForReadyRead(500);
            if (socket->readAll().startsWith("activate")) {
                qInfo() << "Another Rats Search instance was started - raising this one";
                emit secondInstanceStarted();
            }
            socket->disconnectFromServer();
        }
    });
}

} // namespace rats::bootstrap
