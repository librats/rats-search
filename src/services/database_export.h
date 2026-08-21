#ifndef RATS_SERVICE_DATABASE_EXPORT_H
#define RATS_SERVICE_DATABASE_EXPORT_H

#include "services/export_sink.h"

#include <QByteArray>
#include <QString>
#include <memory>

class QFile;

// Interchange export formats: the sinks that write the index for *other*
// programs to read.
//
// Unlike the .ratsdb dump next door these are one-way — nothing here can be
// imported back — which is exactly what frees them to be uncompressed,
// human-readable and (for CSV) lossy. Moving an index between two Rats installs
// is still database_dump.h's job; this is the "open it in something else" half.
namespace rats::service {

// Newline-delimited JSON (JSONL / NDJSON): one compact JSON object per line.
//
// Each line is byte-for-byte the line a .ratsdb frame holds — the same
// domain::codec::toJson(t, {files, info}) — so the two exports cannot drift
// apart and JSONL is the lossless view of everything a dump carries. The
// announcing ipv4/port are absent here because the codec never serialises them;
// that privacy boundary is inherited, not re-decided.
//
// There is deliberately NO metadata/header line: `jq`, pandas
// (read_json(lines=True)) and DuckDB (read_json_auto) all assume every line is
// a record, and a preamble would break the format for its only audience. The
// ExportHeader is therefore accepted and ignored.
class JsonLinesWriter : public TorrentSink {
public:
    JsonLinesWriter();
    ~JsonLinesWriter() override;

    JsonLinesWriter(const JsonLinesWriter&) = delete;
    JsonLinesWriter& operator=(const JsonLinesWriter&) = delete;

    // Create/truncate `path`. `header` is unused (see above).
    bool open(const QString& path, const ExportHeader& header, QString* error = nullptr) override;
    bool isOpen() const;

    // Append one LF-terminated JSON object, flushing once the buffer is full.
    bool write(const domain::Torrent& torrent, QString* error = nullptr) override;

    // Flush the buffer and close. Safe to call once.
    bool finish(QString* error = nullptr) override;

    // Close and remove the partial file.
    void abort() override;

    // The line embeds "files_list", so the file query is needed.
    bool needsFiles() const override { return true; }

    qint64 torrentsWritten() const { return written_; }
    qint64 bytesWritten() const override;

private:
    bool flush(QString* error);

    std::unique_ptr<QFile> file_;
    QByteArray pending_;
    qint64 written_ = 0;
    bool finished_ = false;
};

} // namespace rats::service

#endif // RATS_SERVICE_DATABASE_EXPORT_H
