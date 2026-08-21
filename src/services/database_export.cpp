#include "services/database_export.h"

#include "domain/torrent_codec.h"

#include <QFile>
#include <QJsonDocument>

namespace rats::service {

namespace {

// Rows accumulate here and go out in one write(): a syscall per torrent would
// dominate the runtime of a multi-million-row sweep.
constexpr int kFlushBytes = 256 * 1024;

void setError(QString* error, const QString& text)
{
    if (error)
        *error = text;
}

} // namespace

// ===========================================================================
// JsonLinesWriter
// ===========================================================================

JsonLinesWriter::JsonLinesWriter() = default;

JsonLinesWriter::~JsonLinesWriter()
{
    // Same rule as DumpWriter: a writer that never finished leaves no file, so a
    // cancelled export cannot be mistaken for a complete one.
    if (file_ && !finished_)
        abort();
}

bool JsonLinesWriter::open(const QString& path, const ExportHeader&, QString* error)
{
    file_ = std::make_unique<QFile>(path);
    if (!file_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(error, file_->errorString());
        file_.reset();
        return false;
    }
    return true;
}

bool JsonLinesWriter::isOpen() const
{
    return file_ && file_->isOpen();
}

bool JsonLinesWriter::write(const domain::Torrent& torrent, QString* error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("Export file is not open"));
        return false;
    }

    pending_ += QJsonDocument(domain::codec::toJson(torrent, { /*includeFiles*/ true, /*includeInfo*/ true }))
                    .toJson(QJsonDocument::Compact);
    pending_ += '\n';
    ++written_;

    if (pending_.size() >= kFlushBytes)
        return flush(error);
    return true;
}

bool JsonLinesWriter::flush(QString* error)
{
    if (pending_.isEmpty())
        return true;
    if (file_->write(pending_) != pending_.size()) {
        setError(error, file_->errorString());
        return false;
    }
    pending_.clear();
    return true;
}

bool JsonLinesWriter::finish(QString* error)
{
    if (finished_)
        return true;
    if (!isOpen()) {
        setError(error, QStringLiteral("Export file is not open"));
        return false;
    }

    if (!flush(error))
        return false;

    file_->close();
    finished_ = true;
    return true;
}

void JsonLinesWriter::abort()
{
    if (!file_)
        return;
    file_->close();
    file_->remove();
    file_.reset();
    pending_.clear();
}

qint64 JsonLinesWriter::bytesWritten() const
{
    // pending_ has not reached the file yet, but progress should count it.
    return file_ ? file_->size() + pending_.size() : 0;
}

} // namespace rats::service
