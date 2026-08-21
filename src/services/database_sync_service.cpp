#include "services/database_sync_service.h"

#include "data/torrent_repository.h"
#include "net/p2p_transport.h"
#include "services/database_dump.h"
#include "services/indexing_service.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMetaObject>
#include <QMutexLocker>
#include <QStringList>
#include <QTimer>
#include <QtConcurrent>

#include <utility>

namespace rats::service {

namespace {

// Torrents per database page while exporting. Also the IN() batch size for the
// importer, and both must stay under Manticore's max_matches (1000).
constexpr int kPageSize = 500;
// How long we wait for a peer to answer databaseRequest at all.
constexpr int kResponseTimeoutMs = 60 * 1000;
// How long we then wait for the file offer: the peer has to export its whole
// index first, which on a large one takes minutes.
constexpr int kOfferTimeoutMs = 30 * 60 * 1000;

QString operationName(DatabaseSyncService::Operation operation)
{
    switch (operation) {
    case DatabaseSyncService::Operation::Export:
        return QStringLiteral("export");
    case DatabaseSyncService::Operation::Import:
        return QStringLiteral("import");
    case DatabaseSyncService::Operation::PeerPull:
        return QStringLiteral("peerPull");
    case DatabaseSyncService::Operation::PeerServe:
        return QStringLiteral("peerServe");
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
    if (!s.format.isEmpty())
        obj["format"] = s.format;
    obj["peer"] = s.peerId;
    obj["processed"] = static_cast<double>(s.processed);
    obj["total"] = static_cast<double>(s.total);
    obj["inserted"] = static_cast<double>(s.inserted);
    obj["merged"] = static_cast<double>(s.merged);
    obj["rejected"] = static_cast<double>(s.rejected);
    obj["bytes"] = static_cast<double>(s.bytes);
    obj["totalBytes"] = static_cast<double>(s.totalBytes);
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
{
    if (transport_) {
        connect(transport_, &net::P2PTransport::fileOffered, this, &DatabaseSyncService::onFileOffered);
        connect(transport_, &net::P2PTransport::fileTransferProgress, this, &DatabaseSyncService::onTransferProgress);
        connect(transport_, &net::P2PTransport::fileTransferFinished, this, &DatabaseSyncService::onTransferFinished);
        connect(transport_, &net::P2PTransport::peerDisconnected, this, &DatabaseSyncService::onPeerDisconnected);
    }
}

DatabaseSyncService::~DatabaseSyncService()
{
    shutdown();
}

// ===========================================================================
// Operation slot
// ===========================================================================

bool DatabaseSyncService::beginOperation(
    Operation operation, const QString& path, const QString& peerId, QString* error)
{
    bool expected = false;
    if (!busy_.compare_exchange_strong(expected, true)) {
        if (error)
            *error = tr("A database sync is already running.");
        return false;
    }

    cancelRequested_ = false;
    {
        QMutexLocker lock(&mutex_);
        status_ = Status {};
        status_.operation = operation;
        status_.running = true;
        status_.path = path;
        status_.peerId = peerId;
    }
    emit syncStarted(statusJson());
    return true;
}

void DatabaseSyncService::finishOperation(bool success, const QString& error)
{
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

    pendingPeer_.clear();
    incomingTransferId_ = 0;
    outgoingTransferId_ = 0;
    outgoingPath_.clear();
    busy_ = false;

    emit syncFinished(success, summary);
}

void DatabaseSyncService::publishProgress()
{
    emit syncProgress(statusJson());
}

void DatabaseSyncService::updateStage(const QString& stage)
{
    {
        QMutexLocker lock(&mutex_);
        status_.stage = stage;
    }
    publishProgress();
}

bool DatabaseSyncService::isBusy() const
{
    return busy_.load();
}

DatabaseSyncService::Status DatabaseSyncService::status() const
{
    QMutexLocker lock(&mutex_);
    return status_;
}

QJsonObject DatabaseSyncService::statusJson() const
{
    QMutexLocker lock(&mutex_);
    return statusToJson(status_);
}

void DatabaseSyncService::cancel()
{
    if (!busy_.load())
        return;
    cancelRequested_ = true;

    // A transfer has no worker loop to notice the flag; tear it down here.
    if (transport_) {
        if (incomingTransferId_ != 0 && !pendingPeer_.isEmpty())
            transport_->cancelFile(pendingPeer_, incomingTransferId_);
        if (outgoingTransferId_ != 0)
            transport_->cancelFile(status().peerId, outgoingTransferId_);
    }
    updateStage(QStringLiteral("cancelling"));

    // Nothing is running in the background while we only wait on a peer, so the
    // slot has to be released here rather than by a worker.
    if (!worker_.isRunning() && incomingTransferId_ == 0 && outgoingTransferId_ == 0)
        finishOperation(false, tr("Cancelled."));
}

void DatabaseSyncService::shutdown()
{
    cancelRequested_ = true;
    if (worker_.isRunning())
        worker_.waitForFinished();
}

void DatabaseSyncService::setSharingEnabled(bool enabled)
{
    sharingEnabled_ = enabled;
}

// ===========================================================================
// Export
// ===========================================================================

bool DatabaseSyncService::exportToFile(const QString& path, const ExportSettings& settings, QString* error)
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
    if (!beginOperation(Operation::Export, path, QString(), error))
        return false;

    const ExportFormat format = settings.format.value_or(exportFormatForPath(path));
    ExportOptions options;
    options.includeFiles = settings.includeFiles;
    {
        // Set after beginOperation, which resets the status: this is what tells
        // an API caller which format a sniffed extension resolved to.
        QMutexLocker lock(&mutex_);
        status_.format = exportFormatName(format);
    }

    worker_ = QtConcurrent::run([this, path, format, options]() { runExport(path, QString(), format, options); });
    return true;
}

void DatabaseSyncService::runExport(
    const QString& path, const QString& forPeer, ExportFormat format, const ExportOptions& options)
{
    updateStage(QStringLiteral("exporting"));

    dump::Header header;
    header.client = clientVersion_;
    header.peerId = transport_ ? transport_->ourPeerId() : QString();
    header.created = QDateTime::currentDateTime();
    header.torrents = repository_->statistics().torrents;

    {
        QMutexLocker lock(&mutex_);
        status_.total = header.torrents;
    }

    const std::unique_ptr<TorrentSink> sink = makeExportSink(format, options);
    QString error;
    if (!sink->open(path, header, &error)) {
        QMetaObject::invokeMethod(this, [this, error]() { finishOperation(false, error); }, Qt::QueuedConnection);
        return;
    }

    // Keyset pagination, for the reason spelled out on pageAfterId(): OFFSET
    // cannot walk past max_matches.
    qint64 afterId = 0;
    qint64 written = 0;
    bool ok = true;

    while (!cancelRequested_.load()) {
        const QVector<domain::Torrent> page = repository_->pageAfterId(afterId, kPageSize);
        if (page.isEmpty())
            break;
        afterId = page.last().id;

        // A flat sink records only the file *count*, which already lives on the
        // torrent row — so skip the file query, the expensive half of the sweep.
        QHash<QString, QVector<domain::File>> files;
        if (sink->needsFiles()) {
            QStringList hashes;
            hashes.reserve(page.size());
            for (const domain::Torrent& t : page)
                hashes << t.hash;
            files = repository_->filesOf(hashes);
        }

        for (const domain::Torrent& t : page) {
            domain::Torrent copy = t;
            copy.fileList = files.value(t.hash);
            if (!sink->write(copy, &error)) {
                ok = false;
                break;
            }
            ++written;
        }
        if (!ok)
            break;

        {
            QMutexLocker lock(&mutex_);
            status_.processed = written;
            status_.bytes = sink->bytesWritten();
        }
        QMetaObject::invokeMethod(this, [this]() { publishProgress(); }, Qt::QueuedConnection);
    }

    if (cancelRequested_.load()) {
        sink->abort();
        QMetaObject::invokeMethod(this, [this]() { finishOperation(false, tr("Cancelled.")); }, Qt::QueuedConnection);
        return;
    }
    if (!ok || !sink->finish(&error)) {
        sink->abort();
        const QString reason = error.isEmpty() ? tr("Failed to write the dump.") : error;
        QMetaObject::invokeMethod(this, [this, reason]() { finishOperation(false, reason); }, Qt::QueuedConnection);
        return;
    }

    {
        QMutexLocker lock(&mutex_);
        status_.processed = written;
        status_.bytes = QFileInfo(path).size();
        status_.totalBytes = status_.bytes;
    }

    if (forPeer.isEmpty()) {
        QMetaObject::invokeMethod(this, [this]() { finishOperation(true); }, Qt::QueuedConnection);
        return;
    }

    // Serving a peer: the dump is only the first half of the job.
    QMetaObject::invokeMethod(
        this,
        [this, path, forPeer]() {
            if (!transport_ || !transport_->isFileTransferAvailable()) {
                QFile::remove(path);
                finishOperation(false, tr("File transfer is not available."));
                return;
            }
            outgoingPath_ = path;
            outgoingTransferId_ = transport_->sendFile(forPeer, path);
            if (outgoingTransferId_ == 0) {
                QFile::remove(path);
                finishOperation(false, tr("Could not offer the database to the peer."));
                return;
            }
            updateStage(QStringLiteral("transferring"));
            qInfo() << "[DatabaseSync] offering" << QFileInfo(path).size() << "bytes to" << forPeer.left(8);
        },
        Qt::QueuedConnection);
}

// ===========================================================================
// Import
// ===========================================================================

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
    if (!beginOperation(Operation::Import, path, QString(), error))
        return false;

    worker_ = QtConcurrent::run([this, path, options]() { runImport(path, options); });
    return true;
}

void DatabaseSyncService::runImport(const QString& path, const ImportOptions& options)
{
    updateStage(QStringLiteral("importing"));

    DumpReader reader;
    QString error;
    if (!reader.open(path, &error)) {
        QMetaObject::invokeMethod(this, [this, error]() { finishOperation(false, error); }, Qt::QueuedConnection);
        return;
    }

    const qint64 fileSize = reader.fileSize();
    qint64 processed = 0;
    qint64 inserted = 0;
    qint64 merged = 0;
    qint64 rejected = 0;

    if (options.resume) {
        const qint64 offset = loadResumeOffset(path, fileSize);
        if (offset > 0 && reader.seekTo(offset)) {
            qInfo() << "[DatabaseSync] resuming import of" << path << "at byte" << offset;
        }
    }

    {
        QMutexLocker lock(&mutex_);
        status_.total = reader.header().torrents;
        status_.totalBytes = fileSize;
    }

    IndexingService::BatchOptions batchOptions;
    batchOptions.applyFilters = options.applyFilters;

    QVector<domain::Torrent> batch;
    bool failed = false;

    while (!cancelRequested_.load() && reader.readBatch(batch, &error)) {
        const IndexingService::BatchResult result = indexing_->insertBatch(batch, batchOptions);
        processed += batch.size();
        inserted += result.inserted;
        merged += result.merged;
        rejected += result.rejected;

        // The offset is only written after the batch it follows is committed, so
        // a resume can duplicate work but never skip it.
        saveResumeState(path, reader.offset(), fileSize);

        {
            QMutexLocker lock(&mutex_);
            status_.processed = processed;
            status_.inserted = inserted;
            status_.merged = merged;
            status_.rejected = rejected;
            status_.bytes = reader.offset();
        }
        QMetaObject::invokeMethod(this, [this]() { publishProgress(); }, Qt::QueuedConnection);
    }

    if (!reader.atEnd() && !error.isEmpty())
        failed = true;

    const bool cancelled = cancelRequested_.load();
    const bool truncated = reader.atEnd() && !reader.complete();
    const bool removeWhenDone = options.removeWhenDone;

    if (!cancelled)
        clearResumeState();

    QMetaObject::invokeMethod(
        this,
        [this, cancelled, failed, truncated, error, removeWhenDone, path, inserted, merged]() {
            if (removeWhenDone && !cancelled)
                QFile::remove(path);

            if (cancelled) {
                finishOperation(false, tr("Cancelled — the import can be resumed."));
                return;
            }
            if (failed) {
                finishOperation(false, error.isEmpty() ? tr("The dump could not be read.") : error);
                return;
            }
            qInfo() << "[DatabaseSync] import finished:" << inserted << "new," << merged << "already known";
            if (truncated) {
                // Everything read was still merged; say so rather than claiming a
                // clean import.
                emit statusMessage(tr("Database import finished, but the dump was truncated."), 8000);
            }
            finishOperation(true);
        },
        Qt::QueuedConnection);
}

// ===========================================================================
// Peer transfer
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
    if (!beginOperation(Operation::PeerPull, QString(), peerId, error))
        return false;

    pendingPeer_ = peerId;
    pendingImportOptions_ = options;
    if (!transport_->sendMessage(peerId, QStringLiteral("databaseRequest"), QJsonObject {})) {
        finishOperation(false, tr("Could not reach the peer."));
        return false;
    }

    updateStage(QStringLiteral("waiting"));
    emit statusMessage(tr("Asked %1 for its database…").arg(peerId.left(8)), 5000);

    // Without a deadline a silent peer would hold the operation slot forever.
    QTimer::singleShot(kResponseTimeoutMs, this, [this, peerId]() {
        if (busy_.load() && pendingPeer_ == peerId && status().stage == QStringLiteral("waiting"))
            finishOperation(false, tr("The peer did not answer."));
    });
    return true;
}

void DatabaseSyncService::handlePeerRequest(const QString& peerId, const QJsonObject& /*data*/)
{
    auto refuse = [this, &peerId](const QString& reason) {
        if (transport_) {
            transport_->sendMessage(peerId, QStringLiteral("databaseRequest_response"),
                QJsonObject { { "accepted", false }, { "reason", reason } });
        }
    };

    if (!sharingEnabled_) {
        refuse(QStringLiteral("sharing disabled"));
        return;
    }
    if (!transport_ || !transport_->isFileTransferAvailable()) {
        refuse(QStringLiteral("file transfer unavailable"));
        return;
    }
    if (isBusy()) {
        refuse(QStringLiteral("busy"));
        return;
    }

    const qint64 torrents = repository_ ? repository_->statistics().torrents : 0;
    if (torrents <= 0) {
        refuse(QStringLiteral("empty database"));
        return;
    }

    const QString path = sharePathFor(peerId);
    QDir().mkpath(transferDirectory());
    QString error;
    if (!beginOperation(Operation::PeerServe, path, peerId, &error)) {
        refuse(QStringLiteral("busy"));
        return;
    }

    transport_->sendMessage(peerId, QStringLiteral("databaseRequest_response"),
        QJsonObject { { "accepted", true }, { "torrents", static_cast<double>(torrents) },
            { "format", static_cast<int>(dump::kFormatVersion) } });

    qInfo() << "[DatabaseSync] serving" << torrents << "torrents to" << peerId.left(8);
    emit statusMessage(tr("Sharing the database with %1…").arg(peerId.left(8)), 5000);
    // Always a dump, whatever the user last exported by hand: the far end feeds
    // this straight into DumpReader.
    worker_
        = QtConcurrent::run([this, path, peerId]() { runExport(path, peerId, ExportFormat::Ratsdb, ExportOptions()); });
}

void DatabaseSyncService::handlePeerResponse(const QString& peerId, const QJsonObject& data)
{
    // Only the peer we actually asked can move this state machine.
    if (pendingPeer_.isEmpty() || peerId != pendingPeer_)
        return;

    if (!data["accepted"].toBool(false)) {
        const QString reason = data["reason"].toString();
        finishOperation(false,
            reason.isEmpty() ? tr("The peer refused to share its database.")
                             : tr("The peer refused to share its database: %1").arg(reason));
        return;
    }

    {
        QMutexLocker lock(&mutex_);
        status_.total = data["torrents"].toVariant().toLongLong();
    }
    updateStage(QStringLiteral("preparing"));
    emit statusMessage(tr("%1 is preparing its database…").arg(peerId.left(8)), 5000);

    // The peer now exports its whole index, which is slow; give it a long but
    // finite window to produce the offer.
    QTimer::singleShot(kOfferTimeoutMs, this, [this, peerId]() {
        if (busy_.load() && pendingPeer_ == peerId && incomingTransferId_ == 0)
            finishOperation(false, tr("The peer never sent its database."));
    });
}

void DatabaseSyncService::onFileOffered(const QString& peerId, quint64 transferId, const QString& name, qint64 size)
{
    // An unsolicited multi-gigabyte file is exactly what this check exists for:
    // accept only from the peer we asked, and only while we are waiting for it.
    const Status current = status();
    if (current.operation != Operation::PeerPull || peerId != pendingPeer_ || incomingTransferId_ != 0) {
        if (transport_)
            transport_->rejectFile(peerId, transferId);
        qInfo() << "[DatabaseSync] rejected unsolicited file offer" << name << "from" << peerId.left(8);
        return;
    }

    QDir().mkpath(transferDirectory());
    const QString destination = incomingPathFor(peerId);
    incomingTransferId_ = transferId;

    {
        QMutexLocker lock(&mutex_);
        status_.path = destination;
        status_.totalBytes = size;
    }

    if (!transport_->acceptFile(peerId, transferId, destination)) {
        incomingTransferId_ = 0;
        finishOperation(false, tr("Could not accept the database transfer."));
        return;
    }
    updateStage(QStringLiteral("transferring"));
}

void DatabaseSyncService::onTransferProgress(
    const QString& peerId, quint64 transferId, bool sending, qint64 transferred, qint64 total, double bytesPerSec)
{
    Q_UNUSED(peerId);
    Q_UNUSED(bytesPerSec);
    if (transferId != incomingTransferId_ && transferId != outgoingTransferId_)
        return;
    if (sending && transferId != outgoingTransferId_)
        return;

    {
        QMutexLocker lock(&mutex_);
        status_.bytes = transferred;
        status_.totalBytes = total;
    }
    publishProgress();
}

void DatabaseSyncService::onTransferFinished(quint64 transferId, bool success, const QString& path)
{
    if (transferId != 0 && transferId == outgoingTransferId_) {
        // We were the sender: the temp dump has done its job either way.
        if (!outgoingPath_.isEmpty())
            QFile::remove(outgoingPath_);
        outgoingPath_.clear();
        outgoingTransferId_ = 0;
        finishOperation(success, success ? QString() : tr("The transfer failed."));
        return;
    }

    if (transferId == 0 || transferId != incomingTransferId_)
        return;

    incomingTransferId_ = 0;
    pendingPeer_.clear();

    if (!success) {
        finishOperation(false, tr("The database transfer failed."));
        return;
    }

    // Straight into the merge, keeping the same operation slot: a received dump
    // that is never imported is just a temp file nobody asked for.
    const QString dumpPath = path.isEmpty() ? status().path : path;
    ImportOptions options = pendingImportOptions_;
    options.removeWhenDone = true;
    options.resume = false;

    {
        QMutexLocker lock(&mutex_);
        status_.operation = Operation::Import;
        status_.path = dumpPath;
        status_.processed = 0;
    }
    worker_ = QtConcurrent::run([this, dumpPath, options]() { runImport(dumpPath, options); });
}

void DatabaseSyncService::onPeerDisconnected(const QString& peerId)
{
    if (!busy_.load())
        return;
    const Status current = status();
    if (current.operation == Operation::PeerPull && peerId == pendingPeer_) {
        finishOperation(false, tr("The peer disconnected."));
        return;
    }
    if (current.operation == Operation::PeerServe && peerId == current.peerId) {
        if (!outgoingPath_.isEmpty())
            QFile::remove(outgoingPath_);
        finishOperation(false, tr("The peer disconnected."));
    }
}

// ===========================================================================
// Paths and resume state
// ===========================================================================

QString DatabaseSyncService::transferDirectory() const
{
    return QDir(dataDirectory_).absoluteFilePath(QStringLiteral("dbsync"));
}

QString DatabaseSyncService::sharePathFor(const QString& peerId) const
{
    return QDir(transferDirectory()).absoluteFilePath(QStringLiteral("share-%1.ratsdb").arg(peerId.left(16)));
}

QString DatabaseSyncService::incomingPathFor(const QString& peerId) const
{
    return QDir(transferDirectory()).absoluteFilePath(QStringLiteral("incoming-%1.ratsdb").arg(peerId.left(16)));
}

QString DatabaseSyncService::resumeStatePath() const
{
    return QDir(transferDirectory()).absoluteFilePath(QStringLiteral("import-state.json"));
}

void DatabaseSyncService::saveResumeState(const QString& path, qint64 offset, qint64 fileSize) const
{
    QDir().mkpath(transferDirectory());
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
    // A different file, or the same name with different contents, is not a
    // resume point — restart rather than seeking into the middle of a frame.
    if (obj["path"].toString() != path || obj["size"].toVariant().toLongLong() != fileSize)
        return 0;
    return obj["offset"].toVariant().toLongLong();
}

void DatabaseSyncService::clearResumeState() const
{
    QFile::remove(resumeStatePath());
}

} // namespace rats::service
