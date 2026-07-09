#include "app/favorites_store.h"

#include "domain/torrent_codec.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace rats::app {

FavoritesStore::FavoritesStore(const QString& dataDirectory, QObject* parent)
    : QObject(parent), filePath_(dataDirectory + QStringLiteral("/favorites.json"))
{
    load();
}

void FavoritesStore::load()
{
    QFile file(filePath_);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonArray array = QJsonDocument::fromJson(file.readAll()).array();
    file.close();

    favorites_.clear();
    for (const QJsonValue& v : array) {
        const QJsonObject obj = v.toObject();
        Entry entry;
        entry.torrent = domain::codec::torrentFromJson(obj);
        const qint64 favMs = obj["favoritedAt"].toVariant().toLongLong();
        entry.favoritedAt = favMs > 0 ? QDateTime::fromMSecsSinceEpoch(favMs) : QDateTime::currentDateTime();
        if (entry.torrent.isValid())
            favorites_.append(entry);
    }
    rebuildIndex();
}

void FavoritesStore::save()
{
    QJsonArray array;
    for (const Entry& e : favorites_) {
        QJsonObject obj = domain::codec::toJson(e.torrent, { /*includeFiles*/ false, /*includeInfo*/ false });
        obj["favoritedAt"] = e.favoritedAt.toMSecsSinceEpoch();
        array.append(obj);
    }
    QFile file(filePath_);
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

bool FavoritesStore::add(const domain::Torrent& torrent)
{
    if (!torrent.isValid() || index_.contains(torrent.hash))
        return false;
    index_.insert(torrent.hash, favorites_.size());
    favorites_.append(Entry { torrent, QDateTime::currentDateTime() });
    save();
    emit favoritesChanged();
    return true;
}

bool FavoritesStore::remove(const QString& hash)
{
    const auto it = index_.constFind(hash);
    if (it == index_.constEnd())
        return false;
    favorites_.remove(it.value());
    rebuildIndex();
    save();
    emit favoritesChanged();
    return true;
}

bool FavoritesStore::isFavorite(const QString& hash) const
{
    return index_.contains(hash);
}

void FavoritesStore::rebuildIndex()
{
    index_.clear();
    for (int i = 0; i < favorites_.size(); ++i)
        index_.insert(favorites_.at(i).torrent.hash, i);
}

} // namespace rats::app
