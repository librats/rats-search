#include "searchengine.h"
#include <QDebug>
#include <algorithm>

SearchEngine::SearchEngine(TorrentDatabase *database, QObject *parent)
    : QObject(parent)
    , database_(database)
{
}

SearchEngine::~SearchEngine()
{
}

QVector<TorrentInfo> SearchEngine::search(const QString& query, int maxResults)
{
    if (!database_ || query.isEmpty()) {
        return QVector<TorrentInfo>();
    }
    
    // Create search options
    SearchOptions options;
    options.query = query;
    options.limit = maxResults;
    
    // Search in database
    QVector<TorrentInfo> results = database_->searchTorrents(options);
    
    // Sort by relevance and seeders
    sortByRelevance(results, query);
    
    emit searchCompleted(results.size());
    return results;
}

QVector<TorrentInfo> SearchEngine::searchByCategory(const QString& category, int maxResults)
{
    Q_UNUSED(category);
    Q_UNUSED(maxResults);
    // TODO: Implement category search
    return QVector<TorrentInfo>();
}

QVector<TorrentInfo> SearchEngine::searchBySize(qint64 minSize, qint64 maxSize, int maxResults)
{
    Q_UNUSED(minSize);
    Q_UNUSED(maxSize);
    Q_UNUSED(maxResults);
    // TODO: Implement size-based search
    return QVector<TorrentInfo>();
}

QVector<TorrentInfo> SearchEngine::getTopRanked(int limit)
{
    if (!database_) {
        return QVector<TorrentInfo>();
    }
    
    return database_->getTopTorrents("", "", 0, limit);
}

QVector<TorrentInfo> SearchEngine::getRecent(int limit)
{
    if (!database_) {
        return QVector<TorrentInfo>();
    }
    
    return database_->getRecentTorrents(limit);
}

double SearchEngine::calculateRelevanceScore(const TorrentInfo& torrent, const QString& query)
{
    double score = 0.0;
    
    QString lowerName = torrent.name.toLower();
    QString lowerQuery = query.toLower();
    
    // Exact match bonus
    if (lowerName == lowerQuery) {
        score += 100.0;
    }
    // Starts with query bonus
    else if (lowerName.startsWith(lowerQuery)) {
        score += 50.0;
    }
    // Contains query bonus
    else if (lowerName.contains(lowerQuery)) {
        score += 25.0;
    }
    
    // Seeder boost (logarithmic to avoid overwhelming other factors)
    if (torrent.seeders > 0) {
        score += std::log10(torrent.seeders + 1) * 10.0;
    }
    
    // Recency boost (newer torrents get slight boost)
    qint64 daysSinceAdded = torrent.added.daysTo(QDateTime::currentDateTime());
    if (daysSinceAdded < 7 && daysSinceAdded >= 0) {
        score += (7 - daysSinceAdded) * 2.0;
    }
    
    return score;
}

void SearchEngine::sortByRelevance(QVector<TorrentInfo>& results, const QString& query)
{
    std::sort(results.begin(), results.end(), 
        [this, &query](const TorrentInfo& a, const TorrentInfo& b) {
            double scoreA = calculateRelevanceScore(a, query);
            double scoreB = calculateRelevanceScore(b, query);
            return scoreA > scoreB;
        });
}

