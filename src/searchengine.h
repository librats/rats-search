#ifndef SEARCHENGINE_H
#define SEARCHENGINE_H

#include <QObject>
#include <QString>
#include <QVector>
#include "torrentdatabase.h"

/**
 * @brief SearchEngine - Search and ranking engine for torrents
 */
class SearchEngine : public QObject
{
    Q_OBJECT

public:
    explicit SearchEngine(TorrentDatabase *database, QObject *parent = nullptr);
    ~SearchEngine();

    // Search operations
    QVector<TorrentInfo> search(const QString& query, int maxResults = 100);
    QVector<TorrentInfo> searchByCategory(const QString& category, int maxResults = 100);
    QVector<TorrentInfo> searchBySize(qint64 minSize, qint64 maxSize, int maxResults = 100);
    
    // Ranking and sorting
    QVector<TorrentInfo> getTopRanked(int limit = 50);
    QVector<TorrentInfo> getRecent(int limit = 50);
    
signals:
    void searchCompleted(int resultsCount);
    void searchFailed(const QString& error);

private:
    double calculateRelevanceScore(const TorrentInfo& torrent, const QString& query);
    void sortByRelevance(QVector<TorrentInfo>& results, const QString& query);
    
    TorrentDatabase *database_;
};

#endif // SEARCHENGINE_H

