#ifndef RATS_SERVICE_DATABASE_SYNC_SERVICE_H
#define RATS_SERVICE_DATABASE_SYNC_SERVICE_H

#include "services/database_importer.h"
#include "services/database_snapshot.h"
#include "services/database_transfer_resume.h"
#include "services/database_worker.h"

#include <QFuture>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>
#include <memory>

namespace rats::data {
class TorrentRepository;
}
namespace rats::net {
class P2PTransport;
}

namespace rats::service {

class IndexingService;

// Whole-database replication: write the local index out as a portable dump, merge
// somebody else's dump into it, and move such a dump between two peers.
//
// Merging is the point — an import never replaces anything. See DatabaseImporter.
//
// ---------------------------------------------------------------------------
// Two independent lanes
// ---------------------------------------------------------------------------
//
// *Local* work is what the user asked for: export to a file, import from a file,
// or pull a peer's database. One at a time, and it is the only lane the GUI
// reports on.
//
// *Serving* is what other peers asked for. It runs in the background, never
// blocks the user's own lane, and never raises a dialog: a failure there is the
// far side's problem, not something to interrupt this user about. Sharing one
// lane for both is what used to put "the database sync did not finish" on the
// screen of the person doing the *sharing*.
//
// ---------------------------------------------------------------------------
// The peer flow
// ---------------------------------------------------------------------------
//
//   A -> B  databaseRequest          {session, have?}   "may I have your index?"
//   B -> A  databaseRequest_response {session, accepted, ready, torrents, bytes, snapshot}
//   B -> A  databaseProgress         {session, processed, total}   (while ready=false)
//   B       offers the snapshot over the librats file transfer
//   A       accepts it, imports it, deletes it
//   A -> B  databaseCancel           {session}          at any point
//
// Every message carries the session id A generated, so a reply, a heartbeat or a
// timer belonging to an abandoned attempt cannot disturb the current one.
//
// ---------------------------------------------------------------------------
// Continuing an interrupted pull
// ---------------------------------------------------------------------------
//
// A multi-gigabyte dump over a household connection does not always arrive in one
// piece, and starting a 3M-row index over from zero on every dropped connection
// is how a pull becomes impossible rather than slow. So the received bytes are
// kept (dbsync/incoming-<peer>.ratsdb.part, with a DatabaseTransferResume note
// beside them) and the next attempt continues from where the last one stopped.
//
// Both ends have a say. `have` tells B which generation A already holds bytes of;
// if B still has that file it serves *that* one instead of its newest, which is
// what makes the continuation possible at all — a rebuilt snapshot is a different
// file, and half of the old one is no use against it. `snapshot` names the
// generation in the answer, and A only continues when the name, the peer and the
// total size all match the note it kept. Everything else falls back to a fresh
// download: an older peer sends no `snapshot`, an older peer's file transfer
// ignores the offset and streams from the top, and both end up merely as slow as
// they were before.
//
// `ready` distinguishes the two cases that used to be indistinguishable: B either
// has a snapshot on disk and the file is seconds away, or has to build one first
// and A should expect to wait. While building, B heartbeats — so A's deadline
// measures *silence*, not the size of B's index, which is the only way a fixed
// timeout can be correct for both a 50k and a 3M-row database.
//
// Serving is gated by the `databaseSharing` config key and advertised in the
// client_info handshake. Receiving is gated by having asked: an offer from a peer
// we did not ask is rejected unopened.
class DatabaseSyncService : public QObject {
    Q_OBJECT

public:
    enum class Operation { None, Export, Import, PeerPull };

    struct ImportOptions {
        bool applyFilters = true;
        // Continue an interrupted import of the same file.
        bool resume = true;
        // Remove the dump once it has been fully imported (the temp file a peer
        // transfer leaves behind). A cancelled import keeps it, so it can resume.
        bool removeWhenDone = false;
    };

    // The user's own operation.
    struct Status {
        Operation operation = Operation::None;
        bool running = false;
        // "exporting", "waiting" (for an answer), "preparing" (the peer is
        // building its snapshot), "awaitingOffer", "transferring", "importing",
        // "cancelling", "done", "failed"
        QString stage;
        QString path;
        QString peerId;
        QString error;
        qint64 processed = 0;
        qint64 total = 0;
        qint64 inserted = 0;
        qint64 merged = 0;
        qint64 rejected = 0;
        qint64 bytes = 0;
        qint64 totalBytes = 0;
        // Where a continued transfer picked up; 0 when it started from nothing.
        qint64 resumedFrom = 0;
    };

    // What we are doing for other peers. Background, informational.
    struct ServeStatus {
        bool generating = false;
        qint64 processed = 0;
        qint64 total = 0;
        int waiting = 0; // peers queued on the snapshot being generated
        int sending = 0; // peers currently receiving it
    };

    DatabaseSyncService(data::TorrentRepository* repository, IndexingService* indexing, net::P2PTransport* transport,
        QString dataDirectory, QString clientVersion, QObject* parent = nullptr);
    ~DatabaseSyncService() override;

    // --- the user's lane ---------------------------------------------------

    // Write the whole index to `path`.
    bool exportToFile(const QString& path, QString* error = nullptr);

    // Merge the dump at `path` into the local index.
    //
    // `options` carries no default argument: GCC refuses to evaluate a nested
    // struct's member initialisers while the enclosing class is still incomplete,
    // so the convenience overloads supply the defaults instead.
    bool importFromFile(const QString& path, const ImportOptions& options, QString* error = nullptr);
    bool importFromFile(const QString& path, QString* error = nullptr)
    {
        return importFromFile(path, ImportOptions(), error);
    }

    // Ask `peerId` for its whole index; watch syncProgress/syncFinished.
    bool requestFromPeer(const QString& peerId, const ImportOptions& options, QString* error = nullptr);
    bool requestFromPeer(const QString& peerId, QString* error = nullptr)
    {
        return requestFromPeer(peerId, ImportOptions(), error);
    }

    // Stop the user's operation. An export removes its partial file; an import
    // keeps what it merged and saves a resume point.
    void cancel();

    // Cancel everything and block until no worker is inside the data layer. The
    // workers read and write the database, so they must be gone before Manticore
    // is stopped.
    void shutdown();

    bool isBusy() const;
    Status status() const;
    ServeStatus serveStatus() const;
    QJsonObject statusJson() const;

    // An import that was interrupted and can be continued, or an empty object.
    QJsonObject pendingImportJson() const;

    // Half-received peer dumps that a new pull from the same peer would continue,
    // newest first. One entry per peer; empty when there is nothing to continue.
    QJsonArray pendingTransfersJson() const;

    // Throw away the snapshot and build a fresh one now.
    bool rebuildSnapshot(QString* error = nullptr);

    // Whether we answer other peers' databaseRequest (config: databaseSharing).
    void setSharingEnabled(bool enabled);
    bool sharingEnabled() const { return sharingEnabled_; }

    // Peer-layer hooks. PeerApi owns the wire names and calls these; the service
    // owns the policy and the transfer.
    void handlePeerRequest(const QString& peerId, const QJsonObject& data);
    void handlePeerResponse(const QString& peerId, const QJsonObject& data);
    void handlePeerProgress(const QString& peerId, const QJsonObject& data);
    void handlePeerCancel(const QString& peerId, const QJsonObject& data);

signals:
    // The user's lane. These are the ones a GUI may put in front of the user.
    void syncStarted(const QJsonObject& info);
    void syncProgress(const QJsonObject& info);
    void syncFinished(bool success, const QJsonObject& summary);

    // The serving lane. Background: log it, show it in a status panel, never
    // interrupt with it.
    void serveProgress(const QJsonObject& info);
    void serveFinished(const QString& peerId, bool success, const QJsonObject& summary);

    // Human-readable one-liner for a status bar.
    void statusMessage(const QString& message, int timeoutMs);

private:
    // One run of the user's lane. Held by shared_ptr so a worker that is still
    // unwinding keeps looking at *its* cancel flag and *its* identity, and can
    // never finish an operation that has already been replaced.
    struct LocalOperation {
        quint64 id = 0;
        Operation operation = Operation::None;
        CancelToken cancel;
        QFuture<void> worker;
        ImportOptions importOptions;
        QString peerId; // peer pull only
        // The snapshot generation the peer said it would serve. Compared against
        // the note beside the partial before a single byte is continued.
        QString snapshotId;
        quint64 sessionId = 0; // id we put on the wire for this pull
        quint64 transferId = 0; // incoming transfer, 0 until the offer arrives
        qint64 deadlineMs = 0; // wall clock; 0 = not waiting on anything
        // The peer accepted and may now be doing work on our behalf. Only then is
        // there anything for a databaseCancel to stop; sending one after a refusal
        // would be noise.
        bool peerEngaged = false;
    };
    using LocalPtr = std::shared_ptr<LocalOperation>;

    // One peer we are serving.
    struct Serve {
        QString peerId;
        quint64 sessionId = 0; // the id *they* chose; echoed in every message
        quint64 transferId = 0; // outgoing transfer, 0 while waiting for a snapshot
        // Which snapshot generation this peer is reading. A newer one can be
        // published mid-transfer, so the prune needs to know what is still in use.
        QString file;
        bool waitingForSnapshot = false;
        qint64 startedMs = 0;
    };

    // Generation of the shared snapshot. At most one at a time, shared by every
    // waiter. Held by shared_ptr for the same reason a LocalOperation is: both
    // generations would write the same temporary file, so a job that is still
    // unwinding must not be able to publish its result or be mistaken for the
    // current one.
    struct SnapshotJob {
        quint64 id = 0;
        CancelToken cancel;
        QFuture<void> worker;
        qint64 processed = 0;
        qint64 total = 0;
    };
    using SnapshotPtr = std::shared_ptr<SnapshotJob>;

    // --- local lane --------------------------------------------------------
    LocalPtr beginLocal(Operation operation, const QString& path, const QString& peerId, QString* error);
    void finishLocal(const LocalPtr& op, bool success, const QString& error = QString());
    void runExport(const LocalPtr& op, const QString& path);
    void runImport(const LocalPtr& op, const QString& path, const ImportOptions& options);
    void startPullImport(const LocalPtr& op, const QString& dumpPath);

    // --- serving lane ------------------------------------------------------
    void refuse(const QString& peerId, quint64 sessionId, const QString& reason, int retryAfterSecs = 0);
    void offerSnapshot(Serve& serve);
    bool ensureSnapshotJob();
    void runSnapshot(const SnapshotPtr& job);
    void onSnapshotFinished(const SnapshotPtr& job, bool ok, qint64 torrents, const QString& error);
    // Takes the id by value: it is erased from serves_ before the rest of the
    // function runs, and a reference into the map would dangle at that point.
    //
    // `releaseSnapshot` is false only when the caller is about to re-add the same
    // peer as a waiter. Dropping the last waiter normally stops the generation, and
    // a peer that restarts its request would otherwise cancel the very snapshot it
    // just asked for.
    void dropServe(QString peerId, bool success, const QString& reason, bool releaseSnapshot = true);
    // Stop generating once the last peer that wanted it has gone.
    void cancelSnapshotIfUnwanted();
    // Delete superseded snapshot generations that no transfer is still reading.
    void pruneSnapshots();

    // --- shared ------------------------------------------------------------
    void publishLocal();
    void publishServe();
    void setStage(const LocalPtr& op, const QString& stage);
    void onWatchdog();

    // Transport callbacks.
    void onFileOffered(const QString& peerId, quint64 transferId, const QString& name, qint64 size);
    void onTransferProgress(
        const QString& peerId, quint64 transferId, bool sending, qint64 transferred, qint64 total, double bytesPerSec);
    void onTransferFinished(const QString& peerId, quint64 transferId, bool success, const QString& path);
    void onPeerDisconnected(const QString& peerId);

    // Paths, housekeeping, resume bookkeeping.
    QString transferDirectory() const;
    QString incomingPathFor(const QString& peerId) const;
    // Where a transfer from this peer accumulates, and the note saying what it is.
    // Both survive a failed transfer; that is the whole point of them.
    QString partialPathFor(const QString& peerId) const;
    QString partialStatePathFor(const QString& peerId) const;
    void discardPartial(const QString& peerId) const;
    QString resumeStatePath() const;
    void saveResumeState(const QString& path, qint64 offset, qint64 fileSize) const;
    qint64 loadResumeOffset(const QString& path, qint64 fileSize) const;
    void clearResumeState() const;
    void sweepTransferDirectory() const;
    // Free space on the volume holding `path`, or -1 when it cannot be told.
    static qint64 availableBytes(const QString& path);

    data::TorrentRepository* repository_;
    IndexingService* indexing_;
    net::P2PTransport* transport_;
    QString dataDirectory_;
    QString clientVersion_;

    DatabaseSnapshot snapshot_;

public:
    // Freshness rule for the dump we serve. Driven by config; see DatabaseSnapshot.
    void setSnapshotPolicy(const DatabaseSnapshot::Policy& policy);

private:
    mutable QMutex mutex_; // guards status_ and serve counters written by workers
    Status status_;

    LocalPtr local_;
    quint64 nextOperationId_ = 0;
    quint64 nextSessionId_ = 0;

    QHash<QString, Serve> serves_; // by peer id
    SnapshotPtr snapshotJob_;
    quint64 nextSnapshotJobId_ = 0;
    QHash<QString, qint64> lastRequestMs_; // per-peer rate limit

    bool sharingEnabled_ = false;
    qint64 lastHeartbeatMs_ = 0;
    qint64 lastProgressPublishMs_ = 0;
};

} // namespace rats::service

#endif // RATS_SERVICE_DATABASE_SYNC_SERVICE_H
