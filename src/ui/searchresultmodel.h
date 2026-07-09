#ifndef SEARCHRESULTMODEL_H
#define SEARCHRESULTMODEL_H

#include "domain/torrent.h"
#include <QAbstractTableModel>
#include <QVector>

/**
 * @brief SearchResultModel - Table model for displaying search results
 * Supports both torrent search results and file search results with highlighted
 * paths. Rows are stored as rats::domain::SearchHit (torrent + how it was
 * matched).
 */
class SearchResultModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { NameColumn = 0, SizeColumn, SeedersColumn, LeechersColumn, DateColumn, ColumnCount };

    // Custom data roles
    enum DataRole {
        ContentTypeRole = Qt::UserRole + 1,
        ContentCategoryRole = Qt::UserRole + 2,
        GoodVotesRole = Qt::UserRole + 3,
        BadVotesRole = Qt::UserRole + 4,
        InfoHashRole = Qt::UserRole + 5,
        MatchingPathsRole = Qt::UserRole + 6, // QStringList of highlighted file paths
        IsFileMatchRole = Qt::UserRole + 7, // bool - true if from file search
        FilesCountRole = Qt::UserRole + 8 // Number of files
    };

    explicit SearchResultModel(QObject* parent = nullptr);
    ~SearchResultModel();

    // QAbstractTableModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    // Custom methods for torrent results
    void setResults(const QVector<rats::domain::SearchHit>& results);
    void addResult(const rats::domain::SearchHit& result);
    void clearResults();

    // File search results methods
    void addFileResult(const rats::domain::SearchHit& result);
    void addFileResults(const QVector<rats::domain::SearchHit>& results);

    // Access methods
    rats::domain::Torrent getTorrent(int row) const;
    int resultCount() const { return results_.size(); }

private:
    void mergeFileResultIntoExisting(const rats::domain::SearchHit& fileResult);
    void applyCurrentSort();

    QVector<rats::domain::SearchHit> results_;
    int sortColumn_ = -1;
    Qt::SortOrder sortOrder_ = Qt::DescendingOrder;
};

#endif // SEARCHRESULTMODEL_H
