#ifndef RATS_SERVICE_DATABASE_SNAPSHOT_H
#define RATS_SERVICE_DATABASE_SNAPSHOT_H

#include <QDateTime>
#include <QSet>
#include <QString>

namespace rats::service {

// The dump this node hands to peers that ask for its database.
//
// One file, reused. The old design exported the whole index *per request*, which
// is where every timeout problem came from: the asking peer had to sit through a
// full export before the first byte moved, a second asker was refused as "busy",
// and five askers cost five identical exports. A snapshot turns the common case
// into "the file is already on disk, here it is", and leaves the slow path — a
// cold or stale snapshot — as the only case that needs a progress heartbeat.
//
// Each generation gets its own file name (`snapshot-<n>.ratsdb`) rather than
// overwriting the last. A regeneration routinely finishes while an earlier peer
// is still downloading the previous file, and that file cannot be removed or
// renamed over while it is open on Windows — so a fixed name would make the new
// snapshot fail to publish exactly when the node is busiest. Superseded files are
// pruned once nothing is reading them.
//
// This class is only the files plus their metadata and the freshness rule; the
// generation itself, and who is waiting for it, belong to DatabaseSyncService so
// that all the threading lives in one place.
class DatabaseSnapshot {
public:
    struct Info {
        bool valid = false;
        qint64 torrents = 0; // rows the snapshot contains
        qint64 bytes = 0; // size of the dump on disk
        QDateTime created;
        QString fileName; // e.g. "snapshot-7.ratsdb"
    };

    struct Policy {
        // Rebuild once the snapshot is older than this. A search index is merged,
        // not mirrored, so a few hours of drift costs a peer nothing but a few
        // torrents it will pick up from gossip anyway.
        qint64 maxAgeSecs = 6 * 60 * 60;
        // Rebuild once the index has drifted this far from what the snapshot holds,
        // in either direction (a shrinking index means rows were removed).
        double maxDriftRatio = 0.10;
        // Below this the index is small enough that drift ratios are noise; use an
        // absolute threshold instead so a nearly empty index is not rebuilt on
        // every single new torrent.
        qint64 minDriftTorrents = 500;
    };

    // Two overloads rather than a `Policy policy = {}` default argument: GCC delays
    // parsing a nested class's default member initializers until the enclosing class
    // is complete, so `{}` here is rejected as a conversion from an empty initializer
    // list (MSVC accepts it, which is why this only broke the Linux build).
    explicit DatabaseSnapshot(QString directory);
    DatabaseSnapshot(QString directory, Policy policy);

    // Freshness is config-driven, and config arrives after construction.
    void setPolicy(const Policy& policy) { policy_ = policy; }
    Policy policy() const { return policy_; }

    // Path of the live dump (empty when there is none), and of the file a
    // generation writes into. The temporary is renamed to a *new* name on
    // success, so a crashed or cancelled generation never leaves a half-written
    // file that looks ready, and a finished one never has to overwrite a file a
    // peer is still reading.
    QString path() const;
    QString temporaryPath() const;
    QString metadataPath() const;

    // Whether `fileName` is one of ours — the live dump or a superseded
    // generation. Used by the startup sweep, which must not mistake a snapshot
    // for the wreckage it is there to clear.
    static bool isSnapshotFile(const QString& fileName);

    // Absolute path of a generation still on disk, live or superseded; empty when
    // there is no such file. This is what lets a peer continue an interrupted
    // download: it names the generation it already holds bytes of and we serve
    // that file rather than the newest one, which would restart it from zero.
    //
    // `fileName` comes off the wire, so only a bare snapshot-<n>.ratsdb is
    // accepted — never a path, and never the metadata or the half-written
    // temporary, neither of which is a dump a peer may read.
    QString pathForGeneration(const QString& fileName) const;

    // Re-read the metadata from disk. Called once at startup; a snapshot whose
    // dump is missing or whose size disagrees with the metadata is discarded.
    void load();

    Info info() const { return info_; }

    // Whether the snapshot can be served as-is against an index of `torrents`
    // rows. False when there is no snapshot, when it has aged out, or when the
    // index has moved too far from it.
    bool isFresh(qint64 torrents) const;

    // Promote the temporary file to a new live snapshot and record its metadata.
    bool commit(qint64 torrents, QString* error = nullptr);

    // Drop the snapshot and its metadata (used when the file turns out to be
    // unreadable, and by an explicit rebuild).
    void discard();

    // Remove superseded generations. `inUse` holds absolute paths a transfer is
    // still reading; those and the live file are kept. A removal that fails
    // because the file is still open is not an error — the next prune, or the
    // startup sweep, will get it.
    void pruneSuperseded(const QSet<QString>& inUse) const;

    // Bytes of dump per torrent, measured from the snapshot we have. Used to
    // predict how much room the next generation needs; falls back to a rough
    // constant before the first one exists.
    double bytesPerTorrent() const;

private:
    void writeMetadata() const;

    QString directory_;
    Policy policy_;
    Info info_;
    quint64 generation_ = 0;
};

} // namespace rats::service

#endif // RATS_SERVICE_DATABASE_SNAPSHOT_H
