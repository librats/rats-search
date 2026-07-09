#include "searchresultmodel.h"
#include "domain/content.h"
#include "format.h"
#include <QDateTime>
#include <QHash>
#include <QPair>
#include <algorithm>

using rats::domain::SearchHit;
using rats::domain::Torrent;

SearchResultModel::SearchResultModel(QObject* parent) : QAbstractTableModel(parent) { }

SearchResultModel::~SearchResultModel() { }

int SearchResultModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return results_.size();
}

int SearchResultModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant SearchResultModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= results_.size()) {
        return QVariant();
    }

    const SearchHit& hit = results_[index.row()];
    const Torrent& torrent = hit.torrent;

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case NameColumn:
            return torrent.name;
        case SizeColumn:
            return rats::ui::formatSize(torrent.size);
        case SeedersColumn:
            return torrent.seeders;
        case LeechersColumn:
            return torrent.leechers;
        case DateColumn:
            return rats::ui::formatDate(torrent.added);
        default:
            return QVariant();
        }
    } else if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case SeedersColumn:
        case LeechersColumn:
            return Qt::AlignCenter;
        case SizeColumn:
            return Qt::AlignRight;
        default:
            return Qt::AlignLeft;
        }
    } else if (role == Qt::ToolTipRole) {
        return QString("Info Hash: %1\nSeeders: %2\nLeechers: %3\nSize: %4\nFiles: %5")
            .arg(torrent.hash)
            .arg(torrent.seeders)
            .arg(torrent.leechers)
            .arg(rats::ui::formatSize(torrent.size))
            .arg(torrent.files);
    }
    // Custom roles for delegate
    else if (role == ContentTypeRole) {
        // Content type as domain enum id (delegate converts back to ContentType)
        return rats::domain::toId(torrent.contentType);
    } else if (role == ContentCategoryRole) {
        // Content category as domain enum id
        return rats::domain::toId(torrent.contentCategory);
    } else if (role == GoodVotesRole) {
        // Good votes
        return torrent.good;
    } else if (role == BadVotesRole) {
        // Bad votes
        return torrent.bad;
    } else if (role == InfoHashRole) {
        // Info hash
        return torrent.hash;
    } else if (role == MatchingPathsRole) {
        // Highlighted file path snippets
        return hit.matchingPaths;
    } else if (role == IsFileMatchRole) {
        // Is this result from file search?
        return hit.fromFileMatch;
    } else if (role == FilesCountRole) {
        // Number of files in torrent
        return torrent.files;
    }

    return QVariant();
}

QVariant SearchResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }

    switch (section) {
    case NameColumn:
        return tr("Name");
    case SizeColumn:
        return tr("Size");
    case SeedersColumn:
        return tr("Seeders");
    case LeechersColumn:
        return tr("Leechers");
    case DateColumn:
        return tr("Date");
    default:
        return QVariant();
    }
}

void SearchResultModel::setResults(const QVector<SearchHit>& results)
{
    beginResetModel();
    results_ = results;
    endResetModel();
    applyCurrentSort();
}

void SearchResultModel::addResult(const SearchHit& result)
{
    // Check for duplicates by hash
    for (const SearchHit& existing : results_) {
        if (existing.torrent.hash == result.torrent.hash) {
            return; // Already exists
        }
    }

    beginInsertRows(QModelIndex(), results_.size(), results_.size());
    results_.append(result);
    endInsertRows();
    applyCurrentSort();
}

void SearchResultModel::clearResults()
{
    beginResetModel();
    results_.clear();
    endResetModel();
}

Torrent SearchResultModel::getTorrent(int row) const
{
    if (row >= 0 && row < results_.size()) {
        return results_[row].torrent;
    }
    return Torrent();
}

// =========================================================================
// File Search Results Methods
// =========================================================================

void SearchResultModel::addFileResult(const SearchHit& result)
{
    mergeFileResultIntoExisting(result);
}

void SearchResultModel::addFileResults(const QVector<SearchHit>& results)
{
    for (const SearchHit& result : results) {
        mergeFileResultIntoExisting(result);
    }
}

void SearchResultModel::mergeFileResultIntoExisting(const SearchHit& fileResult)
{
    // Check if this torrent already exists in results
    for (int i = 0; i < results_.size(); ++i) {
        if (results_[i].torrent.hash == fileResult.torrent.hash) {
            // Merge the matching paths into existing result
            for (const QString& path : fileResult.matchingPaths) {
                if (!results_[i].matchingPaths.contains(path)) {
                    results_[i].matchingPaths.append(path);
                }
            }
            // Keep fromFileMatch if either is a file match
            if (fileResult.fromFileMatch) {
                results_[i].fromFileMatch = true;
            }
            // Emit dataChanged for all columns of this row, including
            // Qt::SizeHintRole so the view recalculates the row height to accommodate
            // the file paths
            QModelIndex topLeft = index(i, 0);
            QModelIndex bottomRight = index(i, ColumnCount - 1);
            emit dataChanged(topLeft, bottomRight, { MatchingPathsRole, IsFileMatchRole, Qt::SizeHintRole });
            return;
        }
    }

    // Torrent doesn't exist, add as new result
    beginInsertRows(QModelIndex(), results_.size(), results_.size());
    results_.append(fileResult);
    endInsertRows();
    applyCurrentSort();
}

void SearchResultModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= ColumnCount) {
        return;
    }

    sortColumn_ = column;
    sortOrder_ = order;
    applyCurrentSort();
}

void SearchResultModel::applyCurrentSort()
{
    if (sortColumn_ < 0 || sortColumn_ >= ColumnCount || results_.isEmpty()) {
        return;
    }

    const int column = sortColumn_;
    const Qt::SortOrder order = sortOrder_;

    emit layoutAboutToBeChanged();

    // Capture persistent indexes by hash so we can remap them after sorting
    const QModelIndexList oldPersistent = persistentIndexList();
    QVector<QPair<QPersistentModelIndex, QString>> persistentByHash;
    persistentByHash.reserve(oldPersistent.size());
    for (const QModelIndex& idx : oldPersistent) {
        if (idx.row() >= 0 && idx.row() < results_.size()) {
            persistentByHash.append({ QPersistentModelIndex(idx), results_[idx.row()].torrent.hash });
        }
    }

    auto lessThan = [column](const SearchHit& a, const SearchHit& b) {
        switch (column) {
        case NameColumn:
            return a.torrent.name.localeAwareCompare(b.torrent.name) < 0;
        case SizeColumn:
            return a.torrent.size < b.torrent.size;
        case SeedersColumn:
            return a.torrent.seeders < b.torrent.seeders;
        case LeechersColumn:
            return a.torrent.leechers < b.torrent.leechers;
        case DateColumn:
            return a.torrent.added < b.torrent.added;
        }
        return false;
    };

    if (order == Qt::AscendingOrder) {
        std::stable_sort(results_.begin(), results_.end(), lessThan);
    } else {
        std::stable_sort(results_.begin(), results_.end(),
            [&lessThan](const SearchHit& a, const SearchHit& b) { return lessThan(b, a); });
    }

    // Rebuild a hash -> new row map to remap persistent indexes
    QHash<QString, int> rowByHash;
    rowByHash.reserve(results_.size());
    for (int i = 0; i < results_.size(); ++i) {
        rowByHash.insert(results_[i].torrent.hash, i);
    }
    for (const auto& pair : persistentByHash) {
        const int newRow = rowByHash.value(pair.second, -1);
        if (newRow >= 0) {
            changePersistentIndex(pair.first, index(newRow, pair.first.column()));
        } else {
            changePersistentIndex(pair.first, QModelIndex());
        }
    }

    emit layoutChanged();
}
