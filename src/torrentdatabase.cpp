#include "torrentdatabase.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QVariant>

TorrentDatabase::TorrentDatabase(const QString& dataDirectory, QObject *parent)
    : QObject(parent)
    , dataDirectory_(dataDirectory)
{
}

TorrentDatabase::~TorrentDatabase()
{
    close();
}

bool TorrentDatabase::initialize()
{
    QString dbPath = getDatabasePath();
    
    qInfo() << "Initializing database at:" << dbPath;
    
    // Create directory if it doesn't exist
    QDir dir(dataDirectory_);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qCritical() << "Failed to create data directory:" << dataDirectory_;
            return false;
        }
    }
    
    // Open database
    db_ = QSqlDatabase::addDatabase("QSQLITE");
    db_.setDatabaseName(dbPath);
    
    if (!db_.open()) {
        qCritical() << "Failed to open database:" << db_.lastError().text();
        emit databaseError(db_.lastError().text());
        return false;
    }
    
    // Create tables
    if (!createTables()) {
        qCritical() << "Failed to create database tables";
        return false;
    }
    
    qInfo() << "Database initialized successfully";
    qInfo() << "Total torrents:" << getTorrentCount();
    
    return true;
}

void TorrentDatabase::close()
{
    if (db_.isOpen()) {
        db_.close();
        qInfo() << "Database closed";
    }
}

bool TorrentDatabase::createTables()
{
    QSqlQuery query(db_);
    
    // Create torrents table
    QString createTorrentsTable = R"(
        CREATE TABLE IF NOT EXISTS torrents (
            info_hash TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            size INTEGER DEFAULT 0,
            seeders INTEGER DEFAULT 0,
            leechers INTEGER DEFAULT 0,
            indexed_date DATETIME DEFAULT CURRENT_TIMESTAMP,
            category TEXT,
            description TEXT
        )
    )";
    
    if (!query.exec(createTorrentsTable)) {
        qCritical() << "Failed to create torrents table:" << query.lastError().text();
        return false;
    }
    
    // Create files table for torrent files
    QString createFilesTable = R"(
        CREATE TABLE IF NOT EXISTS torrent_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            info_hash TEXT NOT NULL,
            file_path TEXT NOT NULL,
            file_size INTEGER DEFAULT 0,
            FOREIGN KEY(info_hash) REFERENCES torrents(info_hash) ON DELETE CASCADE
        )
    )";
    
    if (!query.exec(createFilesTable)) {
        qCritical() << "Failed to create files table:" << query.lastError().text();
        return false;
    }
    
    // Create indices for better search performance
    query.exec("CREATE INDEX IF NOT EXISTS idx_torrent_name ON torrents(name)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_torrent_date ON torrents(indexed_date)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_torrent_seeders ON torrents(seeders)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_file_hash ON torrent_files(info_hash)");
    
    // Create FTS (Full Text Search) virtual table
    QString createFtsTable = R"(
        CREATE VIRTUAL TABLE IF NOT EXISTS torrents_fts USING fts5(
            info_hash UNINDEXED,
            name,
            description,
            content=torrents,
            content_rowid=rowid
        )
    )";
    
    if (!query.exec(createFtsTable)) {
        qWarning() << "Failed to create FTS table (FTS may not be available):" << query.lastError().text();
        // FTS is optional, continue anyway
    }
    
    return true;
}

bool TorrentDatabase::addTorrent(const TorrentInfo& torrent)
{
    QSqlQuery query(db_);
    query.prepare(R"(
        INSERT OR REPLACE INTO torrents 
        (info_hash, name, size, seeders, leechers, category, description, indexed_date)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");
    
    query.addBindValue(torrent.infoHash);
    query.addBindValue(torrent.name);
    query.addBindValue(torrent.size);
    query.addBindValue(torrent.seeders);
    query.addBindValue(torrent.leechers);
    query.addBindValue(torrent.category);
    query.addBindValue(torrent.description);
    query.addBindValue(torrent.indexedDate);
    
    if (!query.exec()) {
        qWarning() << "Failed to add torrent:" << query.lastError().text();
        emit databaseError(query.lastError().text());
        return false;
    }
    
    // Add files if any
    if (!torrent.files.isEmpty()) {
        QSqlQuery fileQuery(db_);
        fileQuery.prepare("INSERT INTO torrent_files (info_hash, file_path) VALUES (?, ?)");
        
        for (const QString& file : torrent.files) {
            fileQuery.addBindValue(torrent.infoHash);
            fileQuery.addBindValue(file);
            fileQuery.exec();
        }
    }
    
    // Update FTS table
    QSqlQuery ftsQuery(db_);
    ftsQuery.prepare("INSERT OR REPLACE INTO torrents_fts (info_hash, name, description) VALUES (?, ?, ?)");
    ftsQuery.addBindValue(torrent.infoHash);
    ftsQuery.addBindValue(torrent.name);
    ftsQuery.addBindValue(torrent.description);
    ftsQuery.exec();  // Ignore errors if FTS not available
    
    emit torrentAdded(torrent.infoHash);
    return true;
}

bool TorrentDatabase::updateTorrent(const TorrentInfo& torrent)
{
    return addTorrent(torrent);  // Uses INSERT OR REPLACE
}

bool TorrentDatabase::removeTorrent(const QString& infoHash)
{
    QSqlQuery query(db_);
    query.prepare("DELETE FROM torrents WHERE info_hash = ?");
    query.addBindValue(infoHash);
    
    if (!query.exec()) {
        qWarning() << "Failed to remove torrent:" << query.lastError().text();
        return false;
    }
    
    emit torrentRemoved(infoHash);
    return true;
}

TorrentInfo TorrentDatabase::getTorrent(const QString& infoHash)
{
    TorrentInfo info;
    
    QSqlQuery query(db_);
    query.prepare(R"(
        SELECT info_hash, name, size, seeders, leechers, indexed_date, category, description
        FROM torrents WHERE info_hash = ?
    )");
    query.addBindValue(infoHash);
    
    if (query.exec() && query.next()) {
        info.infoHash = query.value(0).toString();
        info.name = query.value(1).toString();
        info.size = query.value(2).toLongLong();
        info.seeders = query.value(3).toInt();
        info.leechers = query.value(4).toInt();
        info.indexedDate = query.value(5).toDateTime();
        info.category = query.value(6).toString();
        info.description = query.value(7).toString();
        
        // Get files
        QSqlQuery fileQuery(db_);
        fileQuery.prepare("SELECT file_path FROM torrent_files WHERE info_hash = ?");
        fileQuery.addBindValue(infoHash);
        if (fileQuery.exec()) {
            while (fileQuery.next()) {
                info.files << fileQuery.value(0).toString();
            }
        }
    }
    
    return info;
}

QVector<TorrentInfo> TorrentDatabase::searchTorrents(const QString& query, int limit)
{
    QVector<TorrentInfo> results;
    
    if (query.isEmpty()) {
        return results;
    }
    
    // Try FTS search first
    QSqlQuery ftsQuery(db_);
    ftsQuery.prepare(R"(
        SELECT t.info_hash, t.name, t.size, t.seeders, t.leechers, t.indexed_date, t.category, t.description
        FROM torrents t
        INNER JOIN torrents_fts fts ON t.info_hash = fts.info_hash
        WHERE torrents_fts MATCH ?
        ORDER BY t.seeders DESC
        LIMIT ?
    )");
    ftsQuery.addBindValue(query);
    ftsQuery.addBindValue(limit);
    
    if (ftsQuery.exec()) {
        while (ftsQuery.next()) {
            TorrentInfo info;
            info.infoHash = ftsQuery.value(0).toString();
            info.name = ftsQuery.value(1).toString();
            info.size = ftsQuery.value(2).toLongLong();
            info.seeders = ftsQuery.value(3).toInt();
            info.leechers = ftsQuery.value(4).toInt();
            info.indexedDate = ftsQuery.value(5).toDateTime();
            info.category = ftsQuery.value(6).toString();
            info.description = ftsQuery.value(7).toString();
            results.append(info);
        }
        
        if (!results.isEmpty()) {
            return results;
        }
    }
    
    // Fallback to LIKE search if FTS fails
    QSqlQuery likeQuery(db_);
    likeQuery.prepare(R"(
        SELECT info_hash, name, size, seeders, leechers, indexed_date, category, description
        FROM torrents
        WHERE name LIKE ? OR description LIKE ?
        ORDER BY seeders DESC
        LIMIT ?
    )");
    likeQuery.addBindValue("%" + query + "%");
    likeQuery.addBindValue("%" + query + "%");
    likeQuery.addBindValue(limit);
    
    if (likeQuery.exec()) {
        while (likeQuery.next()) {
            TorrentInfo info;
            info.infoHash = likeQuery.value(0).toString();
            info.name = likeQuery.value(1).toString();
            info.size = likeQuery.value(2).toLongLong();
            info.seeders = likeQuery.value(3).toInt();
            info.leechers = likeQuery.value(4).toInt();
            info.indexedDate = likeQuery.value(5).toDateTime();
            info.category = likeQuery.value(6).toString();
            info.description = likeQuery.value(7).toString();
            results.append(info);
        }
    }
    
    return results;
}

QVector<TorrentInfo> TorrentDatabase::getRecentTorrents(int limit)
{
    QVector<TorrentInfo> results;
    
    QSqlQuery query(db_);
    query.prepare(R"(
        SELECT info_hash, name, size, seeders, leechers, indexed_date, category, description
        FROM torrents
        ORDER BY indexed_date DESC
        LIMIT ?
    )");
    query.addBindValue(limit);
    
    if (query.exec()) {
        while (query.next()) {
            TorrentInfo info;
            info.infoHash = query.value(0).toString();
            info.name = query.value(1).toString();
            info.size = query.value(2).toLongLong();
            info.seeders = query.value(3).toInt();
            info.leechers = query.value(4).toInt();
            info.indexedDate = query.value(5).toDateTime();
            info.category = query.value(6).toString();
            info.description = query.value(7).toString();
            results.append(info);
        }
    }
    
    return results;
}

QVector<TorrentInfo> TorrentDatabase::getTopTorrents(int limit)
{
    QVector<TorrentInfo> results;
    
    QSqlQuery query(db_);
    query.prepare(R"(
        SELECT info_hash, name, size, seeders, leechers, indexed_date, category, description
        FROM torrents
        ORDER BY seeders DESC, leechers DESC
        LIMIT ?
    )");
    query.addBindValue(limit);
    
    if (query.exec()) {
        while (query.next()) {
            TorrentInfo info;
            info.infoHash = query.value(0).toString();
            info.name = query.value(1).toString();
            info.size = query.value(2).toLongLong();
            info.seeders = query.value(3).toInt();
            info.leechers = query.value(4).toInt();
            info.indexedDate = query.value(5).toDateTime();
            info.category = query.value(6).toString();
            info.description = query.value(7).toString();
            results.append(info);
        }
    }
    
    return results;
}

int TorrentDatabase::getTorrentCount() const
{
    QSqlQuery query("SELECT COUNT(*) FROM torrents", db_);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

qint64 TorrentDatabase::getDatabaseSize() const
{
    QFileInfo fileInfo(getDatabasePath());
    return fileInfo.size();
}

TorrentDatabase::Statistics TorrentDatabase::getStatistics() const
{
    Statistics stats;
    stats.totalTorrents = getTorrentCount();
    
    // Total size
    QSqlQuery sizeQuery("SELECT SUM(size) FROM torrents", db_);
    if (sizeQuery.exec() && sizeQuery.next()) {
        stats.totalSize = sizeQuery.value(0).toLongLong();
    }
    
    // Torrents today
    QSqlQuery todayQuery("SELECT COUNT(*) FROM torrents WHERE date(indexed_date) = date('now')", db_);
    if (todayQuery.exec() && todayQuery.next()) {
        stats.torrentsToday = todayQuery.value(0).toInt();
    }
    
    // Torrents this week
    QSqlQuery weekQuery("SELECT COUNT(*) FROM torrents WHERE date(indexed_date) >= date('now', '-7 days')", db_);
    if (weekQuery.exec() && weekQuery.next()) {
        stats.torrentsWeek = weekQuery.value(0).toInt();
    }
    
    return stats;
}

QString TorrentDatabase::getDatabasePath() const
{
    return dataDirectory_ + "/rats-search.db";
}

