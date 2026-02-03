#include "feedmanager.h"
#include "../torrentdatabase.h"
#include "../sphinxql.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <cmath>

// ============================================================================
// Content Filtering Helper
// ============================================================================

// Check if content should be blocked from feed (bad or xxx content)
static bool shouldBlockContent(const QString& contentType, const QString& contentCategory)
{
    return contentType.toLower() == "bad" || contentCategory.toLower() == "xxx";
}

// ============================================================================
// TorrentFeedFile
// ============================================================================

QJsonObject TorrentFeedFile::toJson() const
{
    QJsonObject obj;
    obj["path"] = path;
    obj["size"] = size;
    return obj;
}

TorrentFeedFile TorrentFeedFile::fromJson(const QJsonObject& obj)
{
    TorrentFeedFile file;
    file.path = obj["path"].toString();
    file.size = obj["size"].toVariant().toLongLong();
    return file;
}

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
    
    // Include files list for P2P replication (critical for proper sync)
    if (!filesList.isEmpty()) {
        QJsonArray filesArray;
        for (const TorrentFeedFile& file : filesList) {
            filesArray.append(file.toJson());
        }
        obj["filesList"] = filesArray;
    }
    
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
    
    // Parse files list for P2P replication
    if (obj.contains("filesList")) {
        QJsonArray filesArray = obj["filesList"].toArray();
        for (const QJsonValue& fileVal : filesArray) {
            if (fileVal.isObject()) {
                item.filesList.append(TorrentFeedFile::fromJson(fileVal.toObject()));
            }
        }
    }
    
    // Update files count if not set but we have filesList
    if (item.files == 0 && !item.filesList.isEmpty()) {
        item.files = item.filesList.size();
    }
    
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
    if (!database_ || !database_->isReady()) {
        qWarning() << "FeedManager: Database not ready";
        loaded_ = true;
        return false;
    }
    
    SphinxQL* sphinxQL = database_->sphinxQL();
    if (!sphinxQL) {
        qWarning() << "FeedManager: SphinxQL not available";
        loaded_ = true;
        return false;
    }
    
    // Feed table is defined in sphinx.conf (manticoremanager.cpp generateConfig)
    // Schema: id (auto), feedIndex (rt_field), data (rt_attr_json)
    // Data is stored as JSON, like legacy feed.js
    
    // Load feed items - data is stored as JSON in the 'data' column
    QString querySql = QString("SELECT * FROM feed LIMIT %1").arg(maxSize_);
    SphinxQL::Results rows = sphinxQL->query(querySql);
    
    feed_.clear();
    
    for (const QVariantMap& row : rows) {
        // Parse JSON data from the 'data' column (like legacy: JSON.parse(f.data))
        QString jsonData = row["data"].toString();
        if (jsonData.isEmpty()) {
            continue;
        }
        
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning() << "FeedManager: Failed to parse feed item JSON:" << parseError.errorString();
            continue;
        }
        
        TorrentFeedItem item = TorrentFeedItem::fromJson(doc.object());
        if (!item.hash.isEmpty()) {
            // Skip bad/xxx content during load (filter existing data)
            if (shouldBlockContent(item.contentType, item.contentCategory)) {
                qInfo() << "FeedManager: Filtering blocked content on load:" << item.hash.left(8);
                continue;
            }
            feed_.append(item);
        }
    }
    
    // Sort by ranking score (like legacy _order)
    reorder();
    
    // Get the latest feedDate as our feed date
    if (!feed_.isEmpty()) {
        for (const TorrentFeedItem& item : feed_) {
            if (item.feedDate > feedDate_) {
                feedDate_ = item.feedDate;
            }
        }
    }
    
    loaded_ = true;
    
    qInfo() << "FeedManager: Loaded" << feed_.size() << "feed items from database";
    return true;
}

bool FeedManager::save()
{
    if (!database_ || !database_->isReady()) {
        return false;
    }
    
    SphinxQL* sphinxQL = database_->sphinxQL();
    if (!sphinxQL) {
        return false;
    }
    
    // Clear and repopulate the feed table
    // (Manticore doesn't support TRUNCATE well, so delete all)
    // Like legacy: await this.sphinx.query('delete from feed where id > 0')
    if (!sphinxQL->exec("DELETE FROM feed WHERE id > 0")) {
        qWarning() << "FeedManager: Failed to clear feed table:" << sphinxQL->lastError();
        // Continue anyway - may fail if table is empty
    }
    
    // Insert all items with data stored as JSON (like legacy feed.js)
    // Legacy: 'insert into feed(id, data) values(?, ?)', [++id, JSON.stringify(record)]
    qint64 nextId = 1;
    for (const TorrentFeedItem& item : feed_) {
        // Serialize item to JSON
        QJsonObject jsonObj = item.toJson();
        QString jsonData = QString::fromUtf8(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));
        
        QVariantMap values;
        values["id"] = nextId++;
        values["data"] = jsonData;
        
        if (!sphinxQL->insertValues("feed", values)) {
            qWarning() << "FeedManager: Failed to save feed item:" << item.hash << sphinxQL->lastError();
        }
    }
    
    qInfo() << "FeedManager: Saved" << feed_.size() << "feed items to database";
    return true;
}

void FeedManager::clear()
{
    feed_.clear();
    feedDate_ = 0;
    
    // Also clear from database
    if (database_ && database_->sphinxQL()) {
        database_->sphinxQL()->exec("DELETE FROM feed WHERE id > 0");
    }
    
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
    // Block bad/xxx content from being added to feed
    if (shouldBlockContent(item.contentType, item.contentCategory)) {
        qInfo() << "FeedManager: Blocking item" << item.hash.left(8) 
                << "- content type:" << item.contentType 
                << "category:" << item.contentCategory;
        return;
    }
    
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
    
    // Auto-save periodically (every 10 additions would be good, but for now save immediately)
    // In a real app you'd want to debounce this
    save();
}

void FeedManager::addByHash(const QString& hash)
{
    if (!database_ || hash.length() != 40) {
        return;
    }
    
    // Get torrent WITH files included for P2P replication
    TorrentInfo torrent = database_->getTorrent(hash, true);
    if (!torrent.isValid()) {
        return;
    }
    
    // Block bad/xxx content from being added to feed
    if (shouldBlockContent(torrent.contentTypeString(), torrent.contentCategoryString())) {
        qInfo() << "FeedManager: Blocking torrent" << hash.left(8) 
                << "- content type:" << torrent.contentTypeString() 
                << "category:" << torrent.contentCategoryString();
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
    
    // Include files for P2P replication (critical for proper sync)
    for (const TorrentFile& f : torrent.filesList) {
        TorrentFeedFile feedFile;
        feedFile.path = f.path;
        feedFile.size = f.size;
        item.filesList.append(feedFile);
    }
    
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
                // Skip bad/xxx content during P2P sync
                if (shouldBlockContent(item.contentType, item.contentCategory)) {
                    qInfo() << "FeedManager: Filtering blocked content from P2P:" << item.hash.left(8);
                    continue;
                }
                feed_.append(item);
            }
        }
        
        if (feed_.size() >= maxSize_) {
            break;
        }
    }
    
    reorder();
    feedDate_ = remoteFeedDate > 0 ? remoteFeedDate : QDateTime::currentSecsSinceEpoch();
    
    // Save the new feed to database
    save();
    
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
    // Ranking algorithm from legacy feed.js _compare function
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
