#include "services/database_sync_service.h"

#include "data/torrent_repository.h"
#include "net/p2p_transport.h"
#include "services/database_dump.h"
#include "services/database_exporter.h"
#include "services/indexing_service.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMetaObject>
#include <QMutexLocker>
#include <QSet>
#include <QStorageInfo>
#include <QTimer>
#include <QtConcurrent>

#include <utility>

namespace rats::service {

namespace {

// --- timeouts --------------------------------------------------------------
//
// Every deadline below measures *silence*, never work. That is the whole point of
// the heartbeat: how long a peer takes to build a snapshot depends on the size of
// its index, so no constant can be right for both a 50k and a 3M-row database.
// How long it may go quiet does not.

// A peer must answer databaseRequest at all.
constexpr qint64 kResponseTimeoutMs = 60 * 1000;
// Maximum silence while a peer is building its snapshot. Reset by every
// databaseProgress, so this is "the peer stopped talking", not "the peer is slow".
constexpr qint64 kPrepareSilenceMs = 3 * 60 * 1000;
// Once the peer says the snapshot is ready, the file offer should follow promptly.
constexpr qint64 kOfferTimeoutMs = 3 * 60 * 1000;
// How often we heartbeat to the peers waiting on a snapshot we are building.
constexpr qint64 kHeartbeatMs = 10 * 1000;
// Watchdog tick. Every deadline is checked here rather than by its own one-shot
// timer, so a timer can never outlive the operation that armed it.
constexpr int kWatchdogMs = 1000;

// --- serving limits --------------------------------------------------------

// Concurrent outgoing database transfers. Each one is a multi-hundred-megabyte
// upload; serving the whole swarm at once helps nobody.
constexpr int kMaxConcurrentServes = 3;
// Minimum gap between two accepted requests from the same peer. Serving is cheap
// now that it is a file already on disk, so this only exists to stop a peer
// looping on it.
constexpr qint64 kMinRequestIntervalMs = 30 * 1000;
// Room we insist on having beyond the estimated dump size before generating one.
constexpr double kFreeSpaceMargin = 1.25;

// --- housekeeping ----------------------------------------------------------

// Leftovers in the transfer directory older than this are swept at startup.
constexpr qint64 kStaleFileAgeSecs = 24 * 60 * 60;
// Floor between two progress signals. An import of a million torrents commits two
// thousand batches; without this each one becomes a websocket broadcast and a
// status-bar repaint.
constexpr qint64 kProgressIntervalMs = 300;

qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

// Torrents in a dump, from its header. The snapshot metadata only records this
// for the live generation, and a peer continuing an interrupted download is
// served an older one — whose header is right there in the first few bytes.
qint64 torrentsInDump(const QString& path)
{
    DumpReader reader;
    if (!reader.open(path))
        return 0;
    return reader.header().torrents;
}

QString operationName(DatabaseSyncService::Operation operation)
{
    switch (operation) {
    case DatabaseSyncService::Operation::Export:
        return QStringLiteral("export");
    case DatabaseSyncService::Operation::Import:
        return QStringLiteral("import");
    case DatabaseSyncService::Operation::PeerPull:
        return QStringLiteral("peerPull");
    case DatabaseSyncService::Operation::None:
        break;
    }
    return QStringLiteral("idle");
}

QJsonObject statusToJson(const DatabaseSyncService::Status& s)
{
    QJsonObject obj;
    obj["operation"] = operationName(s.operation);
    obj["running"] = s.running;
    obj["stage"] = s.stage;
    obj["path"] = s.path;
    obj["peer"] = s.peerId;
    obj["processed"] = static_cast<double>(s.processed);
    obj["total"] = static_cast<double>(s.total);
    obj["inserted"] = static_cast<double>(s.inserted);
    obj["merged"] = static_cast<double>(s.merged);
    obj["rejected"] = static_cast<double>(s.rejected);
    obj["bytes"] = static_cast<double>(s.bytes);
    obj["totalBytes"] = static_cast<double>(s.totalBytes);
    if (s.resumedFrom > 0)
        obj["resumedFrom"] = static_cast<double>(s.resumedFrom);
    if (!s.error.isEmpty())
        obj["error"] = s.error;
    return obj;
}

} // namespace

DatabaseSyncService::DatabaseSyncService(data::TorrentRepository* repository, IndexingService* indexing,
    net::P2PTransport* transport, QString dataDirectory, QString clientVersion, QObject* parent)
    : QObject(parent)
    , repository_(repository)
    , indexing_(indexing)
    , transport_(transport)
    , dataDirectory_(std::move(dataDirectory))
    , clientVersion_(std::move(clientVersion))
    , snapshot_(QDir(dataDirectory_).absoluteFilePath(QStringLiteral("dbsync")))
{
    QDir().mkpath(transferDirectory());
    sweepTransferDirectory();
    snapshot_.load();

    if (transport_) {
        connect(transport_, &net::P2PTransport::fileOffered, this, &DatabaseSyncService::onFileOffered);
        connect(transport_, &net::P2PTransport::fileTransferProgress, this, &DatabaseSyncService::onTransferProgress);
        connect(transport_, &net::P2PTransport::fileTransferFinished, this, &DatabaseSyncService::onTransferFinished);
        connect(transport_, &net::P2PTransport::peerDisconnected, this, &DatabaseSyncService::onPeerDisconnected);
    }

    auto* watchdog = new QTimer(this);
    connect(watchdog, &QTimer::timeout, this, &DatabaseSyncService::onWatchdog);
    watchdog->start(kWatchdogMs);
}

DatabaseSyncService::~DatabaseSyncService()
{
    shutdown();
}

// ===========================================================================
// Status
// ===========================================================================

bool DatabaseSyncService::isBusy() const
{
    return local_ != nullptr;
}

DatabaseSyncService::Status DatabaseSyncService::status() const
{
    QMutexLocker lock(&mutex_);
    return status_;
}

DatabaseSyncService::ServeStatus DatabaseSyncService::serveStatus() const
{
    ServeStatus s;
    if (const SnapshotPtr job = snapshotJob_) {
        QMutexLocker lock(&mutex_);
        s.generating = true;
        s.processed = job->processed;
        s.total = job->total;
    }
    for (const Serve& serve : serves_) {
        if (serve.waitingForSnapshot)
            ++s.waiting;
        else if (serve.transferId != 0)
            ++s.sending;
    }
    return s;
}

QJsonObject DatabaseSyncService::statusJson() const
{
    QJsonObject obj = statusToJson(status());

    const ServeStatus serve = serveStatus();
    QJsonObject serveObj;
    serveObj["generating"] = serve.generating;
    serveObj["processed"] = static_cast<double>(serve.processed);
    serveObj["total"] = static_cast<double>(serve.total);
    serveObj["waiting"] = serve.waiting;
    serveObj["sending"] = serve.sending;
    obj["serve"] = serveObj;

    const DatabaseSnapshot::Info info = snapshot_.info();
    QJsonObject snapObj;
    snapObj["valid"] = info.valid;
    snapObj["torrents"] = static_cast<double>(info.torrents);
    snapObj["bytes"] = static_cast<double>(info.bytes);
    if (info.created.isValid())
        snapObj["created"] = info.created.toString(Qt::ISODate);
    snapObj["fresh"] = repository_ && snapshot_.isFresh(repository_->statistics().torrents);
    obj["snapshot"] = snapObj;

    obj["sharing"] = sharingEnabled_;
    const QJsonObject pending = pendingImportJson();
    if (!pending.isEmpty())
        obj["pendingImport"] = pending;
    const QJsonArray transfers = pendingTransfersJson();
    if (!transfers.isEmpty())
        obj["pendingTransfers"] = transfers;
    return obj;
}

QJsonArray DatabaseSyncService::pendingTransfersJson() const
{
    QJsonArray out;
    const QDir dir(transferDirectory());
    const QFileInfoList notes
        = dir.entryInfoList({ QStringLiteral("incoming-*.ratsdb.part.json") }, QDir::Files, QDir::Time);
    for (const QFileInfo& note : notes) {
        const DatabaseTransferResume state = DatabaseTransferResume::load(note.absoluteFilePath());
        // The note is a claim about a file; the file is what makes it a resume
        // point. chopped() strips the ".json" that turns the one name into the other.
        const QFileInfo partial(note.absoluteFilePath().chopped(5));
        if (!state.isValid() || !partial.exists())
            continue;
        if (state.offsetFor(partial.size(), state.peerId, state.snapshot, state.size) <= 0)
            continue;
        out.append(QJsonObject { { "peer", state.peerId }, { "snapshot", state.snapshot },
            { "bytes", static_cast<double>(partial.size()) }, { "totalBytes", static_cast<double>(state.size) },
            { "path", partial.absoluteFilePath() } });
    }
    return out;
}

QJsonObject DatabaseSyncService::pendingImportJson() const
{
    QFile file(resumeStatePath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    const QString path = obj["path"].toString();
    if (path.isEmpty())
        return {};

    // Only offer to resume something that is still there and still the same file.
    const QFileInfo info(path);
    if (!info.exists() || info.size() != obj["size"].toVariant().toLongLong())
        return {};
    return obj;
}

void DatabaseSyncService::publishLocal()
{
    emit syncProgress(statusToJson(status()));
}

void DatabaseSyncService::publishServe()
{
    const ServeStatus serve = serveStatus();
    QJsonObject obj;
    obj["generating"] = serve.generating;
    obj["processed"] = static_cast<double>(serve.processed);
    obj["total"] = static_cast<double>(serve.total);
    obj["waiting"] = serve.waiting;
    obj["sending"] = serve.sending;
    emit serveProgress(obj);
}

void DatabaseSyncService::setStage(const LocalPtr& op, const QString& stage)
{
    if (local_ != op)
        return;
    {
        QMutexLocker lock(&mutex_);
        status_.stage = stage;
    }
    publishLocal();
}

void DatabaseSyncService::setSnapshotPolicy(const DatabaseSnapshot::Policy& policy)
{
    snapshot_.setPolicy(policy);
}

void DatabaseSyncService::setSharingEnabled(bool enabled)
{
    sharingEnabled_ = enabled;
    if (!enabled) {
        const QStringList peers = serves_.keys();
        for (const QString& peerId : peers)
            dropServe(peerId, false, QStringLiteral("sharing disabled"));
        cancelSnapshotIfUnwanted();
    }
}

// ===========================================================================
// The user's lane
// ===========================================================================

DatabaseSyncService::LocalPtr DatabaseSyncService::beginLocal(
    Operation operation, const QString& path, const QString& peerId, QString* error)
{
    if (local_) {
        if (error)
            *error = tr("A database operation is already running.");
        return nullptr;
    }

    auto op = std::make_shared<LocalOperation>();
    op->id = ++nextOperationId_;
    op->operation = operation;
    local_ = op;

    {
        QMutexLocker lock(&mutex_);
        status_ = Status {};
        status_.operation = operation;
        status_.running = true;
        status_.path = path;
        status_.peerId = peerId;
    }
    emit syncStarted(statusToJson(status()));
    return op;
}

void DatabaseSyncService::finishLocal(const LocalPtr& op, bool success, const QString& error)
{
    // The identity check is the whole reason an operation is an object. A worker
    // that was cancelled, or a deadline belonging to an attempt that has already
    // been replaced, arrives here holding *its* operation — and finishes nothing.
    if (!op || local_ != op)
        return;

    // Tear down anything this operation still owns on the wire.
    if (transport_ && op->transferId != 0 && !op->peerId.isEmpty())
        transport_->cancelFile(op->peerId, op->transferId);
    if (op->peerEngaged && !success && transport_ && !op->peerId.isEmpty()) {
        // Tell the peer to stop. Without this it keeps building a snapshot for a
        // requester that is no longer listening, holds a serving slot while doing
        // it, and then offers a file that gets rejected — which is what used to put
        // a failure dialog on the *sharing* user's screen. Gated on the peer having
        // accepted: there is nothing to cancel after a refusal.
        transport_->sendMessage(op->peerId, QStringLiteral("databaseCancel"),
            QJsonObject { { "session", static_cast<double>(op->sessionId) } });
    }

    QJsonObject summary;
    {
        QMutexLocker lock(&mutex_);
        status_.running = false;
        status_.stage = success ? QStringLiteral("done") : QStringLiteral("failed");
        status_.error = error;
        summary = statusToJson(status_);
        status_.operation = Operation::None;
    }
    summary["success"] = success;

    local_.reset();
    emit syncFinished(success, summary);
}

void DatabaseSyncService::cancel()
{
    const LocalPtr op = local_;
    if (!op)
        return;
    op->cancel.cancel();
    setStage(op, QStringLiteral("cancelling"));

    // A worker notices the token and finishes the operation itself. An operation
    // that has no worker — one merely waiting on a peer — has nobody to notice,
    // so it is finished here.
    if (!op->worker.isRunning())
        finishLocal(op, false, tr("Cancelled."));
}

void DatabaseSyncService::shutdown()
{
    if (local_) {
        local_->cancel.cancel();
        if (local_->worker.isRunning())
            local_->worker.waitForFinished();
        local_.reset();
    }
    if (snapshotJob_) {
        snapshotJob_->cancel.cancel();
        if (snapshotJob_->worker.isRunning())
            snapshotJob_->worker.waitForFinished();
        snapshotJob_.reset();
    }
}

// --- export ----------------------------------------------------------------

bool DatabaseSyncService::exportToFile(const QString& path, QString* error)
{
    if (path.isEmpty()) {
        if (error)
            *error = tr("No output path given.");
        return false;
    }
    if (!repository_) {
        if (error)
            *error = tr("Database is not available.");
        return false;
    }

    const qint64 torrents = repository_->statistics().torrents;
    const qint64 needed = static_cast<qint64>(torrents * snapshot_.bytesPerTorrent() * kFreeSpaceMargin);
    const qint64 free = availableBytes(QFileInfo(path).absolutePath());
    if (free >= 0 && free < needed) {
        if (error) {
            *error = tr("Not enough free space: the export needs about %1 MB, %2 MB are available.")
                         .arg(needed / (1024 * 1024))
                         .arg(free / (1024 * 1024));
        }
        return false;
    }

    const LocalPtr op = beginLocal(Operation::Export, path, QString(), error);
    if (!op)
        return false;

    {
        QMutexLocker lock(&mutex_);
        status_.total = torrents;
        status_.stage = QStringLiteral("exporting");
    }
    publishLocal();

    op->worker = QtConcurrent::run([this, op, path]() { runExport(op, path); });
    return true;
}

void DatabaseSyncService::runExport(const LocalPtr& op, const QString& path)
{
    dump::Header header;
    header.client = clientVersion_;
    header.peerId = transport_ ? transport_->ourPeerId() : QString();
    header.created = QDateTime::currentDateTime();
    header.torrents = repository_->statistics().torrents;

    qint64 lastPublish = 0;
    DatabaseExporter exporter(repository_);
    const DatabaseExporter::Result result
        = exporter.run(path, header, op->cancel, [this, op, &lastPublish](qint64 torrents, qint64 bytes) {
              {
                  QMutexLocker lock(&mutex_);
                  status_.processed = torrents;
                  status_.bytes = bytes;
              }
              const qint64 now = nowMs();
              if (now - lastPublish < kProgressIntervalMs)
                  return;
              lastPublish = now;
              QMetaObject::invokeMethod(
                  this,
                  [this, op]() {
                      if (local_ == op)
                          publishLocal();
                  },
                  Qt::QueuedConnection);
          });

    QMetaObject::invokeMethod(
        this,
        [this, op, result]() {
            if (result.ok) {
                QMutexLocker lock(&mutex_);
                status_.processed = result.torrents;
                status_.bytes = result.bytes;
                status_.totalBytes = result.bytes;
            }
            if (op->cancel.cancelled()) {
                finishLocal(op, false, tr("Cancelled."));
                return;
            }
            finishLocal(op, result.ok, result.error);
        },
        Qt::QueuedConnection);
}

// --- import ----------------------------------------------------------------

bool DatabaseSyncService::importFromFile(const QString& path, const ImportOptions& options, QString* error)
{
    if (!QFile::exists(path)) {
        if (error)
            *error = tr("File does not exist: %1").arg(path);
        return false;
    }
    if (!indexing_) {
        if (error)
            *error = tr("Indexing is not available.");
        return false;
    }

    const LocalPtr op = beginLocal(Operation::Import, path, QString(), error);
    if (!op)
        return false;
    op->worker = QtConcurrent::run([this, op, path, options]() { runImport(op, path, options); });
    return true;
}

void DatabaseSyncService::runImport(const LocalPtr& op, const QString& path, const ImportOptions& options)
{
    QMetaObject::invokeMethod(this, [this, op]() { setStage(op, QStringLiteral("importing")); }, Qt::QueuedConnection);

    const qint64 fileSize = QFileInfo(path).size();

    DatabaseImporter::Options importOptions;
    importOptions.applyFilters = options.applyFilters;
    if (options.resume)
        importOptions.startOffset = loadResumeOffset(path, fileSize);

    {
        QMutexLocker lock(&mutex_);
        status_.totalBytes = fileSize;
    }

    qint64 lastPublish = 0;
    DatabaseImporter importer(indexing_);
    const DatabaseImporter::Result result = importer.run(path, importOptions, op->cancel,
        [this, op, path, fileSize, &lastPublish](const DatabaseImporter::Result& progress) {
            // Persisted after the batch it follows is committed, so a resume can
            // repeat work but never skip it.
            saveResumeState(path, progress.offset, fileSize);
            {
                QMutexLocker lock(&mutex_);
                status_.processed = progress.processed;
                status_.inserted = progress.inserted;
                status_.merged = progress.merged;
                status_.rejected = progress.rejected;
                status_.bytes = progress.offset;
            }
            const qint64 now = nowMs();
            if (now - lastPublish < kProgressIntervalMs)
                return;
            lastPublish = now;
            QMetaObject::invokeMethod(
                this,
                [this, op]() {
                    if (local_ == op)
                        publishLocal();
                },
                Qt::QueuedConnection);
        });

    if (!result.cancelled)
        clearResumeState();

    QMetaObject::invokeMethod(
        this,
        [this, op, path, options, result]() {
            {
                QMutexLocker lock(&mutex_);
                status_.processed = result.processed;
                status_.inserted = result.inserted;
                status_.merged = result.merged;
                status_.rejected = result.rejected;
            }
            if (result.cancelled) {
                finishLocal(op, false, tr("Cancelled — the import can be resumed."));
                return;
            }
            if (!result.ok) {
                finishLocal(op, false, result.error.isEmpty() ? tr("The dump could not be read.") : result.error);
                return;
            }

            if (options.removeWhenDone)
                QFile::remove(path);
            qInfo() << "[DatabaseSync] import finished:" << result.inserted << "new," << result.merged
                    << "already known";
            if (result.collided > 0) {
                // A birthday collision on the 63-bit row id. Expected to be zero on
                // any realistic index, so say so loudly if it is not.
                qWarning() << "[DatabaseSync]" << result.collided
                           << "torrents skipped: their row id belongs to a different torrent";
            }
            if (result.truncated)
                emit statusMessage(tr("Database import finished, but the dump was truncated."), 8000);
            finishLocal(op, true);
        },
        Qt::QueuedConnection);
}

// ===========================================================================
// Pulling from a peer
// ===========================================================================

bool DatabaseSyncService::requestFromPeer(const QString& peerId, const ImportOptions& options, QString* error)
{
    if (!transport_ || !transport_->isRunning()) {
        if (error)
            *error = tr("P2P is not running.");
        return false;
    }
    if (!transport_->isFileTransferAvailable()) {
        if (error)
            *error = tr("File transfer is not available.");
        return false;
    }
    if (peerId.isEmpty()) {
        if (error)
            *error = tr("No peer given.");
        return false;
    }

    const LocalPtr op = beginLocal(Operation::PeerPull, QString(), peerId, error);
    if (!op)
        return false;

    op->peerId = peerId;
    op->importOptions = options;
    op->sessionId = ++nextSessionId_;

    QJsonObject request { { "session", static_cast<double>(op->sessionId) } };

    // Name the generation we already hold bytes of. Without this the peer answers
    // with whatever its newest snapshot is, and a snapshot it rebuilt in the
    // meantime is a different file that the partial is no prefix of — the pull
    // would start from zero however much of the old one is on disk.
    const DatabaseTransferResume have = DatabaseTransferResume::load(partialStatePathFor(peerId));
    const qint64 partial = QFileInfo(partialPathFor(peerId)).size();
    if (have.offsetFor(partial, peerId, have.snapshot, have.size) > 0) {
        request["have"] = QJsonObject { { "snapshot", have.snapshot }, { "bytes", static_cast<double>(partial) } };
        qInfo() << "[DatabaseSync] asking" << peerId.left(8) << "to continue" << have.snapshot << "from" << partial
                << "bytes";
    }

    if (!transport_->sendMessage(peerId, QStringLiteral("databaseRequest"), request)) {
        finishLocal(op, false, tr("Could not reach the peer."));
        return false;
    }

    op->deadlineMs = nowMs() + kResponseTimeoutMs;
    setStage(op, QStringLiteral("waiting"));
    emit statusMessage(tr("Asked %1 for its database…").arg(peerId.left(8)), 5000);
    return true;
}

void DatabaseSyncService::handlePeerResponse(const QString& peerId, const QJsonObject& data)
{
    const LocalPtr op = local_;
    if (!op || op->operation != Operation::PeerPull || op->peerId != peerId)
        return;
    if (data["session"].toVariant().toULongLong() != op->sessionId)
        return; // an answer to an attempt we have already abandoned
    if (op->transferId != 0)
        return; // the file beat the answer here; the transfer is already running

    if (!data["accepted"].toBool(false)) {
        const QString reason = data["reason"].toString();
        const int retryAfter = data["retryAfter"].toInt();
        QString message = reason.isEmpty() ? tr("The peer refused to share its database.")
                                           : tr("The peer refused to share its database: %1").arg(reason);
        if (retryAfter > 0)
            message += QLatin1Char(' ') + tr("Try again in %n second(s).", nullptr, retryAfter);
        finishLocal(op, false, message);
        return;
    }

    // Which generation this is. An older peer sends none, and a pull from one can
    // never be continued — there would be no way to tell one of its dumps from the
    // next, and continuing into the wrong one wastes the whole download.
    op->snapshotId = data["snapshot"].toString();

    {
        QMutexLocker lock(&mutex_);
        status_.total = data["torrents"].toVariant().toLongLong();
        status_.totalBytes = data["bytes"].toVariant().toLongLong();
    }

    op->peerEngaged = true;

    if (data["ready"].toBool(false)) {
        // The peer already has a snapshot; the offer is seconds away.
        op->deadlineMs = nowMs() + kOfferTimeoutMs;
        setStage(op, QStringLiteral("awaitingOffer"));
        return;
    }

    // The peer has to build one. From here the deadline measures silence and is
    // pushed forward by every heartbeat.
    op->deadlineMs = nowMs() + kPrepareSilenceMs;
    setStage(op, QStringLiteral("preparing"));
    emit statusMessage(tr("%1 is preparing its database…").arg(peerId.left(8)), 5000);
}

void DatabaseSyncService::handlePeerProgress(const QString& peerId, const QJsonObject& data)
{
    const LocalPtr op = local_;
    if (!op || op->operation != Operation::PeerPull || op->peerId != peerId)
        return;
    if (data["session"].toVariant().toULongLong() != op->sessionId)
        return;

    op->deadlineMs = nowMs() + kPrepareSilenceMs;
    {
        QMutexLocker lock(&mutex_);
        status_.processed = data["processed"].toVariant().toLongLong();
        const qint64 total = data["total"].toVariant().toLongLong();
        if (total > 0)
            status_.total = total;
    }
    setStage(op, QStringLiteral("preparing"));
}

void DatabaseSyncService::startPullImport(const LocalPtr& op, const QString& dumpPath)
{
    ImportOptions options = op->importOptions;
    // Kept, not forced off: an import of a received dump that is interrupted
    // half-way should be resumable like any other, and the file is only deleted
    // once it has actually been merged in full.
    options.resume = true;
    options.removeWhenDone = true;

    {
        QMutexLocker lock(&mutex_);
        status_.operation = Operation::Import;
        status_.path = dumpPath;
        status_.processed = 0;
    }
    op->operation = Operation::Import;
    op->deadlineMs = 0;
    op->worker = QtConcurrent::run([this, op, dumpPath, options]() { runImport(op, dumpPath, options); });
}

// ===========================================================================
// Serving other peers
// ===========================================================================

void DatabaseSyncService::refuse(const QString& peerId, quint64 sessionId, const QString& reason, int retryAfterSecs)
{
    if (!transport_)
        return;
    QJsonObject payload { { "session", static_cast<double>(sessionId) }, { "accepted", false }, { "reason", reason } };
    if (retryAfterSecs > 0)
        payload["retryAfter"] = retryAfterSecs;
    transport_->sendMessage(peerId, QStringLiteral("databaseRequest_response"), payload);
}

void DatabaseSyncService::handlePeerRequest(const QString& peerId, const QJsonObject& data)
{
    const quint64 sessionId = data["session"].toVariant().toULongLong();

    if (!sharingEnabled_) {
        refuse(peerId, sessionId, QStringLiteral("sharing disabled"));
        return;
    }
    if (!transport_ || !transport_->isFileTransferAvailable()) {
        refuse(peerId, sessionId, QStringLiteral("file transfer unavailable"));
        return;
    }

    const qint64 torrents = repository_ ? repository_->statistics().torrents : 0;
    if (torrents <= 0) {
        refuse(peerId, sessionId, QStringLiteral("empty database"));
        return;
    }

    const qint64 now = nowMs();
    const qint64 last = lastRequestMs_.value(peerId, 0);
    if (last != 0 && now - last < kMinRequestIntervalMs && !serves_.contains(peerId)) {
        refuse(peerId, sessionId, QStringLiteral("too many requests"),
            static_cast<int>((kMinRequestIntervalMs - (now - last)) / 1000) + 1);
        return;
    }

    // A repeat request from a peer we are already serving replaces the old serve:
    // the peer restarted its attempt, so the transfer it is no longer expecting is
    // dead weight. The snapshot is explicitly *not* released — this peer is about
    // to become a waiter again, and letting the drop stop the generation would
    // cancel the very snapshot the new request needs.
    if (serves_.contains(peerId))
        dropServe(peerId, false, QStringLiteral("superseded by a new request"), /*releaseSnapshot*/ false);

    if (serves_.size() >= kMaxConcurrentServes) {
        refuse(peerId, sessionId, QStringLiteral("busy"), 60);
        return;
    }

    lastRequestMs_.insert(peerId, now);

    Serve serve;
    serve.peerId = peerId;
    serve.sessionId = sessionId;
    serve.startedMs = now;

    // The peer is continuing a download of a particular generation. If we still
    // have that file, serve it — freshness does not enter into it, because most of
    // a slightly stale dump already on their disk beats a newer one they would
    // have to fetch from the first byte. pathForGeneration() is what keeps a
    // peer-supplied name from naming anything but a dump of ours.
    const QString wanted = data["have"].toObject()["snapshot"].toString();
    const QString wantedPath = wanted.isEmpty() ? QString() : snapshot_.pathForGeneration(wanted);
    if (!wantedPath.isEmpty()) {
        serve.file = wantedPath;
        serves_.insert(peerId, serve);
        qInfo() << "[DatabaseSync]" << peerId.left(8) << "is continuing" << wanted;
        offerSnapshot(serves_[peerId]);
        return;
    }

    if (snapshot_.isFresh(torrents)) {
        serves_.insert(peerId, serve);
        offerSnapshot(serves_[peerId]);
        return;
    }

    if (!ensureSnapshotJob()) {
        refuse(peerId, sessionId, QStringLiteral("cannot prepare a database snapshot"), 300);
        return;
    }

    serve.waitingForSnapshot = true;
    serves_.insert(peerId, serve);

    transport_->sendMessage(peerId, QStringLiteral("databaseRequest_response"),
        QJsonObject { { "session", static_cast<double>(sessionId) }, { "accepted", true }, { "ready", false },
            { "torrents", static_cast<double>(torrents) }, { "format", static_cast<int>(dump::kFormatVersion) } });

    qInfo() << "[DatabaseSync]" << peerId.left(8) << "is waiting for a snapshot of" << torrents << "torrents";
    emit statusMessage(tr("Preparing the database for %1…").arg(peerId.left(8)), 5000);
    publishServe();
}

void DatabaseSyncService::handlePeerCancel(const QString& peerId, const QJsonObject& data)
{
    auto it = serves_.find(peerId);
    if (it == serves_.end())
        return;
    if (data.contains("session") && data["session"].toVariant().toULongLong() != it->sessionId)
        return;
    qInfo() << "[DatabaseSync]" << peerId.left(8) << "cancelled its database request";
    dropServe(peerId, false, QStringLiteral("cancelled by the peer"));
}

void DatabaseSyncService::offerSnapshot(Serve& serve)
{
    // The answer to the request is sent from here (see below), so a failure before
    // that point has to be said out loud: otherwise the requester sits out its
    // whole deadline waiting for an answer that is never coming.
    if (!transport_ || !transport_->isFileTransferAvailable()) {
        refuse(serve.peerId, serve.sessionId, QStringLiteral("file transfer unavailable"));
        dropServe(serve.peerId, false, QStringLiteral("file transfer unavailable"));
        return;
    }
    // serve.file is already set when the peer asked to continue a generation of
    // its choosing; otherwise it gets the live one.
    const QString file = serve.file.isEmpty() ? snapshot_.path() : serve.file;
    if (file.isEmpty()) {
        refuse(serve.peerId, serve.sessionId, QStringLiteral("no snapshot to serve"), 300);
        dropServe(serve.peerId, false, QStringLiteral("no snapshot to serve"));
        return;
    }
    serve.waitingForSnapshot = false;
    // Remember *which* generation this peer is reading. A later generation can be
    // published while this transfer is still running, and the prune must not pull
    // the file out from under it.
    serve.file = file;

    // The answer goes out here, next to the offer, rather than at the point the
    // request arrived: only now is it known which generation the peer is getting,
    // and that name is what lets it continue this download later. A peer that was
    // told to wait gets this as a second answer, which reads as "ready now".
    const QFileInfo info(file);
    // The live generation's row count is in the metadata; a superseded one that a
    // peer is continuing has only its own header to say.
    const DatabaseSnapshot::Info live = snapshot_.info();
    const qint64 torrents = (live.valid && file == snapshot_.path()) ? live.torrents : torrentsInDump(file);
    transport_->sendMessage(serve.peerId, QStringLiteral("databaseRequest_response"),
        QJsonObject { { "session", static_cast<double>(serve.sessionId) }, { "accepted", true }, { "ready", true },
            { "torrents", static_cast<double>(torrents) }, { "bytes", static_cast<double>(info.size()) },
            { "snapshot", info.fileName() }, { "format", static_cast<int>(dump::kFormatVersion) } });

    serve.transferId = transport_->sendFile(serve.peerId, file);
    if (serve.transferId == 0) {
        dropServe(serve.peerId, false, QStringLiteral("could not offer the snapshot"));
        return;
    }
    qInfo() << "[DatabaseSync] offering" << QFileInfo(file).size() << "bytes to" << serve.peerId.left(8);
    publishServe();
}

bool DatabaseSyncService::ensureSnapshotJob()
{
    if (snapshotJob_)
        return true;
    if (!repository_)
        return false;

    const qint64 torrents = repository_->statistics().torrents;
    const qint64 needed = static_cast<qint64>(torrents * snapshot_.bytesPerTorrent() * kFreeSpaceMargin);
    const qint64 free = availableBytes(transferDirectory());
    if (free >= 0 && free < needed) {
        qWarning() << "[DatabaseSync] not generating a snapshot:" << (needed / (1024 * 1024)) << "MB needed,"
                   << (free / (1024 * 1024)) << "MB free";
        return false;
    }

    auto job = std::make_shared<SnapshotJob>();
    job->id = ++nextSnapshotJobId_;
    job->total = torrents;
    snapshotJob_ = job;
    job->worker = QtConcurrent::run([this, job]() { runSnapshot(job); });
    publishServe();
    return true;
}

void DatabaseSyncService::pruneSnapshots()
{
    QSet<QString> inUse;
    for (const Serve& serve : serves_) {
        if (!serve.file.isEmpty())
            inUse.insert(serve.file);
    }
    snapshot_.pruneSuperseded(inUse);
}

void DatabaseSyncService::cancelSnapshotIfUnwanted()
{
    if (!snapshotJob_)
        return;
    for (const Serve& serve : serves_) {
        if (serve.waitingForSnapshot)
            return;
    }
    // Nobody is waiting any more. Finishing the export would spend minutes of disk
    // and CPU on a file no peer is going to ask for — which is exactly the work the
    // old design kept doing after its requester had already given up.
    qInfo() << "[DatabaseSync] no peers left waiting; stopping snapshot generation";
    snapshotJob_->cancel.cancel();
}

void DatabaseSyncService::runSnapshot(const SnapshotPtr& job)
{
    dump::Header header;
    header.client = clientVersion_;
    header.peerId = transport_ ? transport_->ourPeerId() : QString();
    header.created = QDateTime::currentDateTime();
    header.torrents = repository_->statistics().torrents;

    qint64 lastPublish = 0;

    DatabaseExporter exporter(repository_);
    const DatabaseExporter::Result result = exporter.run(
        snapshot_.temporaryPath(), header, job->cancel, [this, job, &lastPublish](qint64 torrents, qint64 bytes) {
            Q_UNUSED(bytes);
            {
                QMutexLocker lock(&mutex_);
                job->processed = torrents;
            }
            const qint64 now = nowMs();
            if (now - lastPublish < kProgressIntervalMs)
                return;
            lastPublish = now;
            QMetaObject::invokeMethod(this, [this]() { publishServe(); }, Qt::QueuedConnection);
        });

    QMetaObject::invokeMethod(
        this, [this, job, result]() { onSnapshotFinished(job, result.ok, result.torrents, result.error); },
        Qt::QueuedConnection);
}

void DatabaseSyncService::onSnapshotFinished(const SnapshotPtr& job, bool ok, qint64 torrents, const QString& error)
{
    // A job that is no longer the current one has been superseded or shut down. It
    // must not publish: both jobs write the same temporary path, so letting a stale
    // one commit would hand peers a half-written file.
    if (snapshotJob_ != job) {
        QFile::remove(snapshot_.temporaryPath());
        return;
    }
    snapshotJob_.reset();

    QString commitError;
    if (ok)
        ok = snapshot_.commit(torrents, &commitError);
    else
        QFile::remove(snapshot_.temporaryPath());

    if (!ok) {
        const QString reason = error.isEmpty() ? commitError : error;
        qWarning() << "[DatabaseSync] snapshot generation failed:" << reason;
        const QStringList waiting = serves_.keys();
        for (const QString& peerId : waiting) {
            // find(), not operator[]: the latter inserts a default Serve for a key
            // an earlier iteration removed, conjuring a peer we are not serving.
            const auto it = serves_.constFind(peerId);
            if (it != serves_.constEnd() && it->waitingForSnapshot)
                dropServe(peerId, false, reason.isEmpty() ? QStringLiteral("snapshot failed") : reason);
        }
        publishServe();
        return;
    }

    pruneSnapshots();

    // One export, every waiter served from it — the point of the whole snapshot.
    const QStringList waiting = serves_.keys();
    for (const QString& peerId : waiting) {
        auto it = serves_.find(peerId);
        if (it != serves_.end() && it->waitingForSnapshot)
            offerSnapshot(*it);
    }
    publishServe();
}

void DatabaseSyncService::dropServe(QString peerId, bool success, const QString& reason, bool releaseSnapshot)
{
    auto it = serves_.find(peerId);
    if (it == serves_.end())
        return;

    const Serve serve = *it;
    serves_.erase(it);

    if (transport_ && serve.transferId != 0 && !success)
        transport_->cancelFile(peerId, serve.transferId);

    // The live snapshot is never removed here: it is shared, and outliving any one
    // peer's transfer is the whole point of it. A *superseded* generation this
    // peer was the last reader of can go now.
    if (releaseSnapshot)
        cancelSnapshotIfUnwanted();
    pruneSnapshots();

    QJsonObject summary { { "peer", peerId }, { "reason", reason } };
    emit serveFinished(peerId, success, summary);
    publishServe();
}

bool DatabaseSyncService::rebuildSnapshot(QString* error)
{
    if (snapshotJob_) {
        if (error)
            *error = tr("A database snapshot is already being prepared.");
        return false;
    }
    // Deliberately does *not* discard the current snapshot first. Peers keep being
    // served from it while the replacement builds, and the commit supersedes it
    // atomically — discarding up front would strand anyone mid-download and, on
    // Windows, fail to delete the open file anyway.
    if (!ensureSnapshotJob()) {
        if (error)
            *error = tr("Could not start preparing a database snapshot.");
        return false;
    }
    return true;
}

// ===========================================================================
// Transfers
// ===========================================================================

void DatabaseSyncService::onFileOffered(const QString& peerId, quint64 transferId, const QString& name, qint64 size)
{
    const LocalPtr op = local_;
    // An unsolicited multi-gigabyte file is exactly what this check exists for:
    // accept only from the peer we asked, and only while we are waiting for it.
    if (!op || op->operation != Operation::PeerPull || op->peerId != peerId || op->transferId != 0) {
        if (transport_)
            transport_->rejectFile(peerId, transferId);
        qInfo() << "[DatabaseSync] rejected unsolicited file offer" << name << "from" << peerId.left(8);
        return;
    }

    const QString destination = incomingPathFor(peerId);
    const QString partial = partialPathFor(peerId);
    const QString note = partialStatePathFor(peerId);

    // What is on disk, and whether it is a prefix of *this* file. The check is on
    // the note rather than on the bytes: one dump is only told from another by
    // what the peer said it was serving, and the alternative — finding out from
    // the SHA-256 — costs the entire download first.
    const qint64 have = QFileInfo(partial).size();
    const qint64 resumeFrom = DatabaseTransferResume::load(note).offsetFor(have, peerId, op->snapshotId, size);
    if (resumeFrom == 0 && have > 0) {
        // librats resumes from whatever length the partial has, so a partial we
        // have decided not to trust has to be gone before the transfer starts.
        discardPartial(peerId);
    }

    const qint64 needed = size - resumeFrom;
    const qint64 free = availableBytes(transferDirectory());
    if (free >= 0 && free < needed) {
        transport_->rejectFile(peerId, transferId);
        finishLocal(op, false,
            tr("Not enough free space for the database: %1 MB needed, %2 MB available.")
                .arg(needed / (1024 * 1024))
                .arg(free / (1024 * 1024)));
        return;
    }

    op->transferId = transferId;
    op->deadlineMs = 0; // the transfer has its own idle timeout in librats
    {
        QMutexLocker lock(&mutex_);
        status_.path = destination;
        status_.totalBytes = size;
        status_.bytes = resumeFrom;
        status_.resumedFrom = resumeFrom;
    }

    // Write the note before a byte lands, so an interruption at any point — a
    // dropped peer, a killed process — leaves something the next attempt can
    // identify. Without a generation name there is nothing to identify it by, and
    // the partial is then only worth the transfer it is part of.
    if (op->snapshotId.isEmpty())
        DatabaseTransferResume::clear(note);
    else
        DatabaseTransferResume { peerId, op->snapshotId, size }.save(note);

    // Always into the partial, resuming or not: that is what makes *the next*
    // attempt a resume rather than a fresh download.
    if (!transport_->acceptFileResume(peerId, transferId, destination, partial)) {
        op->transferId = 0;
        finishLocal(op, false, tr("Could not accept the database transfer."));
        return;
    }
    if (resumeFrom > 0) {
        qInfo() << "[DatabaseSync] continuing" << op->snapshotId << "from" << resumeFrom << "of" << size << "bytes";
        emit statusMessage(tr("Continuing the database download at %1 MB of %2 MB…")
                               .arg(resumeFrom / (1024 * 1024))
                               .arg(size / (1024 * 1024)),
            8000);
    }
    setStage(op, QStringLiteral("transferring"));
}

void DatabaseSyncService::onTransferProgress(
    const QString& peerId, quint64 transferId, bool sending, qint64 transferred, qint64 total, double bytesPerSec)
{
    Q_UNUSED(bytesPerSec);

    if (sending) {
        auto it = serves_.find(peerId);
        if (it == serves_.end() || it->transferId != transferId)
            return;
        const qint64 now = nowMs();
        if (now - lastProgressPublishMs_ >= kProgressIntervalMs) {
            lastProgressPublishMs_ = now;
            publishServe();
        }
        return;
    }

    const LocalPtr op = local_;
    if (!op || op->peerId != peerId || op->transferId != transferId)
        return;
    {
        QMutexLocker lock(&mutex_);
        status_.bytes = transferred;
        status_.totalBytes = total;
    }
    const qint64 now = nowMs();
    if (now - lastProgressPublishMs_ >= kProgressIntervalMs) {
        lastProgressPublishMs_ = now;
        publishLocal();
    }
}

void DatabaseSyncService::onTransferFinished(
    const QString& peerId, quint64 transferId, bool success, const QString& path)
{
    // Matching on the (peer, id) pair, never on the id alone: transfer ids are
    // allocated by the sender, so a stranger's transfer 1 and ours collide
    // routinely — and matching loosely used to abort a live download because some
    // unrelated peer's offer had been rejected.
    auto serve = serves_.find(peerId);
    if (serve != serves_.end() && serve->transferId == transferId) {
        if (success)
            qInfo() << "[DatabaseSync] sent the database to" << peerId.left(8);
        // Background lane: logged and reported, never raised to the user.
        dropServe(peerId, success, success ? QString() : QStringLiteral("transfer failed"));
        return;
    }

    const LocalPtr op = local_;
    if (!op || op->peerId != peerId || op->transferId != transferId)
        return;

    op->transferId = 0;
    if (!success) {
        // The bytes stay on disk with their note beside them: another
        // database.pull from the same peer continues from here. Which is the point
        // — an 18M-torrent dump does not always arrive in one piece, and starting
        // over on every dropped connection is how it never arrives at all.
        const qint64 have = QFileInfo(partialPathFor(peerId)).size();
        if (have > 0 && !op->snapshotId.isEmpty()) {
            finishLocal(op, false,
                tr("The database transfer failed at %1 MB — pull from this peer again to continue.")
                    .arg(have / (1024 * 1024)));
        } else {
            finishLocal(op, false, tr("The database transfer failed."));
        }
        return;
    }

    // Complete: the partial became the dump itself, so its note describes nothing.
    DatabaseTransferResume::clear(partialStatePathFor(peerId));

    // Straight into the merge, keeping the same operation: a received dump that is
    // never imported is just a temp file nobody asked for.
    startPullImport(op, path.isEmpty() ? status().path : path);
}

void DatabaseSyncService::onPeerDisconnected(const QString& peerId)
{
    if (serves_.contains(peerId))
        dropServe(peerId, false, QStringLiteral("the peer disconnected"));

    const LocalPtr op = local_;
    if (!op || op->operation != Operation::PeerPull || op->peerId != peerId)
        return;
    // Only the *pull* dies with the peer. Once the dump is on disk the import is
    // ours and finishes regardless of who we got it from.
    finishLocal(op, false, tr("The peer disconnected."));
}

// ===========================================================================
// Watchdog
// ===========================================================================

void DatabaseSyncService::onWatchdog()
{
    const qint64 now = nowMs();

    // Heartbeat to everyone waiting on a snapshot we are building. This is what
    // lets their deadline measure silence instead of the size of our index.
    if (snapshotJob_ && now - lastHeartbeatMs_ >= kHeartbeatMs) {
        lastHeartbeatMs_ = now;
        qint64 processed = 0;
        qint64 total = 0;
        {
            QMutexLocker lock(&mutex_);
            processed = snapshotJob_->processed;
            total = snapshotJob_->total;
        }
        if (transport_) {
            for (const Serve& serve : serves_) {
                if (!serve.waitingForSnapshot)
                    continue;
                transport_->sendMessage(serve.peerId, QStringLiteral("databaseProgress"),
                    QJsonObject { { "session", static_cast<double>(serve.sessionId) },
                        { "processed", static_cast<double>(processed) }, { "total", static_cast<double>(total) } });
            }
        }
    }

    // The rate-limit table is keyed by peer id, so without pruning it grows for
    // the life of the process on a node that many peers ask.
    for (auto it = lastRequestMs_.begin(); it != lastRequestMs_.end();) {
        if (now - it.value() > kMinRequestIntervalMs * 20)
            it = lastRequestMs_.erase(it);
        else
            ++it;
    }

    const LocalPtr op = local_;
    if (!op || op->deadlineMs == 0 || now < op->deadlineMs)
        return;

    const QString stage = status().stage;
    if (stage == QLatin1String("waiting"))
        finishLocal(op, false, tr("The peer did not answer."));
    else if (stage == QLatin1String("awaitingOffer"))
        finishLocal(op, false, tr("The peer said its database was ready but never sent it."));
    else
        finishLocal(op, false, tr("The peer stopped responding while preparing its database."));
}

// ===========================================================================
// Paths, housekeeping, resume state
// ===========================================================================

QString DatabaseSyncService::transferDirectory() const
{
    return QDir(dataDirectory_).absoluteFilePath(QStringLiteral("dbsync"));
}

QString DatabaseSyncService::incomingPathFor(const QString& peerId) const
{
    return QDir(transferDirectory()).absoluteFilePath(QStringLiteral("incoming-%1.ratsdb").arg(peerId.left(16)));
}

QString DatabaseSyncService::partialPathFor(const QString& peerId) const
{
    // Beside the destination and named after it: incoming-<peer>.ratsdb.part is
    // what a broken transfer leaves, and .part.json is the note saying what it is.
    return incomingPathFor(peerId) + QStringLiteral(".part");
}

QString DatabaseSyncService::partialStatePathFor(const QString& peerId) const
{
    return partialPathFor(peerId) + QStringLiteral(".json");
}

void DatabaseSyncService::discardPartial(const QString& peerId) const
{
    QFile::remove(partialPathFor(peerId));
    DatabaseTransferResume::clear(partialStatePathFor(peerId));
}

QString DatabaseSyncService::resumeStatePath() const
{
    return QDir(transferDirectory()).absoluteFilePath(QStringLiteral("import-state.json"));
}

qint64 DatabaseSyncService::availableBytes(const QString& path)
{
    const QStorageInfo storage(path);
    if (!storage.isValid() || !storage.isReady())
        return -1;
    return storage.bytesAvailable();
}

void DatabaseSyncService::sweepTransferDirectory() const
{
    // Nothing in here is precious except the snapshot and an import that can still
    // be resumed. Everything else is the wreckage of a crash or a cancelled
    // transfer, and it used to accumulate for the life of the installation.
    // A half-received dump (incoming-*.ratsdb.part) is a resume point rather than
    // wreckage, and deliberately gets no exemption here: it ages out with
    // everything else, because an abandoned one is gigabytes of a database nobody
    // is going to continue. A day is long enough to come back to a broken pull.
    const QJsonObject pending = pendingImportJson();
    const QString keep = pending.isEmpty() ? QString() : pending["path"].toString();

    const QDir dir(transferDirectory());
    const QDateTime cutoff = QDateTime::currentDateTime().addSecs(-kStaleFileAgeSecs);

    int removed = 0;
    const QFileInfoList entries = dir.entryInfoList(QDir::Files);
    for (const QFileInfo& entry : entries) {
        const QString name = entry.fileName();
        if (DatabaseSnapshot::isSnapshotFile(name) || name == QLatin1String("import-state.json"))
            continue;
        if (!keep.isEmpty() && entry.absoluteFilePath() == keep)
            continue;
        // share-*.ratsdb is from the per-request export this service no longer
        // does; those are dead on arrival whatever their age.
        const bool legacy = name.startsWith(QLatin1String("share-"));
        if (!legacy && entry.lastModified() > cutoff)
            continue;
        if (QFile::remove(entry.absoluteFilePath()))
            ++removed;
    }
    if (removed > 0)
        qInfo() << "[DatabaseSync] swept" << removed << "stale files from" << dir.absolutePath();
}

void DatabaseSyncService::saveResumeState(const QString& path, qint64 offset, qint64 fileSize) const
{
    QFile file(resumeStatePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    const QJsonObject obj { { "path", path }, { "offset", static_cast<double>(offset) },
        { "size", static_cast<double>(fileSize) } };
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

qint64 DatabaseSyncService::loadResumeOffset(const QString& path, qint64 fileSize) const
{
    QFile file(resumeStatePath());
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    // A different file, or the same name with different contents, is not a resume
    // point — restart rather than seeking into the middle of a frame.
    if (obj["path"].toString() != path || obj["size"].toVariant().toLongLong() != fileSize)
        return 0;
    return obj["offset"].toVariant().toLongLong();
}

void DatabaseSyncService::clearResumeState() const
{
    QFile::remove(resumeStatePath());
}

} // namespace rats::service
