#include "services/database_export.h"

#include "domain/torrent_codec.h"

#include <QByteArrayList>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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

constexpr char kBom[] = "\xEF\xBB\xBF";
constexpr char kEol[] = "\r\n";
constexpr char kTorrentColumns[] = "hash,name,size,files,pieceLength,added,contentType,contentCategory,seeders,"
                                   "leechers,completed,trackersChecked,good,bad,magnet,poster,description";
constexpr char kFileColumns[] = "hash,path,size";

// One RFC 4180 field out of untrusted text. The three transformations are the
// ones spelled out on CsvWriter: flatten control characters, defuse a leading
// formula trigger, then quote if what is left needs it.
QByteArray csvField(const QString& value)
{
    QString field;
    field.reserve(value.size() + 2);
    for (const QChar c : value)
        field += (c.unicode() < 0x20 || c.unicode() == 0x7F) ? QLatin1Char(' ') : c;

    if (!field.isEmpty()) {
        const QChar first = field.at(0);
        if (first == QLatin1Char('=') || first == QLatin1Char('+') || first == QLatin1Char('-')
            || first == QLatin1Char('@'))
            field.prepend(QLatin1Char('\''));
    }

    // Control characters are gone by now, so a comma or a double quote are the
    // only things left that can force quoting.
    if (!field.contains(QLatin1Char(',')) && !field.contains(QLatin1Char('"')))
        return field.toUtf8();

    field.replace(QLatin1Char('"'), QLatin1String("\"\""));
    return QByteArray("\"") + field.toUtf8() + QByteArray("\"");
}

// An ISO 8601 UTC instant, or an empty field for "never". Epoch 0 is how a
// never-scraped torrent is stored, not something that happened in 1970.
QByteArray csvDate(const QDateTime& when)
{
    if (!when.isValid() || when.toSecsSinceEpoch() <= 0)
        return {};
    return when.toUTC().toString(Qt::ISODate).toUtf8();
}

// "index.csv" -> "index.files.csv"; anything else just gains the suffix.
QString companionFilesPath(const QString& path)
{
    const QFileInfo info(path);
    if (info.suffix().compare(QLatin1String("csv"), Qt::CaseInsensitive) == 0)
        return info.dir().filePath(info.completeBaseName() + QStringLiteral(".files.csv"));
    return path + QStringLiteral(".files.csv");
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

// ===========================================================================
// CsvWriter
// ===========================================================================

CsvWriter::CsvWriter(Options options) : options_(options) { }

CsvWriter::CsvWriter() : CsvWriter(Options()) { }

CsvWriter::~CsvWriter()
{
    if (file_ && !finished_)
        abort();
}

bool CsvWriter::open(const QString& path, const ExportHeader&, QString* error)
{
    file_ = std::make_unique<QFile>(path);
    if (!file_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(error, file_->errorString());
        file_.reset();
        return false;
    }
    pending_ = QByteArray(kBom) + QByteArray(kTorrentColumns) + QByteArray(kEol);

    if (options_.includeFiles) {
        filesPath_ = companionFilesPath(path);
        filesFile_ = std::make_unique<QFile>(filesPath_);
        if (!filesFile_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            setError(error, filesFile_->errorString());
            abort(); // takes the main file with it: half an export is no export
            return false;
        }
        filesPending_ = QByteArray(kBom) + QByteArray(kFileColumns) + QByteArray(kEol);
    }
    return true;
}

bool CsvWriter::isOpen() const
{
    return file_ && file_->isOpen();
}

bool CsvWriter::write(const domain::Torrent& torrent, QString* error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("Export file is not open"));
        return false;
    }

    pending_ += QByteArrayList { csvField(torrent.hash), csvField(torrent.name), QByteArray::number(torrent.size),
        QByteArray::number(torrent.files), QByteArray::number(torrent.pieceLength), csvDate(torrent.added),
        csvField(domain::toString(torrent.contentType)), csvField(domain::toString(torrent.contentCategory)),
        QByteArray::number(torrent.seeders), QByteArray::number(torrent.leechers),
        QByteArray::number(torrent.completed), csvDate(torrent.trackersChecked), QByteArray::number(torrent.good),
        QByteArray::number(torrent.bad), csvField(torrent.magnetLink()),
        csvField(torrent.info.value(QLatin1String("poster")).toString()),
        csvField(torrent.info.value(QLatin1String("description")).toString()) }
                    .join(',');
    pending_ += kEol;
    ++written_;

    // fileList is populated only when needsFiles() asked the exporter for it.
    if (filesFile_) {
        for (const domain::File& file : torrent.fileList) {
            const QByteArrayList row { csvField(torrent.hash), csvField(file.path), QByteArray::number(file.size) };
            filesPending_ += row.join(',');
            filesPending_ += kEol;
            ++filesWritten_;
        }
    }

    if (pending_.size() >= kFlushBytes || filesPending_.size() >= kFlushBytes)
        return flush(error);
    return true;
}

bool CsvWriter::flush(QString* error)
{
    if (!pending_.isEmpty()) {
        if (file_->write(pending_) != pending_.size()) {
            setError(error, file_->errorString());
            return false;
        }
        pending_.clear();
    }
    if (filesFile_ && !filesPending_.isEmpty()) {
        if (filesFile_->write(filesPending_) != filesPending_.size()) {
            setError(error, filesFile_->errorString());
            return false;
        }
        filesPending_.clear();
    }
    return true;
}

bool CsvWriter::finish(QString* error)
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
    if (filesFile_)
        filesFile_->close();
    finished_ = true;
    return true;
}

void CsvWriter::abort()
{
    if (file_) {
        file_->close();
        file_->remove();
        file_.reset();
    }
    if (filesFile_) {
        filesFile_->close();
        filesFile_->remove();
        filesFile_.reset();
    }
    filesPath_.clear();
    pending_.clear();
    filesPending_.clear();
}

qint64 CsvWriter::bytesWritten() const
{
    qint64 total = file_ ? file_->size() + pending_.size() : 0;
    if (filesFile_)
        total += filesFile_->size() + filesPending_.size();
    return total;
}

} // namespace rats::service
