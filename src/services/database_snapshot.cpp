#include "services/database_snapshot.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

namespace rats::service {

namespace {

constexpr char kNamePrefix[] = "snapshot-";
constexpr char kNameSuffix[] = ".ratsdb";
constexpr char kTempName[] = "snapshot.ratsdb.part";
constexpr char kMetaName[] = "snapshot.json";

// Used only until a snapshot exists to measure against. A torrent with its file
// list is a couple of kilobytes of JSON that zlib takes down to a few hundred
// bytes; this is deliberately on the generous side, because the number is spent
// on a free-space check and guessing low there is the harmful direction.
constexpr double kAssumedBytesPerTorrent = 512.0;

QString nameFor(quint64 generation)
{
    return QLatin1String(kNamePrefix) + QString::number(generation) + QLatin1String(kNameSuffix);
}

} // namespace

DatabaseSnapshot::DatabaseSnapshot(QString directory) : DatabaseSnapshot(std::move(directory), Policy()) { }

DatabaseSnapshot::DatabaseSnapshot(QString directory, Policy policy) : directory_(std::move(directory)), policy_(policy)
{
}

bool DatabaseSnapshot::isSnapshotFile(const QString& fileName)
{
    return fileName == QLatin1String(kMetaName) || fileName == QLatin1String(kTempName)
        || (fileName.startsWith(QLatin1String(kNamePrefix)) && fileName.endsWith(QLatin1String(kNameSuffix)));
}

QString DatabaseSnapshot::pathForGeneration(const QString& fileName) const
{
    if (!fileName.startsWith(QLatin1String(kNamePrefix)) || !fileName.endsWith(QLatin1String(kNameSuffix)))
        return {};
    constexpr qsizetype prefix = sizeof(kNamePrefix) - 1;
    constexpr qsizetype suffix = sizeof(kNameSuffix) - 1;
    const QString generation = fileName.mid(prefix, fileName.size() - prefix - suffix);
    if (generation.isEmpty())
        return {};
    // Digits only: that rules out a separator, a "..", and anything else a peer
    // might hope turns this into a path of its choosing.
    for (const QChar c : generation) {
        if (!c.isDigit())
            return {};
    }
    const QString absolute = QDir(directory_).absoluteFilePath(fileName);
    return QFileInfo::exists(absolute) ? absolute : QString();
}

QString DatabaseSnapshot::path() const
{
    if (info_.fileName.isEmpty())
        return {};
    return QDir(directory_).absoluteFilePath(info_.fileName);
}

QString DatabaseSnapshot::temporaryPath() const
{
    return QDir(directory_).absoluteFilePath(QLatin1String(kTempName));
}

QString DatabaseSnapshot::metadataPath() const
{
    return QDir(directory_).absoluteFilePath(QLatin1String(kMetaName));
}

void DatabaseSnapshot::load()
{
    info_ = Info {};

    QFile meta(metadataPath());
    if (!meta.open(QIODevice::ReadOnly))
        return;
    const QJsonObject obj = QJsonDocument::fromJson(meta.readAll()).object();
    meta.close();

    // Resume the counter past whatever the last run reached, so a new generation
    // cannot land on a name a peer is still downloading from the previous process.
    generation_ = obj["generation"].toVariant().toULongLong();

    const QString fileName = obj["file"].toString();
    if (fileName.isEmpty())
        return;
    const QFileInfo dump(QDir(directory_).absoluteFilePath(fileName));
    if (!dump.exists())
        return;
    // The metadata is the only record of how many torrents the dump holds, so a
    // size that disagrees with it means the two are out of step and neither can be
    // trusted — most likely a crash between writing the file and the metadata.
    if (obj["bytes"].toVariant().toLongLong() != dump.size()) {
        qWarning() << "[DatabaseSnapshot] metadata does not match the dump; discarding";
        QFile::remove(dump.absoluteFilePath());
        QFile::remove(metadataPath());
        return;
    }

    info_.valid = true;
    info_.fileName = fileName;
    info_.torrents = obj["torrents"].toVariant().toLongLong();
    info_.bytes = dump.size();
    info_.created = QDateTime::fromString(obj["created"].toString(), Qt::ISODate);
    qInfo() << "[DatabaseSnapshot] loaded" << info_.fileName << "-" << info_.torrents << "torrents," << info_.bytes
            << "bytes, created" << info_.created.toString(Qt::ISODate);
}

bool DatabaseSnapshot::isFresh(qint64 torrents) const
{
    if (!info_.valid || info_.torrents <= 0)
        return false;
    if (!info_.created.isValid())
        return false;
    if (info_.created.secsTo(QDateTime::currentDateTime()) > policy_.maxAgeSecs)
        return false;

    const qint64 drift = qAbs(torrents - info_.torrents);
    if (drift <= policy_.minDriftTorrents)
        return true;
    return static_cast<double>(drift) / static_cast<double>(info_.torrents) <= policy_.maxDriftRatio;
}

bool DatabaseSnapshot::commit(qint64 torrents, QString* error)
{
    const QString temp = temporaryPath();
    if (!QFileInfo::exists(temp)) {
        if (error)
            *error = QStringLiteral("No generated snapshot to publish");
        return false;
    }

    // A fresh name every time: the previous generation may still be open for a
    // peer that is mid-download, and renaming over it would fail.
    const QString fileName = nameFor(++generation_);
    const QString livePath = QDir(directory_).absoluteFilePath(fileName);
    if (!QFile::rename(temp, livePath)) {
        if (error)
            *error = QStringLiteral("Could not publish the snapshot");
        QFile::remove(temp);
        return false;
    }

    info_.valid = true;
    info_.fileName = fileName;
    info_.torrents = torrents;
    info_.bytes = QFileInfo(livePath).size();
    info_.created = QDateTime::currentDateTime();
    writeMetadata();
    qInfo() << "[DatabaseSnapshot] published" << fileName << "-" << info_.torrents << "torrents," << info_.bytes
            << "bytes";
    return true;
}

void DatabaseSnapshot::discard()
{
    if (!info_.fileName.isEmpty())
        QFile::remove(QDir(directory_).absoluteFilePath(info_.fileName));
    QFile::remove(metadataPath());
    info_ = Info {};
}

void DatabaseSnapshot::pruneSuperseded(const QSet<QString>& inUse) const
{
    const QDir dir(directory_);
    const QString live = path();
    const QStringList names
        = dir.entryList({ QLatin1String(kNamePrefix) + QStringLiteral("*") + QLatin1String(kNameSuffix) }, QDir::Files);
    for (const QString& name : names) {
        const QString absolute = dir.absoluteFilePath(name);
        if (absolute == live || inUse.contains(absolute))
            continue;
        if (QFile::remove(absolute))
            qInfo() << "[DatabaseSnapshot] removed superseded" << name;
    }
}

double DatabaseSnapshot::bytesPerTorrent() const
{
    if (!info_.valid || info_.torrents <= 0 || info_.bytes <= 0)
        return kAssumedBytesPerTorrent;
    return static_cast<double>(info_.bytes) / static_cast<double>(info_.torrents);
}

void DatabaseSnapshot::writeMetadata() const
{
    QFile meta(metadataPath());
    if (!meta.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[DatabaseSnapshot] could not write" << metadataPath();
        return;
    }
    const QJsonObject obj { { "file", info_.fileName }, { "generation", static_cast<double>(generation_) },
        { "torrents", static_cast<double>(info_.torrents) }, { "bytes", static_cast<double>(info_.bytes) },
        { "created", info_.created.toString(Qt::ISODate) } };
    meta.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

} // namespace rats::service
