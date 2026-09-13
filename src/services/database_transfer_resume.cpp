#include "services/database_transfer_resume.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace rats::service {

bool DatabaseTransferResume::isValid() const
{
    return !peerId.isEmpty() && !snapshot.isEmpty() && size > 0;
}

DatabaseTransferResume DatabaseTransferResume::load(const QString& path)
{
    DatabaseTransferResume state;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return state;
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    state.peerId = obj["peer"].toString();
    state.snapshot = obj["snapshot"].toString();
    state.size = obj["size"].toVariant().toLongLong();
    return state;
}

bool DatabaseTransferResume::save(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const QJsonObject obj { { "peer", peerId }, { "snapshot", snapshot }, { "size", static_cast<double>(size) } };
    return file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact)) > 0;
}

void DatabaseTransferResume::clear(const QString& path)
{
    QFile::remove(path);
}

qint64 DatabaseTransferResume::offsetFor(
    const qint64 partialSize, const QString& fromPeer, const QString& offeredSnapshot, const qint64 offeredSize) const
{
    if (!isValid())
        return 0;
    // An offer with no generation name comes from a peer too old to have one. It
    // may well be serving the same file, but there is no way to tell, and the
    // wrong guess costs the whole download twice.
    if (offeredSnapshot.isEmpty() || peerId != fromPeer || snapshot != offeredSnapshot || size != offeredSize)
        return 0;
    // Nothing on disk is not a resume; as much as the whole file is not a prefix
    // of it, so it cannot be continued either.
    if (partialSize <= 0 || partialSize >= offeredSize)
        return 0;
    return partialSize;
}

} // namespace rats::service
