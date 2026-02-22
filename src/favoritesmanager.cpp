#include "favoritesmanager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QDebug>

// ============================================================================
// FavoriteEntry
// ============================================================================

QJsonObject FavoriteEntry::toJson() const
{
    QJsonObject obj;
    obj["hash"] = hash;
    obj["name"] = name;
    obj["size"] = size;
    obj["files"] = files;
    obj["seeders"] = seeders;
    obj["leechers"] = leechers;
    obj["completed"] = completed;
    obj["contentType"] = contentType;
    obj["contentCategory"] = contentCategory;
    obj["added"] = added.isValid() ? added.toMSecsSinceEpoch() : 0;
    obj["favoritedAt"] = favoritedAt.isValid() ? favoritedAt.toMSecsSinceEpoch() : 0;
    return obj;
}

FavoriteEntry FavoriteEntry::fromJson(const QJsonObject& obj)
{
    FavoriteEntry entry;
    entry.hash = obj["hash"].toString();
    entry.name = obj["name"].toString();
    entry.size = obj["size"].toVariant().toLongLong();
    entry.files = obj["files"].toInt();
    entry.seeders = obj["seeders"].toInt();
    entry.leechers = obj["leechers"].toInt();
    entry.completed = obj["completed"].toInt();
    entry.contentType = obj["contentType"].toString();
    entry.contentCategory = obj["contentCategory"].toString();
    
    qint64 addedMs = obj["added"].toVariant().toLongLong();
    if (addedMs > 0) {
        entry.added = QDateTime::fromMSecsSinceEpoch(addedMs);
    }
    
    qint64 favMs = obj["favoritedAt"].toVariant().toLongLong();
    if (favMs > 0) {
        entry.favoritedAt = QDateTime::fromMSecsSinceEpoch(favMs);
    }
    
    return entry;
}

// ============================================================================
// FavoritesManager
// ============================================================================

FavoritesManager::FavoritesManager(const QString& dataDirectory, QObject *parent)
    : QObject(parent)
    , filePath_(dataDirectory + "/favorites.json")
{
}

FavoritesManager::~FavoritesManager()
{
    save();
}

void FavoritesManager::load()
{
    favorites_.clear();
    hashIndex_.clear();
    
    QFile file(filePath_);
    if (!file.exists()) {
        qInfo() << "FavoritesManager: No favorites file found, starting empty";
        return;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "FavoritesManager: Failed to open favorites file:" << file.errorString();
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "FavoritesManager: Failed to parse favorites:" << error.errorString();
        return;
    }
    
    QJsonArray arr = doc.array();
    favorites_.reserve(arr.size());
    
    for (const QJsonValue& val : arr) {
        FavoriteEntry entry = FavoriteEntry::fromJson(val.toObject());
        if (entry.isValid()) {
            favorites_.append(entry);
        }
    }
    
    rebuildIndex();
    qInfo() << "FavoritesManager: Loaded" << favorites_.size() << "favorites";
}

void FavoritesManager::save()
{
    QJsonArray arr;
    for (const FavoriteEntry& entry : favorites_) {
        arr.append(entry.toJson());
    }
    
    QJsonDocument doc(arr);
    
    // Ensure directory exists
    QFileInfo fi(filePath_);
    QDir().mkpath(fi.absolutePath());
    
    QFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "FavoritesManager: Failed to save favorites:" << file.errorString();
        return;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    qInfo() << "FavoritesManager: Saved" << favorites_.size() << "favorites";
}

bool FavoritesManager::addFavorite(const FavoriteEntry& entry)
{
    if (!entry.isValid()) {
        qWarning() << "FavoritesManager: Cannot add invalid entry";
        return false;
    }
    
    if (hashIndex_.contains(entry.hash)) {
        qInfo() << "FavoritesManager: Already in favorites:" << entry.hash.left(16);
        return false;
    }
    
    FavoriteEntry newEntry = entry;
    if (!newEntry.favoritedAt.isValid()) {
        newEntry.favoritedAt = QDateTime::currentDateTime();
    }
    
    favorites_.append(newEntry);
    hashIndex_[newEntry.hash] = favorites_.size() - 1;
    
    save();
    
    emit favoriteAdded(newEntry.hash, newEntry.name);
    emit favoritesChanged();
    
    qInfo() << "FavoritesManager: Added to favorites:" << newEntry.name;
    return true;
}

bool FavoritesManager::removeFavorite(const QString& hash)
{
    if (!hashIndex_.contains(hash)) {
        return false;
    }
    
    int idx = hashIndex_[hash];
    favorites_.removeAt(idx);
    rebuildIndex();
    
    save();
    
    emit favoriteRemoved(hash);
    emit favoritesChanged();
    
    qInfo() << "FavoritesManager: Removed from favorites:" << hash.left(16);
    return true;
}

bool FavoritesManager::isFavorite(const QString& hash) const
{
    return hashIndex_.contains(hash);
}

QVector<FavoriteEntry> FavoritesManager::favorites() const
{
    return favorites_;
}

void FavoritesManager::rebuildIndex()
{
    hashIndex_.clear();
    for (int i = 0; i < favorites_.size(); ++i) {
        hashIndex_[favorites_[i].hash] = i;
    }
}
