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

// Comma-separated values (RFC 4180), for spreadsheets and ad-hoc analysis.
//
// This is the lossy sink, deliberately: a CSV row is flat, so the two nested
// halves of a torrent have to go somewhere. `info` is reduced to the two fields
// a human reads (poster, description), and the file list moves to a companion
// file — "<base>.files.csv", one row per file, joined back on `hash` — written
// only when the caller asks for it. When it is off the exporter skips the
// per-page file query altogether, which is the expensive half of a full sweep;
// the `files` *count* column survives either way because it lives on the
// torrent row itself.
//
// Three details exist purely because the audience is Excel:
//   - a UTF-8 BOM, without which Excel on Windows mojibakes every non-ASCII
//     name, and a DHT index is full of them;
//   - CRLF line endings, as the RFC specifies;
//   - a leading apostrophe on any field starting with =, +, - or @, which Excel
//     would otherwise execute as a formula. Every name in this database was
//     written by a stranger on the DHT, so that is untrusted input reaching a
//     spreadsheet (CWE-1236).
//
// Control characters inside a name are replaced with spaces rather than quoted:
// the RFC does permit embedded newlines inside quotes, but enough real-world
// parsers mis-handle them that keeping one record on one physical line is worth
// the substitution.
class CsvWriter : public TorrentSink {
public:
    struct Options {
        // Also write the companion "<base>.files.csv".
        bool includeFiles = false;
    };

    explicit CsvWriter(Options options);
    // GCC refuses to evaluate a nested struct's member initialisers in a default
    // argument while the enclosing class is still incomplete — the same reason
    // DatabaseSyncService::importFromFile carries overloads instead of one — so
    // the no-argument form is its own constructor.
    CsvWriter();
    ~CsvWriter() override;

    CsvWriter(const CsvWriter&) = delete;
    CsvWriter& operator=(const CsvWriter&) = delete;

    // Create/truncate `path` (and the companion file, when enabled) and queue
    // the BOM + column headers. `header` is unused: a provenance line would put
    // something other than a record in a CSV, which no reader expects.
    bool open(const QString& path, const ExportHeader& header, QString* error = nullptr) override;
    bool isOpen() const;

    bool write(const domain::Torrent& torrent, QString* error = nullptr) override;
    bool finish(QString* error = nullptr) override;
    void abort() override;

    bool needsFiles() const override { return options_.includeFiles; }

    // Companion file path, empty when the file list is not being written.
    QString filesPath() const { return filesPath_; }
    qint64 torrentsWritten() const { return written_; }
    qint64 filesWritten() const { return filesWritten_; }
    qint64 bytesWritten() const override;

private:
    bool flush(QString* error);

    Options options_;
    std::unique_ptr<QFile> file_;
    std::unique_ptr<QFile> filesFile_;
    QString filesPath_;
    QByteArray pending_;
    QByteArray filesPending_;
    qint64 written_ = 0;
    qint64 filesWritten_ = 0;
    bool finished_ = false;
};

} // namespace rats::service

#endif // RATS_SERVICE_DATABASE_EXPORT_H
