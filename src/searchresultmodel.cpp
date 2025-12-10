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
        return QString("Info Hash: %1\nSeeders: %2\nLeechers: %3\nSize: %4")
            .arg(torrent.hash)
            .arg(torrent.seeders)
            .arg(torrent.leechers)
            .arg(formatSize(torrent.size));
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

