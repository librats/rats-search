#ifndef RATS_SERVICE_DATABASE_DUMP_H
#define RATS_SERVICE_DATABASE_DUMP_H

#include "domain/torrent.h"
#include "services/export_sink.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <memory>

class QFile;

namespace rats::service {

// The portable database-dump format (".ratsdb"): a whole torrent index written
// as a stream of compressed batches, so a multi-million-row index can be written
// and read back without ever being held in memory.
//
// Layout (all integers big-endian):
//
//   "RATSDB\0"                 7-byte magic
//   uint32 formatVersion       kFormatVersion
//   uint32 headerLen + bytes   compact JSON header (uncompressed: readable with
//                              a hex editor, and cheap to inspect before import)
//   repeated:
//     uint32 frameLen + bytes  qCompress()'d JSONL — one torrent per line
//   uint32 0                   end-of-frames marker
//   uint32 footerLen + bytes   compact JSON footer with the exact totals
//
// A torrent line is exactly domain::codec::toJson(t, {includeFiles: true}), so
// the dump and the P2P wire share one serialisation and cannot drift apart.
//
// A dump whose end-of-frames marker/footer is missing was truncated (the writer
// died, or the transfer was cut). It is still importable — every complete frame
// read before the truncation is valid — and DumpReader::complete() reports which
// case it was.
namespace dump {

inline constexpr quint32 kFormatVersion = 1;
// Torrents per frame. Large enough that the compressor sees repetition, small
// enough that one frame is a few hundred KB.
inline constexpr int kFrameTorrents = 500;

// What the writer puts in the header / the reader gets out of it. Everything
// but the format version is provenance shared with the other export formats,
// so it lives on ExportHeader; the exact row totals land in the footer.
struct Header : ExportHeader {
    quint32 version = kFormatVersion;
};

struct Footer {
    qint64 torrents = 0;
    qint64 files = 0;
    qint64 totalSize = 0;
};

} // namespace dump

// Streaming writer. Torrents are buffered until a frame is full, so callers can
// push one at a time without thinking about batching. This is the sink the peer
// transfer and the "keep my index" export use — the only format a DumpReader on
// the other end can merge back in.
class DumpWriter : public TorrentSink {
public:
    DumpWriter();
    ~DumpWriter() override;

    DumpWriter(const DumpWriter&) = delete;
    DumpWriter& operator=(const DumpWriter&) = delete;

    // Create/truncate `path` and write the magic + header. Returns false and
    // fills `error` if the file cannot be opened.
    bool open(const QString& path, const ExportHeader& header, QString* error = nullptr) override;
    bool isOpen() const;

    // Queue one torrent; flushes a frame once kFrameTorrents have accumulated.
    bool write(const domain::Torrent& torrent, QString* error = nullptr) override;

    // Flush the pending frame, then write the end marker and the footer. After
    // this the file is a complete dump. Safe to call once.
    bool finish(QString* error = nullptr) override;

    // Abort: closes and removes the partial file.
    void abort() override;

    // A dump is lossless, so it always wants the file list.
    bool needsFiles() const override { return true; }

    qint64 torrentsWritten() const { return written_; }
    qint64 bytesWritten() const override;

private:
    bool flushFrame(QString* error);

    std::unique_ptr<QFile> file_;
    QByteArray pending_; // JSONL of the frame being filled
    int pendingCount_ = 0;
    qint64 written_ = 0;
    dump::Footer footer_;
    bool finished_ = false;
};

// Streaming reader. Yields whole frames, which is also the natural unit for the
// importer's batched inserts.
class DumpReader {
public:
    DumpReader();
    ~DumpReader();

    DumpReader(const DumpReader&) = delete;
    DumpReader& operator=(const DumpReader&) = delete;

    // Open `path` and parse the magic + header. Returns false and fills `error`
    // when the file is not a dump or its format version is newer than we read.
    bool open(const QString& path, QString* error = nullptr);
    bool isOpen() const;

    const dump::Header& header() const { return header_; }
    // Only meaningful once atEnd() is true.
    const dump::Footer& footer() const { return footer_; }
    bool complete() const { return complete_; }
    bool atEnd() const { return atEnd_; }

    // Read the next frame into `out` (cleared first). Returns false on error or
    // at end-of-frames — check atEnd() to tell the two apart.
    bool readBatch(QVector<domain::Torrent>& out, QString* error = nullptr);

    // Byte offset of the next frame. Persisting this after a batch is applied
    // makes an interrupted import resumable via seekTo().
    qint64 offset() const;
    bool seekTo(qint64 offset, QString* error = nullptr);

    qint64 fileSize() const;

private:
    std::unique_ptr<QFile> file_;
    dump::Header header_;
    dump::Footer footer_;
    bool atEnd_ = false;
    bool complete_ = false;
};

} // namespace rats::service

#endif // RATS_SERVICE_DATABASE_DUMP_H
