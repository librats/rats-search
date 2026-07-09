#include "services/feed_service.h"

#include "data/feed_repository.h"
#include "data/torrent_repository.h"
#include "domain/torrent_codec.h"

#include <QDateTime>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace rats::service {

using domain::ContentCategory;
using domain::ContentType;
using domain::Torrent;

namespace {

// Debounce window for coalescing feed writes into a single table rewrite.
constexpr int kFlushIntervalMs = 2000;

// Bad or xxx content never enters the feed (matches the legacy filter).
bool shouldBlock(const Torrent& torrent)
{
    return torrent.contentType == ContentType::Bad || torrent.contentCategory == ContentCategory::XXX;
}

// Stored/wire shape: the torrent JSON (with files, for P2P replication) plus
// the extra feedDate. Info is intentionally omitted to keep the feed payload
// lean.
QJsonObject itemToJson(const FeedItem& item)
{
    QJsonObject obj = domain::codec::toJson(item.torrent, { /*includeFiles*/ true, /*includeInfo*/ false });
    obj["feedDate"] = item.feedDate;
    return obj;
}

FeedItem itemFromJson(const QJsonObject& obj)
{
    FeedItem item;
    item.torrent = domain::codec::torrentFromJson(obj);
    item.feedDate = obj["feedDate"].toVariant().toLongLong();
    return item;
}

} // namespace

FeedService::FeedService(data::FeedRepository* repository, data::TorrentRepository* torrents, QObject* parent)
    : QObject(parent), repository_(repository), torrents_(torrents), flushTimer_(new QTimer(this))
{
    flushTimer_->setSingleShot(true);
    flushTimer_->setInterval(kFlushIntervalMs);
    connect(flushTimer_, &QTimer::timeout, this, &FeedService::flush);
}

FeedService::~FeedService()
{
    if (dirty_)
        persistNow();
}

bool FeedService::load()
{
    const QJsonArray stored = repository_ ? repository_->load(maxSize_) : QJsonArray();

    feed_.clear();
    for (const auto& value : stored) {
        if (!value.isObject())
            continue;
        FeedItem item = itemFromJson(value.toObject());
        if (item.torrent.hash.isEmpty())
            continue;
        // Filter existing blocked content on load.
        if (shouldBlock(item.torrent)) {
            qInfo() << "[FeedService] filtering blocked content on load:" << item.torrent.hash.left(8);
            continue;
        }
        feed_.append(item);
    }

    reorder();

    feedDate_ = 0;
    for (const FeedItem& item : feed_)
        feedDate_ = qMax(feedDate_, item.feedDate);

    qInfo() << "[FeedService] loaded" << feed_.size() << "feed items";
    return true;
}

bool FeedService::save()
{
    return persistNow();
}

int FeedService::size() const
{
    return feed_.size();
}

qint64 FeedService::feedDate() const
{
    return feedDate_;
}

void FeedService::add(const FeedItem& item)
{
    if (shouldBlock(item.torrent)) {
        qInfo() << "[FeedService] blocking item" << item.torrent.hash.left(8)
                << "- type:" << domain::toString(item.torrent.contentType)
                << "category:" << domain::toString(item.torrent.contentCategory);
        return;
    }

    const auto existing = index_.constFind(item.torrent.hash);
    if (existing != index_.constEnd()) {
        // Refresh votes/seeders, keep the original feedDate.
        FeedItem& current = feed_[existing.value()];
        current.torrent.good = item.torrent.good;
        current.torrent.bad = item.torrent.bad;
        current.torrent.seeders = item.torrent.seeders;
    } else {
        FeedItem newItem = item;
        if (newItem.feedDate == 0)
            newItem.feedDate = QDateTime::currentSecsSinceEpoch();

        if (feed_.size() >= maxSize_) {
            // Drop the lowest-scored tail to make room, else replace the last.
            reorder();
            while (feed_.size() >= maxSize_ && !feed_.isEmpty()) {
                if (calculateScore(feed_.last()) <= 0) {
                    feed_.removeLast();
                } else {
                    feed_.last() = newItem;
                    break;
                }
            }
            if (feed_.size() < maxSize_)
                feed_.append(newItem);
        } else {
            feed_.append(newItem);
        }
    }

    reorder();
    feedDate_ = QDateTime::currentSecsSinceEpoch();

    markDirty();
    emit feedUpdated();
}

void FeedService::addByHash(const QString& hash)
{
    if (!torrents_ || hash.length() != 40)
        return;

    // With files for P2P replication (critical for proper sync).
    const auto torrent = torrents_->get(hash, /*includeFiles*/ true);
    if (!torrent || !torrent->isValid())
        return;

    if (shouldBlock(*torrent)) {
        qInfo() << "[FeedService] blocking torrent" << hash.left(8)
                << "- type:" << domain::toString(torrent->contentType)
                << "category:" << domain::toString(torrent->contentCategory);
        return;
    }

    FeedItem item;
    item.torrent = *torrent;
    add(item);
}

QVector<FeedItem> FeedService::getFeed(int index, int limit) const
{
    if (index < 0 || index >= feed_.size())
        return {};

    const int endIndex = qMin(index + limit, feed_.size());
    return feed_.mid(index, endIndex - index);
}

QJsonArray FeedService::toJsonArray(int index, int limit) const
{
    QJsonArray arr;
    const QVector<FeedItem> items = getFeed(index, limit);
    for (const FeedItem& item : items)
        arr.append(itemToJson(item));
    return arr;
}

void FeedService::fromJsonArray(const QJsonArray& array, qint64 remoteFeedDate)
{
    feed_.clear();

    for (const auto& value : array) {
        if (!value.isObject())
            continue;
        FeedItem item = itemFromJson(value.toObject());
        if (item.torrent.hash.isEmpty())
            continue;
        if (shouldBlock(item.torrent)) {
            qInfo() << "[FeedService] filtering blocked content from P2P:" << item.torrent.hash.left(8);
            continue;
        }
        feed_.append(item);
        if (feed_.size() >= maxSize_)
            break;
    }

    reorder();
    feedDate_ = remoteFeedDate > 0 ? remoteFeedDate : QDateTime::currentSecsSinceEpoch();

    markDirty();
    emit feedUpdated();
}

void FeedService::reorder()
{
    // Decorate-sort-undecorate: score each item once instead of recomputing the
    // sqrt-based Wilson score (and a currentSecsSinceEpoch() call) twice per
    // comparison. Scoring inside the comparator also risked breaking strict weak
    // ordering, since the time term could shift mid-sort.
    QVector<int> order(feed_.size());
    QVector<double> scores(feed_.size());
    for (int i = 0; i < feed_.size(); ++i) {
        order[i] = i;
        scores[i] = calculateScore(feed_[i]);
    }
    std::sort(order.begin(), order.end(), [&scores](int a, int b) { return scores[a] > scores[b]; });

    QVector<FeedItem> sorted;
    sorted.reserve(feed_.size());
    for (int i : order)
        sorted.append(std::move(feed_[i]));
    feed_ = std::move(sorted);

    rebuildIndex();
}

void FeedService::rebuildIndex()
{
    index_.clear();
    index_.reserve(feed_.size());
    for (int i = 0; i < feed_.size(); ++i)
        index_.insert(feed_[i].torrent.hash, i);
}

double FeedService::calculateScore(const FeedItem& item) const
{
    // Ranking algorithm from the legacy feed.js _compare function.
    const int good = item.torrent.good;
    const int bad = item.torrent.bad;

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 age = now - item.feedDate;

    const qint64 maxTime = 600000; // ~7 days in seconds
    if (age > maxTime)
        age = maxTime;

    const double relativeTime = static_cast<double>(maxTime - age) / maxTime;

    // Wilson score interval for the rating (95% confidence).
    auto wilsonScore = [](int positive, int negative) -> double {
        const int n = positive + negative;
        if (n == 0)
            return 0.0;

        const double z = 1.96;
        const double phat = static_cast<double>(positive) / n;
        const double denominator = 1 + z * z / n;
        const double numerator = phat + z * z / (2 * n) - z * std::sqrt((phat * (1 - phat) + z * z / (4 * n)) / n);

        return numerator / denominator;
    };

    return relativeTime * relativeTime + good * 1.5 * relativeTime - bad * 0.6 * relativeTime + wilsonScore(good, bad);
}

void FeedService::markDirty()
{
    dirty_ = true;
    if (!flushTimer_->isActive())
        flushTimer_->start();
}

void FeedService::flush()
{
    if (dirty_)
        persistNow();
}

bool FeedService::persistNow()
{
    dirty_ = false;
    flushTimer_->stop();
    if (!repository_)
        return false;
    return repository_->replaceAll(toStoredArray());
}

QJsonArray FeedService::toStoredArray() const
{
    QJsonArray arr;
    for (const FeedItem& item : feed_)
        arr.append(itemToJson(item));
    return arr;
}

} // namespace rats::service
