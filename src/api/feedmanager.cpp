#include "feedmanager.h"
#include "../torrentdatabase.h"
#include <QDateTime>
#include <cmath>

// ============================================================================
// TorrentFeedItem
// ============================================================================

QJsonObject TorrentFeedItem::toJson() const
{
    QJsonObject obj;
    obj["hash"] = hash;
    obj["name"] = name;
    obj["size"] = size;
    obj["files"] = files;
    obj["contentType"] = contentType;
    obj["contentCategory"] = contentCategory;
    obj["seeders"] = seeders;
    obj["good"] = good;
    obj["bad"] = bad;
    obj["feedDate"] = feedDate;
    return obj;
}

TorrentFeedItem TorrentFeedItem::fromJson(const QJsonObject& obj)
{
    TorrentFeedItem item;
    item.hash = obj["hash"].toString();
    item.name = obj["name"].toString();
    item.size = obj["size"].toVariant().toLongLong();
    item.files = obj["files"].toInt();
    item.contentType = obj["contentType"].toString();
    item.contentCategory = obj["contentCategory"].toString();
    item.seeders = obj["seeders"].toInt();
    item.good = obj["good"].toInt();
    item.bad = obj["bad"].toInt();
    item.feedDate = obj["feedDate"].toVariant().toLongLong();
    return item;
}

// ============================================================================
// FeedManager
// ============================================================================

FeedManager::FeedManager(TorrentDatabase* database, QObject *parent)
    : QObject(parent)
    , database_(database)
{
}

FeedManager::~FeedManager()
{
    if (loaded_) {
        save();
    }
}

bool FeedManager::load()
{
    if (!database_) {
        loaded_ = true;
        return false;
    }
    
    // TODO: Load from database feed table
    // For now, start with empty feed
    feed_.clear();
    loaded_ = true;
    
    return true;
}

bool FeedManager::save()
{
    if (!database_ || !loaded_) {
        return false;
    }
    
    // TODO: Save to database feed table
    
    return true;
}

void FeedManager::clear()
{
    feed_.clear();
    feedDate_ = 0;
    emit feedUpdated();
}

int FeedManager::size() const
{
    return feed_.size();
}

int FeedManager::maxSize() const
{
    return maxSize_;
}

void FeedManager::setMaxSize(int max)
{
    maxSize_ = max;
    
    // Trim if necessary
    while (feed_.size() > maxSize_) {
        feed_.removeLast();
    }
}

qint64 FeedManager::feedDate() const
{
    return feedDate_;
}

void FeedManager::add(const TorrentFeedItem& item)
{
    // Check if already exists
    int existingIndex = -1;
    for (int i = 0; i < feed_.size(); ++i) {
        if (feed_[i].hash == item.hash) {
            existingIndex = i;
            break;
        }
    }
    
    if (existingIndex >= 0) {
        // Update existing item
        TorrentFeedItem& existing = feed_[existingIndex];
        existing.good = item.good;
        existing.bad = item.bad;
        existing.seeders = item.seeders;
        // Keep original feedDate
    } else {
        // Add new item
        TorrentFeedItem newItem = item;
        if (newItem.feedDate == 0) {
            newItem.feedDate = QDateTime::currentSecsSinceEpoch();
        }
        
        if (feed_.size() >= maxSize_) {
            // Remove lowest scored items until we have space
            reorder();
            while (feed_.size() >= maxSize_ && !feed_.isEmpty()) {
                if (calculateScore(feed_.last()) <= 0) {
                    feed_.removeLast();
                } else {
                    // Replace the last one
                    feed_.last() = newItem;
                    break;
                }
            }
            
            if (feed_.size() < maxSize_) {
                feed_.append(newItem);
            }
        } else {
            feed_.append(newItem);
        }
    }
    
    reorder();
    feedDate_ = QDateTime::currentSecsSinceEpoch();
    
    emit itemAdded(item.hash);
    emit feedUpdated();
}

void FeedManager::addByHash(const QString& hash)
{
    if (!database_ || hash.length() != 40) {
        return;
    }
    
    TorrentInfo torrent = database_->getTorrent(hash);
    if (!torrent.isValid()) {
        return;
    }
    
    TorrentFeedItem item;
    item.hash = torrent.hash;
    item.name = torrent.name;
    item.size = torrent.size;
    item.files = torrent.files;
    item.contentType = torrent.contentTypeString();
    item.contentCategory = torrent.contentCategoryString();
    item.seeders = torrent.seeders;
    item.good = torrent.good;
    item.bad = torrent.bad;
    
    add(item);
}

QVector<TorrentFeedItem> FeedManager::getFeed(int index, int limit) const
{
    if (index < 0 || index >= feed_.size()) {
        return {};
    }
    
    int endIndex = qMin(index + limit, feed_.size());
    return feed_.mid(index, endIndex - index);
}

QVector<TorrentFeedItem> FeedManager::allItems() const
{
    return feed_;
}

QJsonArray FeedManager::toJsonArray(int index, int limit) const
{
    QJsonArray arr;
    QVector<TorrentFeedItem> items = getFeed(index, limit);
    for (const TorrentFeedItem& item : items) {
        arr.append(item.toJson());
    }
    return arr;
}

void FeedManager::fromJsonArray(const QJsonArray& array, qint64 remoteFeedDate)
{
    feed_.clear();
    
    for (const QJsonValue& val : array) {
        if (val.isObject()) {
            TorrentFeedItem item = TorrentFeedItem::fromJson(val.toObject());
            if (!item.hash.isEmpty()) {
                feed_.append(item);
            }
        }
        
        if (feed_.size() >= maxSize_) {
            break;
        }
    }
    
    reorder();
    feedDate_ = remoteFeedDate > 0 ? remoteFeedDate : QDateTime::currentSecsSinceEpoch();
    
    emit feedUpdated();
}

bool FeedManager::contains(const QString& hash) const
{
    for (const TorrentFeedItem& item : feed_) {
        if (item.hash == hash) {
            return true;
        }
    }
    return false;
}

TorrentFeedItem FeedManager::getItem(const QString& hash) const
{
    for (const TorrentFeedItem& item : feed_) {
        if (item.hash == hash) {
            return item;
        }
    }
    return TorrentFeedItem();
}

void FeedManager::reorder()
{
    std::sort(feed_.begin(), feed_.end(), [this](const TorrentFeedItem& a, const TorrentFeedItem& b) {
        return calculateScore(a) > calculateScore(b);
    });
}

double FeedManager::calculateScore(const TorrentFeedItem& item) const
{
    // Ranking algorithm from legacy code
    const int good = item.good;
    const int bad = item.bad;
    const int comments = 0;  // TODO: Add comments support
    
    qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 age = now - item.feedDate;
    
    const qint64 maxTime = 600000;  // ~7 days in seconds
    if (age > maxTime) {
        age = maxTime;
    }
    
    double relativeTime = static_cast<double>(maxTime - age) / maxTime;
    
    // Wilson score interval for rating
    auto wilsonScore = [](int positive, int negative) -> double {
        int n = positive + negative;
        if (n == 0) return 0.0;
        
        double z = 1.96;  // 95% confidence
        double phat = static_cast<double>(positive) / n;
        double denominator = 1 + z * z / n;
        double numerator = phat + z * z / (2 * n) 
                          - z * std::sqrt((phat * (1 - phat) + z * z / (4 * n)) / n);
        
        return numerator / denominator;
    };
    
    return relativeTime * relativeTime
           + good * 1.5 * relativeTime
           + comments * 4 * relativeTime
           - bad * 0.6 * relativeTime
           + wilsonScore(good, bad);
}

