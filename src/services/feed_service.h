#ifndef RATS_SERVICE_FEED_SERVICE_H
#define RATS_SERVICE_FEED_SERVICE_H

#include "domain/torrent.h"

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QVector>

class QTimer;

namespace rats::data {
class FeedRepository;
class TorrentRepository;
} // namespace rats::data

namespace rats::service {

// A single entry in the voted-torrents feed: a domain torrent plus the moment
// it entered the feed. `feedDate` drives the recency half of the ranking and is
// the only field the feed adds on top of the plain torrent — everything else
// lives on the torrent itself (votes, seeders, files), so the codec serialises
// the torrent and only `feedDate` is stitched in alongside.
struct FeedItem {
    domain::Torrent torrent;
    qint64 feedDate = 0;
};

// The ranked, in-memory voted-torrents feed. Items are ordered by a
// recency+votes score (see calculateScore) and capped at maxSize_. Persistence
// is delegated to a data::FeedRepository and addByHash lookups to a
// data::TorrentRepository.
//
// Persistence is debounced: mutations only mark the feed dirty and a short
// coalescing timer flushes one full rewrite. save() forces an immediate flush
// and the destructor flushes if dirty.
class FeedService : public QObject {
    Q_OBJECT

public:
    FeedService(data::FeedRepository* repository, data::TorrentRepository* torrents, QObject* parent = nullptr);
    ~FeedService() override;

    // Load the feed from the repository. Blocked content is filtered out on load.
    bool load();
    // Force an immediate persist of the current feed (bypasses the debounce).
    bool save();

    int size() const;
    qint64 feedDate() const;

    // Look the hash up in the torrent repository (with files) and add it.
    void addByHash(const QString& hash);

    // Page of feed items, ranked.
    QVector<FeedItem> getFeed(int index = 0, int limit = 20) const;

    // JSON representation used by the REST/WS API and P2P feed exchange.
    QJsonArray toJsonArray(int index = 0, int limit = 20) const;
    // Replace the whole feed from a remote peer's JSON (P2P sync).
    void fromJsonArray(const QJsonArray& array, qint64 remoteFeedDate = 0);

signals:
    void feedUpdated();

private:
    // Add or update a torrent in the feed. Existing items keep their feedDate but
    // refresh votes/seeders; new items get the current time if unset.
    void add(const FeedItem& item);

    void reorder();
    void rebuildIndex();
    double calculateScore(const FeedItem& item) const;

    // Debounced persistence.
    void markDirty();
    void flush(); // flush only if dirty (timer slot)
    bool persistNow(); // always rewrite the feed table

    // Serialise the whole feed / a single item to the stored JSON shape.
    QJsonArray toStoredArray() const;

    data::FeedRepository* repository_;
    data::TorrentRepository* torrents_;
    QVector<FeedItem> feed_;
    QHash<QString, int> index_; // hash -> position in feed_ (kept in sync with reorder)
    QTimer* flushTimer_;
    int maxSize_ = 1000;
    qint64 feedDate_ = 0;
    bool dirty_ = false;
};

} // namespace rats::service

#endif // RATS_SERVICE_FEED_SERVICE_H
