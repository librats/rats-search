#include "services/database_dump.h"

#include "domain/torrent_codec.h"

#include <QFile>
#include <QJsonDocument>
#include <QtEndian>

#include <cstring>

namespace rats::service {

namespace {

constexpr char kMagic[] = "RATSDB";
constexpr int kMagicSize = 7; // "RATSDB" plus the trailing NUL
// A frame that claims more than this is a corrupt or hostile file, not one we
// truncated: reject it before allocating.
constexpr quint32 kMaxFrameBytes = 64u * 1024u * 1024u;

void setError(QString* error, const QString& text)
{
    if (error)
        *error = text;
}

bool writeU32(QFile& file, quint32 value)
{
    const quint32 be = qToBigEndian(value);
    return file.write(reinterpret_cast<const char*>(&be), 4) == 4;
}

bool readU32(QFile& file, quint32& value)
{
    char buffer[4];
    if (file.read(buffer, 4) != 4)
        return false;
    value = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(buffer));
    return true;
}

// Length-prefixed block: uint32 length followed by the payload.
bool writeBlock(QFile& file, const QByteArray& payload)
{
    return writeU32(file, static_cast<quint32>(payload.size())) && file.write(payload) == payload.size();
}

QByteArray headerToJson(const ExportHeader& header)
{
    QJsonObject obj;
    obj["format"] = QStringLiteral("rats-search-db");
    obj["version"] = static_cast<int>(dump::kFormatVersion);
    obj["client"] = header.client;
    obj["peerId"] = header.peerId;
    obj["created"] = (header.created.isValid() ? header.created : QDateTime::currentDateTime()).toString(Qt::ISODate);
    obj["torrents"] = static_cast<double>(header.torrents);
    obj["compression"] = QStringLiteral("zlib");
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QByteArray footerToJson(const dump::Footer& footer)
{
    QJsonObject obj;
    obj["torrents"] = static_cast<double>(footer.torrents);
    obj["files"] = static_cast<double>(footer.files);
    obj["totalSize"] = static_cast<double>(footer.totalSize);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

} // namespace

// ===========================================================================
// DumpWriter
// ===========================================================================

DumpWriter::DumpWriter() = default;

DumpWriter::~DumpWriter()
{
    // A writer destroyed without finish() would leave a file that looks like a
    // dump but has no footer; drop it instead.
    if (file_ && !finished_)
        abort();
}

bool DumpWriter::open(const QString& path, const ExportHeader& header, QString* error)
{
    file_ = std::make_unique<QFile>(path);
    if (!file_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(error, file_->errorString());
        file_.reset();
        return false;
    }

    if (file_->write(kMagic, kMagicSize) != kMagicSize || !writeU32(*file_, dump::kFormatVersion)
        || !writeBlock(*file_, headerToJson(header))) {
        setError(error, file_->errorString());
        abort();
        return false;
    }
    return true;
}

bool DumpWriter::isOpen() const
{
    return file_ && file_->isOpen();
}

bool DumpWriter::write(const domain::Torrent& torrent, QString* error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("Dump file is not open"));
        return false;
    }

    pending_ += QJsonDocument(domain::codec::toJson(torrent, { /*includeFiles*/ true, /*includeInfo*/ true }))
                    .toJson(QJsonDocument::Compact);
    pending_ += '\n';
    ++pendingCount_;
    ++written_;
    footer_.torrents = written_;
    footer_.files += torrent.files;
    footer_.totalSize += torrent.size;

    if (pendingCount_ >= dump::kFrameTorrents)
        return flushFrame(error);
    return true;
}

bool DumpWriter::flushFrame(QString* error)
{
    if (pending_.isEmpty())
        return true;

    const QByteArray compressed = qCompress(pending_, 6);
    pending_.clear();
    pendingCount_ = 0;

    if (compressed.size() > static_cast<int>(kMaxFrameBytes)) {
        setError(error, QStringLiteral("Dump frame exceeds the maximum frame size"));
        return false;
    }
    if (!writeBlock(*file_, compressed)) {
        setError(error, file_->errorString());
        return false;
    }
    return true;
}

bool DumpWriter::finish(QString* error)
{
    if (finished_)
        return true;
    if (!isOpen()) {
        setError(error, QStringLiteral("Dump file is not open"));
        return false;
    }

    if (!flushFrame(error))
        return false;
    if (!writeU32(*file_, 0) || !writeBlock(*file_, footerToJson(footer_))) {
        setError(error, file_->errorString());
        return false;
    }

    file_->close();
    finished_ = true;
    return true;
}

void DumpWriter::abort()
{
    if (!file_)
        return;
    file_->close();
    file_->remove();
    file_.reset();
    pending_.clear();
    pendingCount_ = 0;
}

qint64 DumpWriter::bytesWritten() const
{
    return file_ ? file_->size() : 0;
}

// ===========================================================================
// DumpReader
// ===========================================================================

DumpReader::DumpReader() = default;
DumpReader::~DumpReader() = default;

bool DumpReader::open(const QString& path, QString* error)
{
    file_ = std::make_unique<QFile>(path);
    if (!file_->open(QIODevice::ReadOnly)) {
        setError(error, file_->errorString());
        file_.reset();
        return false;
    }

    char magic[kMagicSize];
    if (file_->read(magic, kMagicSize) != kMagicSize || std::memcmp(magic, kMagic, kMagicSize) != 0) {
        setError(error, QStringLiteral("Not a Rats database dump"));
        file_.reset();
        return false;
    }

    quint32 version = 0;
    if (!readU32(*file_, version)) {
        setError(error, QStringLiteral("Truncated dump header"));
        file_.reset();
        return false;
    }
    if (version > dump::kFormatVersion) {
        setError(error,
            QStringLiteral("Dump format version %1 is newer than this client understands (%2)")
                .arg(version)
                .arg(dump::kFormatVersion));
        file_.reset();
        return false;
    }

    quint32 headerLen = 0;
    if (!readU32(*file_, headerLen) || headerLen > kMaxFrameBytes) {
        setError(error, QStringLiteral("Truncated dump header"));
        file_.reset();
        return false;
    }
    const QByteArray headerBytes = file_->read(headerLen);
    if (static_cast<quint32>(headerBytes.size()) != headerLen) {
        setError(error, QStringLiteral("Truncated dump header"));
        file_.reset();
        return false;
    }

    const QJsonObject obj = QJsonDocument::fromJson(headerBytes).object();
    header_.version = version;
    header_.client = obj["client"].toString();
    header_.peerId = obj["peerId"].toString();
    header_.created = QDateTime::fromString(obj["created"].toString(), Qt::ISODate);
    header_.torrents = obj["torrents"].toVariant().toLongLong();
    return true;
}

bool DumpReader::isOpen() const
{
    return file_ && file_->isOpen();
}

bool DumpReader::readBatch(QVector<domain::Torrent>& out, QString* error)
{
    out.clear();
    if (!isOpen()) {
        setError(error, QStringLiteral("Dump file is not open"));
        return false;
    }
    if (atEnd_)
        return false;

    quint32 frameLen = 0;
    if (!readU32(*file_, frameLen)) {
        // No end marker: the producer died or the transfer was cut. Everything
        // read so far is still valid, so this is an end, not a hard error.
        atEnd_ = true;
        complete_ = false;
        return false;
    }

    if (frameLen == 0) {
        // End of frames; the footer carries the exact totals.
        atEnd_ = true;
        quint32 footerLen = 0;
        if (readU32(*file_, footerLen) && footerLen <= kMaxFrameBytes) {
            const QByteArray bytes = file_->read(footerLen);
            if (static_cast<quint32>(bytes.size()) == footerLen) {
                const QJsonObject obj = QJsonDocument::fromJson(bytes).object();
                footer_.torrents = obj["torrents"].toVariant().toLongLong();
                footer_.files = obj["files"].toVariant().toLongLong();
                footer_.totalSize = obj["totalSize"].toVariant().toLongLong();
                complete_ = true;
            }
        }
        return false;
    }

    if (frameLen > kMaxFrameBytes) {
        setError(error, QStringLiteral("Corrupt dump: frame length out of range"));
        atEnd_ = true;
        return false;
    }

    const QByteArray compressed = file_->read(frameLen);
    if (static_cast<quint32>(compressed.size()) != frameLen) {
        atEnd_ = true;
        complete_ = false;
        setError(error, QStringLiteral("Truncated dump frame"));
        return false;
    }

    const QByteArray jsonl = qUncompress(compressed);
    if (jsonl.isEmpty()) {
        setError(error, QStringLiteral("Corrupt dump: frame could not be decompressed"));
        atEnd_ = true;
        return false;
    }

    const QList<QByteArray> lines = jsonl.split('\n');
    out.reserve(lines.size());
    for (const QByteArray& line : lines) {
        if (line.isEmpty())
            continue;
        const QJsonObject obj = QJsonDocument::fromJson(line).object();
        if (obj.isEmpty())
            continue;
        out.append(domain::codec::torrentFromJson(obj));
    }
    return true;
}

qint64 DumpReader::offset() const
{
    return file_ ? file_->pos() : 0;
}

bool DumpReader::seekTo(qint64 offset, QString* error)
{
    if (!isOpen()) {
        setError(error, QStringLiteral("Dump file is not open"));
        return false;
    }
    if (offset < 0 || offset > file_->size()) {
        setError(error, QStringLiteral("Resume offset is outside the dump"));
        return false;
    }
    if (!file_->seek(offset)) {
        setError(error, file_->errorString());
        return false;
    }
    atEnd_ = false;
    complete_ = false;
    return true;
}

qint64 DumpReader::fileSize() const
{
    return file_ ? file_->size() : 0;
}

} // namespace rats::service
