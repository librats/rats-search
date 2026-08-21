#ifndef RATS_SERVICE_DATABASE_SYNC_SERVICE_H
#define RATS_SERVICE_DATABASE_SYNC_SERVICE_H

#include "services/export_sink.h"

#include <QFuture>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>
#include <atomic>
#include <optional>

namespace rats::data {
class TorrentRepository;
}
namespace rats::net {
class P2PTransport;
}

namespace rats::service {

class IndexingService;

// Whole-database replication: write the local index out as a portable dump,
// merge somebody else's dump into it, and move such a dump between two peers
// directly.
//
// Merging is the point — an import never replaces anything. Every torrent goes
// through IndexingService (the single write path), so a dump is treated exactly
// like a very fast peer: new torrents are indexed, known ones keep the local row
// and only gain what the incoming copy adds (file list for a metadata-only row,
// higher vote counts). Copying the Manticore data directory instead would be a
// replacement, not a merge, and its row ids come from a local counter that two
// databases both start at 1.
//
// Export and import run on a worker thread (the data layer's connections are
// per-thread) and report progress as JSON objects, so the GUI and the REST/WS
// API can render the same payload. One operation at a time.
//
// The peer flow, all of it opt-in on both ends:
//   A -> B  databaseRequest            "may I have your index?"
//   B -> A  databaseRequest_response   accepted + row count, or a refusal
//   B       exports to a temp file, then offers it over librats file transfer
//   A       accepts the offer *only* from the peer it asked, imports, deletes
// Serving is gated by the `databaseSharing` config key (off by default: a full
// index can be gigabytes). Receiving is gated by having asked: an offer from a
// peer we did not request from is rejected unopened.
class DatabaseSyncService : public QObject {
    Q_OBJECT

public:
    enum class Operation { None, Export, Import, PeerPull, PeerServe };

    struct ExportSettings {
        // Unset means "take the format from the file extension".
        std::optional<ExportFormat> format;
        // CSV only: also write the companion "<base>.files.csv".
        bool includeFiles = false;
    };

    struct ImportOptions {
        // Run the local filter policy over the incoming torrents. On by default:
        // a foreign index must not smuggle content past the user's own filters.
        bool applyFilters = true;
        // Continue an import of the same file that was interrupted earlier.
        bool resume = true;
        // Remove the dump once it has been fully imported (used for the temp file
        // a peer transfer leaves behind).
        bool removeWhenDone = false;
    };

    struct Status {
        Operation operation = Operation::None;
        bool running = false;
        QString stage; // "exporting", "waiting", "transferring", "importing"
        QString path;
        QString format; // export only: the chosen format's name
        QString peerId;
        QString error;
        qint64 processed = 0; // torrents read/written so far
        qint64 total = 0; // expected torrents, 0 when unknown
        qint64 inserted = 0;
        qint64 merged = 0;
        qint64 rejected = 0;
        qint64 bytes = 0; // transfer progress
        qint64 totalBytes = 0;
    };

    DatabaseSyncService(data::TorrentRepository* repository, IndexingService* indexing, net::P2PTransport* transport,
        QString dataDirectory, QString clientVersion, QObject* parent = nullptr);
    ~DatabaseSyncService() override;

    // Start writing the whole index to `path`. Returns false (with `error`) if
    // another operation is already running or the file cannot be created.
    //
    // `settings` carries no default argument for the same GCC reason spelled out
    // on importFromFile below; the one-argument overload supplies it.
    bool exportToFile(const QString& path, const ExportSettings& settings, QString* error = nullptr);
    bool exportToFile(const QString& path, QString* error = nullptr)
    {
        return exportToFile(path, ExportSettings(), error);
    }

    // Start merging the dump at `path` into the local index.
    //
    // `options` carries no default argument: GCC refuses to evaluate a nested
    // struct's member initialisers while the enclosing class is still incomplete,
    // so the convenience overloads below supply the defaults instead.
    bool importFromFile(const QString& path, const ImportOptions& options, QString* error = nullptr);
    bool importFromFile(const QString& path, QString* error = nullptr)
    {
        return importFromFile(path, ImportOptions(), error);
    }

    // Ask `peerId` for its whole index. The peer answers asynchronously; watch
    // syncProgress/syncFinished for the outcome. `options` applies to the merge
    // that runs once the dump has arrived.
    bool requestFromPeer(const QString& peerId, const ImportOptions& options, QString* error = nullptr);
    bool requestFromPeer(const QString& peerId, QString* error = nullptr)
    {
        return requestFromPeer(peerId, ImportOptions(), error);
    }

    // Ask the running operation to stop. Export removes its partial file; import
    // keeps what it merged and saves a resume point.
    void cancel();

    // Cancel and block until the worker has actually left the data layer. Called
    // during shutdown: the worker reads and writes the database, so it must be
    // gone before Manticore is stopped.
    void shutdown();

    bool isBusy() const;
    Status status() const;
    QJsonObject statusJson() const;

    // Whether we answer other peers' databaseRequest (config: databaseSharing).
    void setSharingEnabled(bool enabled);
    bool sharingEnabled() const { return sharingEnabled_; }

    // Peer-layer hooks. PeerApi owns the wire names and calls these; the service
    // owns the policy and the transfer.
    void handlePeerRequest(const QString& peerId, const QJsonObject& data);
    void handlePeerResponse(const QString& peerId, const QJsonObject& data);

signals:
    void syncStarted(const QJsonObject& info);
    void syncProgress(const QJsonObject& info);
    void syncFinished(bool success, const QJsonObject& summary);
    // Human-readable one-liner for a status bar.
    void statusMessage(const QString& message, int timeoutMs);

private:
    // Worker bodies (run on a QtConcurrent thread).
    void runExport(const QString& path, const QString& forPeer, ExportFormat format, const ExportOptions& options);
    void runImport(const QString& path, const ImportOptions& options);

    // Claim/release the single operation slot.
    bool beginOperation(Operation operation, const QString& path, const QString& peerId, QString* error);
    void finishOperation(bool success, const QString& error = QString());

    void publishProgress();
    void updateStage(const QString& stage);

    // Transport callbacks.
    void onFileOffered(const QString& peerId, quint64 transferId, const QString& name, qint64 size);
    void onTransferProgress(
        const QString& peerId, quint64 transferId, bool sending, qint64 transferred, qint64 total, double bytesPerSec);
    void onTransferFinished(quint64 transferId, bool success, const QString& path);
    void onPeerDisconnected(const QString& peerId);

    QString transferDirectory() const;
    QString sharePathFor(const QString& peerId) const;
    QString incomingPathFor(const QString& peerId) const;

    // Resume bookkeeping for an interrupted import.
    QString resumeStatePath() const;
    void saveResumeState(const QString& path, qint64 offset, qint64 fileSize) const;
    qint64 loadResumeOffset(const QString& path, qint64 fileSize) const;
    void clearResumeState() const;

    data::TorrentRepository* repository_;
    IndexingService* indexing_;
    net::P2PTransport* transport_;
    QString dataDirectory_;
    QString clientVersion_;

    mutable QMutex mutex_; // guards status_
    Status status_;

    std::atomic<bool> busy_ { false };
    std::atomic<bool> cancelRequested_ { false };
    bool sharingEnabled_ = false;

    QFuture<void> worker_;

    // Peer-transfer state (main thread only).
    QString pendingPeer_; // peer we asked and are waiting on
    quint64 incomingTransferId_ = 0;
    quint64 outgoingTransferId_ = 0;
    QString outgoingPath_; // temp dump being served, removed when done
    ImportOptions pendingImportOptions_;
};

} // namespace rats::service

#endif // RATS_SERVICE_DATABASE_SYNC_SERVICE_H
