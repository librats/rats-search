#ifndef RATS_APP_FAVORITES_STORE_H
#define RATS_APP_FAVORITES_STORE_H

#include "domain/torrent.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QVector>

namespace rats::app {

// Personal favourites, persisted to favorites.json in the data directory. Ported
// from the old FavoritesManager but stores the full domain::Torrent instead of a
// parallel FavoriteEntry struct.
class FavoritesStore : public QObject {
    Q_OBJECT

public:
    struct Entry {
        domain::Torrent torrent;
        QDateTime favoritedAt;
    };

    explicit FavoritesStore(const QString& dataDirectory, QObject* parent = nullptr);

    void load();
    void save();

    bool add(const domain::Torrent& torrent); // false if already present
    bool remove(const QString& hash);
    bool isFavorite(const QString& hash) const;
    QVector<Entry> favorites() const { return favorites_; }
    int count() const { return favorites_.size(); }

signals:
    void favoriteAdded(const QString& hash, const QString& name);
    void favoriteRemoved(const QString& hash);
    void favoritesChanged();

private:
    void rebuildIndex();

    QString filePath_;
    QVector<Entry> favorites_;
    QHash<QString, int> index_; // hash -> position
};

} // namespace rats::app

#endif // RATS_APP_FAVORITES_STORE_H
