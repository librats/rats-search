#ifndef SEARCHRESULTMODEL_H
#define SEARCHRESULTMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include "torrentdatabase.h"

/**
 * @brief SearchResultModel - Table model for displaying search results
 * Supports both torrent search results and file search results with highlighted paths
 */
class SearchResultModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        NameColumn = 0,
        SizeColumn,
        SeedersColumn,
        LeechersColumn,
        DateColumn,
        ColumnCount
    };
    
    // Custom data roles
    enum DataRole {
        ContentTypeRole = Qt::UserRole + 1,
        ContentCategoryRole = Qt::UserRole + 2,
        GoodVotesRole = Qt::UserRole + 3,
        BadVotesRole = Qt::UserRole + 4,
        InfoHashRole = Qt::UserRole + 5,
        MatchingPathsRole = Qt::UserRole + 6,  // QStringList of highlighted file paths
        IsFileMatchRole = Qt::UserRole + 7,     // bool - true if from file search
        FilesCountRole = Qt::UserRole + 8       // Number of files
    };

    explicit SearchResultModel(QObject *parent = nullptr);
    ~SearchResultModel();

    // QAbstractTableModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    // Custom methods for torrent results
    void setResults(const QVector<TorrentInfo> &results);
    void addResult(const TorrentInfo &result);
    void addResults(const QVector<TorrentInfo> &results);
    void clearResults();
    
    // File search results methods
    void setFileResults(const QVector<TorrentInfo> &results);
    void addFileResult(const TorrentInfo &result);
    void addFileResults(const QVector<TorrentInfo> &results);
    void clearFileResults();
    
    // Access methods
    TorrentInfo getTorrent(int row) const;
    QString getInfoHash(int row) const;
    int resultCount() const { return results_.size(); }
    int torrentResultCount() const;
    int fileResultCount() const;

private:
    QString formatSize(qint64 bytes) const;
    QString formatDate(const QDateTime &dateTime) const;
    void mergeFileResultIntoExisting(const TorrentInfo &fileResult);
    
    QVector<TorrentInfo> results_;
};

#endif // SEARCHRESULTMODEL_H

