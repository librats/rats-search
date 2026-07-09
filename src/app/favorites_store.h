#ifndef RATS_APP_FAVORITES_STORE_H
#define RATS_APP_FAVORITES_STORE_H

#include "domain/torrent.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QVector>

namespace rats::app {

// Personal favourites, persisted to favorites.json in the data directory.
// Stores the full domain::Torrent, so no parallel entry struct is needed.
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
    void favoritesChanged();

private:
    void rebuildIndex();

    QString filePath_;
    QVector<Entry> favorites_;
    QHash<QString, int> index_; // hash -> position
};

} // namespace rats::app

#endif // RATS_APP_FAVORITES_STORE_H
