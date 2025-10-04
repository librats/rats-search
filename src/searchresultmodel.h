#ifndef SEARCHRESULTMODEL_H
#define SEARCHRESULTMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include "torrentdatabase.h"

/**
 * @brief SearchResultModel - Table model for displaying search results
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

    explicit SearchResultModel(QObject *parent = nullptr);
    ~SearchResultModel();

    // QAbstractTableModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    // Custom methods
    void setResults(const QVector<TorrentInfo> &results);
    void clearResults();
    TorrentInfo getTorrent(int row) const;
    QString getInfoHash(int row) const;

private:
    QString formatSize(qint64 bytes) const;
    QString formatDate(const QDateTime &dateTime) const;
    
    QVector<TorrentInfo> results_;
};

#endif // SEARCHRESULTMODEL_H

