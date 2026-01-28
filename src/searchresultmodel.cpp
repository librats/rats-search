#include "searchresultmodel.h"
#include <QDateTime>

SearchResultModel::SearchResultModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

SearchResultModel::~SearchResultModel()
{
}

int SearchResultModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return results_.size();
}

int SearchResultModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant SearchResultModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= results_.size()) {
        return QVariant();
    }
    
    const TorrentInfo &torrent = results_[index.row()];
    
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case NameColumn:
                return torrent.name;
            case SizeColumn:
                return formatSize(torrent.size);
            case SeedersColumn:
                return torrent.seeders;
            case LeechersColumn:
                return torrent.leechers;
            case DateColumn:
                return formatDate(torrent.added);
            default:
                return QVariant();
        }
    }
    else if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
            case SeedersColumn:
            case LeechersColumn:
                return Qt::AlignCenter;
            case SizeColumn:
                return Qt::AlignRight;
            default:
                return Qt::AlignLeft;
        }
    }
    else if (role == Qt::ToolTipRole) {
        return QString("Info Hash: %1\nSeeders: %2\nLeechers: %3\nSize: %4\nFiles: %5")
            .arg(torrent.hash)
            .arg(torrent.seeders)
            .arg(torrent.leechers)
            .arg(formatSize(torrent.size))
            .arg(torrent.files);
    }
    // Custom roles for delegate
    else if (role == Qt::UserRole + 1) {
        // Content type string for icon coloring
        return torrent.contentType;
    }
    else if (role == Qt::UserRole + 2) {
        // Content category string
        return torrent.contentCategory;
    }
    else if (role == Qt::UserRole + 3) {
        // Good votes
        return torrent.good;
    }
    else if (role == Qt::UserRole + 4) {
        // Bad votes
        return torrent.bad;
    }
    else if (role == Qt::UserRole + 5 || role == InfoHashRole) {
        // Info hash
        return torrent.hash;
    }
    else if (role == MatchingPathsRole) {
        // Highlighted file path snippets
        return torrent.matchingPaths;
    }
    else if (role == IsFileMatchRole) {
        // Is this result from file search?
        return torrent.isFileMatch;
    }
    else if (role == FilesCountRole) {
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

void SearchResultModel::setResults(const QVector<TorrentInfo> &results)
{
    beginResetModel();
    results_ = results;
    endResetModel();
}

void SearchResultModel::addResult(const TorrentInfo &result)
{
    // Check for duplicates by hash
    for (const TorrentInfo& existing : results_) {
        if (existing.hash == result.hash) {
            return;  // Already exists
        }
    }
    
    beginInsertRows(QModelIndex(), results_.size(), results_.size());
    results_.append(result);
    endInsertRows();
}

void SearchResultModel::addResults(const QVector<TorrentInfo> &results)
{
    QVector<TorrentInfo> newResults;
    
    // Filter out duplicates
    for (const TorrentInfo& result : results) {
        bool exists = false;
        for (const TorrentInfo& existing : results_) {
            if (existing.hash == result.hash) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            newResults.append(result);
        }
    }
    
    if (newResults.isEmpty()) {
        return;
    }
    
    beginInsertRows(QModelIndex(), results_.size(), results_.size() + newResults.size() - 1);
    results_.append(newResults);
    endInsertRows();
}

void SearchResultModel::clearResults()
{
    beginResetModel();
    results_.clear();
    endResetModel();
}

TorrentInfo SearchResultModel::getTorrent(int row) const
{
    if (row >= 0 && row < results_.size()) {
        return results_[row];
    }
    return TorrentInfo();
}

QString SearchResultModel::getInfoHash(int row) const
{
    if (row >= 0 && row < results_.size()) {
        return results_[row].hash;
    }
    return QString();
}

QString SearchResultModel::formatSize(qint64 bytes) const
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    const qint64 TB = GB * 1024;
    
    if (bytes >= TB) {
        return QString::number(bytes / static_cast<double>(TB), 'f', 2) + " TB";
    } else if (bytes >= GB) {
        return QString::number(bytes / static_cast<double>(GB), 'f', 2) + " GB";
    } else if (bytes >= MB) {
        return QString::number(bytes / static_cast<double>(MB), 'f', 2) + " MB";
    } else if (bytes >= KB) {
        return QString::number(bytes / static_cast<double>(KB), 'f', 2) + " KB";
    } else {
        return QString::number(bytes) + " B";
    }
}

QString SearchResultModel::formatDate(const QDateTime &dateTime) const
{
    if (!dateTime.isValid()) {
        return tr("Unknown");
    }
    
    QDateTime now = QDateTime::currentDateTime();
    qint64 daysDiff = dateTime.daysTo(now);
    
    if (daysDiff == 0) {
        return tr("Today");
    } else if (daysDiff == 1) {
        return tr("Yesterday");
    } else if (daysDiff < 7) {
        return tr("%1 days ago").arg(daysDiff);
    } else {
        return dateTime.toString("yyyy-MM-dd");
    }
}

// =========================================================================
// File Search Results Methods
// =========================================================================

void SearchResultModel::setFileResults(const QVector<TorrentInfo> &results)
{
    // File results are merged with existing torrent results
    // If a torrent already exists, merge the matching paths
    for (const TorrentInfo& fileResult : results) {
        mergeFileResultIntoExisting(fileResult);
    }
}

void SearchResultModel::addFileResult(const TorrentInfo &result)
{
    mergeFileResultIntoExisting(result);
}

void SearchResultModel::addFileResults(const QVector<TorrentInfo> &results)
{
    for (const TorrentInfo& result : results) {
        mergeFileResultIntoExisting(result);
    }
}

void SearchResultModel::clearFileResults()
{
    // Remove file-only results and clear matchingPaths from torrent results
    beginResetModel();
    
    QVector<TorrentInfo> torrentOnlyResults;
    for (TorrentInfo& torrent : results_) {
        if (!torrent.isFileMatch) {
            // Keep torrent search results, but clear any merged file paths
            torrent.matchingPaths.clear();
            torrentOnlyResults.append(torrent);
        }
        // File-only results are discarded
    }
    
    results_ = torrentOnlyResults;
    endResetModel();
}

void SearchResultModel::mergeFileResultIntoExisting(const TorrentInfo &fileResult)
{
    // Check if this torrent already exists in results
    for (int i = 0; i < results_.size(); ++i) {
        if (results_[i].hash == fileResult.hash) {
            // Merge the matching paths into existing result
            for (const QString& path : fileResult.matchingPaths) {
                if (!results_[i].matchingPaths.contains(path)) {
                    results_[i].matchingPaths.append(path);
                }
            }
            // Keep isFileMatch if either is a file match
            if (fileResult.isFileMatch) {
                results_[i].isFileMatch = true;
            }
            // Emit dataChanged for this row
            QModelIndex idx = index(i, NameColumn);
            emit dataChanged(idx, idx, {MatchingPathsRole, IsFileMatchRole});
            return;
        }
    }
    
    // Torrent doesn't exist, add as new result
    beginInsertRows(QModelIndex(), results_.size(), results_.size());
    results_.append(fileResult);
    endInsertRows();
}

int SearchResultModel::torrentResultCount() const
{
    int count = 0;
    for (const TorrentInfo& result : results_) {
        if (!result.isFileMatch || !result.matchingPaths.isEmpty()) {
            // Count if it's a torrent match or has matching files
            if (!result.isFileMatch) {
                count++;
            }
        }
    }
    return count;
}

int SearchResultModel::fileResultCount() const
{
    int count = 0;
    for (const TorrentInfo& result : results_) {
        if (result.isFileMatch || !result.matchingPaths.isEmpty()) {
            count++;
        }
    }
    return count;
}
