#ifndef RATS_SERVICE_EXPORT_SINK_H
#define RATS_SERVICE_EXPORT_SINK_H

#include "domain/torrent.h"

#include <QDateTime>
#include <QString>

namespace rats::service {

// Provenance of an export, written into whatever preamble the target format
// has (the .ratsdb JSON header, a CSV comment, nothing at all). `torrents` is
// the writer's *estimate* — the row count when the export started — so a sink
// can size a progress bar; the exact totals are only known at finish().
struct ExportHeader {
    QString client; // producing client version
    QString peerId; // producing node's peer id (informational)
    QDateTime created;
    qint64 torrents = 0;
};

// One output format of a whole-index export.
//
// DatabaseSyncService owns the hard part of an export — keyset pagination over
// millions of rows, the worker thread, progress publishing, cancellation — and
// knows nothing about the bytes it produces. A sink is the other half: it turns
// the torrent stream into a file. Adding a format means adding a sink, not a
// second export loop.
//
// The contract mirrors the lifecycle the exporter drives: open() once, write()
// per torrent in id order, then exactly one of finish() (the file is complete)
// or abort() (the file is removed). A sink is single-use and not thread-safe;
// it lives entirely on the worker thread.
class TorrentSink {
public:
    virtual ~TorrentSink() = default;

    // Create/truncate `path` and write any preamble. Returns false and fills
    // `error` if the file cannot be written.
    virtual bool open(const QString& path, const ExportHeader& header, QString* error = nullptr) = 0;

    // Append one torrent. `torrent.fileList` is populated only when the sink
    // asked for it through needsFiles().
    virtual bool write(const domain::Torrent& torrent, QString* error = nullptr) = 0;

    // Write any trailer and close. After this the file is complete. Safe to
    // call once; a second call is a no-op returning true.
    virtual bool finish(QString* error = nullptr) = 0;

    // Close and remove the partial output. Called on failure and cancellation,
    // and from the destructor of a sink that was never finished.
    virtual void abort() = 0;

    // Bytes on disk so far, for progress reporting.
    virtual qint64 bytesWritten() const = 0;

    // Whether write() needs `fileList` filled in. Returning false lets the
    // exporter skip the per-page file query entirely, which is the expensive
    // half of a full sweep — a flat format that only records the file *count*
    // should say so.
    virtual bool needsFiles() const = 0;
};

} // namespace rats::service

#endif // RATS_SERVICE_EXPORT_SINK_H
