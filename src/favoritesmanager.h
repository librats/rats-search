#ifndef FAVORITESMANAGER_H
#define FAVORITESMANAGER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

/**
 * @brief FavoriteEntry - a single favorite torrent entry
 */
struct FavoriteEntry {
    QString hash;               // info_hash (40 char hex)
    QString name;
    qint64 size = 0;
    int files = 0;
    int seeders = 0;
    int leechers = 0;
    int completed = 0;
    QString contentType;
    QString contentCategory;
    QDateTime added;            // When the torrent was originally added
    QDateTime favoritedAt;      // When added to favorites
    
    bool isValid() const { return !hash.isEmpty() && hash.length() == 40; }
    
    QJsonObject toJson() const;
    static FavoriteEntry fromJson(const QJsonObject& obj);
};

/**
 * @brief FavoritesManager - Manages personal favorites stored in a local JSON file
 * 
 * Favorites are persisted to favorites.json in the data directory.
 * Provides add/remove/check operations and emits signals for UI updates.
 */
class FavoritesManager : public QObject
{
    Q_OBJECT

public:
    explicit FavoritesManager(const QString& dataDirectory, QObject *parent = nullptr);
    ~FavoritesManager();
    
    /**
     * @brief Load favorites from disk
     */
    void load();
    
    /**
     * @brief Save favorites to disk
     */
    void save();
    
    /**
     * @brief Add a torrent to favorites
     * @return true if added (false if already exists)
     */
    bool addFavorite(const FavoriteEntry& entry);
    
    /**
     * @brief Remove a torrent from favorites by hash
     * @return true if removed
     */
    bool removeFavorite(const QString& hash);
    
    /**
     * @brief Check if a torrent is in favorites
     */
    bool isFavorite(const QString& hash) const;
    
    /**
     * @brief Get all favorites
     */
    QVector<FavoriteEntry> favorites() const;
    
    /**
     * @brief Get number of favorites
     */
    int count() const { return favorites_.size(); }

signals:
    void favoriteAdded(const QString& hash, const QString& name);
    void favoriteRemoved(const QString& hash);
    void favoritesChanged();

private:
    QString filePath_;
    QVector<FavoriteEntry> favorites_;
    QHash<QString, int> hashIndex_;  // hash -> index in favorites_ for fast lookup
    
    void rebuildIndex();
};

#endif // FAVORITESMANAGER_H
