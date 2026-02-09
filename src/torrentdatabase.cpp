#include "torrentdatabase.h"
#include "contentdetector.h"
#include "manticoremanager.h"
#include "sphinxql.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>

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
    qInfo() << "Initializing Manticore Search database...";
    
    // Create manager
    manager_ = std::make_unique<ManticoreManager>(dataDirectory_, this);
    
    connect(manager_.get(), &ManticoreManager::error, this, [this](const QString& msg) {
        emit databaseError(msg);
    });
    
    // Start Manticore
    if (!manager_->start()) {
        qCritical() << "Failed to start Manticore Search";
        return false;
    }
    
    // Create SphinxQL interface
    sphinxQL_ = std::make_unique<SphinxQL>(manager_.get(), this);
    
    connect(sphinxQL_.get(), &SphinxQL::queryError, this, [this](const QString& msg) {
        emit databaseError(msg);
    });
    
    // Get current IDs
    nextTorrentId_ = sphinxQL_->getMaxId("torrents") + 1;
    nextFilesId_ = sphinxQL_->getMaxId("files") + 1;
    
    // Load initial statistics from database (one-time query, like legacy spider.js)
    auto results = sphinxQL_->query(
        "SELECT COUNT(*) as cnt, SUM(files) as numfiles, SUM(size) as totalsize FROM torrents");
    
    if (!results.isEmpty()) {
        currentStats_.totalTorrents = results[0]["cnt"].toLongLong();
        currentStats_.totalFiles = results[0]["numfiles"].toLongLong();
        currentStats_.totalSize = results[0]["totalsize"].toLongLong();
    }
    
    qInfo() << "Database initialized:";
    qInfo() << "  Torrents:" << currentStats_.totalTorrents;
    qInfo() << "  Files:" << currentStats_.totalFiles;
    qInfo() << "  Total size:" << (currentStats_.totalSize / (1024*1024*1024)) << "GB";
    
    emit ready();
    return true;
}

void TorrentDatabase::close()
{
    if (manager_) {
        qInfo() << "Closing database...";
        manager_->stop();
    }
}

bool TorrentDatabase::isReady() const
{
    return manager_ && manager_->isRunning();
}

bool TorrentDatabase::addTorrent(const TorrentInfo& torrent)
{
    if (!isReady() || !torrent.isValid()) {
        return false;
    }
    
    // Check if already exists
    auto results = sphinxQL_->query("SELECT id FROM torrents WHERE hash = ?", {torrent.hash});
    if (!results.isEmpty()) {
        qInfo() << "Torrent already exists:" << torrent.hash;
        return true;
    }
    
    // Build values map
    QVariantMap values;
    values["id"] = nextTorrentId_++;
    values["hash"] = torrent.hash;
    values["name"] = torrent.name;
    values["nameIndex"] = buildNameIndex(torrent);
    values["size"] = torrent.size;
    values["files"] = torrent.files;
    values["piecelength"] = torrent.piecelength;
    values["added"] = torrent.added.isValid() ? torrent.added.toSecsSinceEpoch() 
                                               : QDateTime::currentSecsSinceEpoch();
    values["ipv4"] = torrent.ipv4.isEmpty() ? QString("") : torrent.ipv4;  // Ensure non-null for Manticore
    values["port"] = torrent.port;
    values["contentType"] = torrent.contentTypeId;
    values["contentCategory"] = torrent.contentCategoryId;
    values["seeders"] = torrent.seeders;
    values["leechers"] = torrent.leechers;
    values["completed"] = torrent.completed;
    values["trackersChecked"] = torrent.trackersChecked.isValid() 
                                 ? torrent.trackersChecked.toSecsSinceEpoch() : 0;
    values["good"] = torrent.good;
    values["bad"] = torrent.bad;
    
    if (!torrent.info.isEmpty()) {
        values["info"] = torrent.info;
    }
    
    if (!sphinxQL_->insertValues("torrents", values)) {
        qWarning() << "Failed to insert torrent:" << torrent.hash;
        return false;
    }
    
    // Add files
    if (!torrent.filesList.isEmpty()) {
        addFilesToDatabase(torrent);
    }
    
    // Update cached statistics incrementally (like legacy spider.js p2p.info)
    currentStats_.totalTorrents++;
    currentStats_.totalFiles += torrent.files;
    currentStats_.totalSize += torrent.size;
    
    emit torrentAdded(torrent.hash);
    emit statisticsChanged(currentStats_.totalTorrents, currentStats_.totalFiles, currentStats_.totalSize);
    return true;
}

bool TorrentDatabase::addFilesToDatabase(const TorrentInfo& torrent)
{
    if (torrent.filesList.isEmpty()) {
        return true;
    }
    
    // First delete existing files
    sphinxQL_->deleteFrom("files", {{"hash", torrent.hash}});
    
    // Build concatenated path and size strings (as in legacy)
    QStringList paths;
    QStringList sizes;
    for (const TorrentFile& file : torrent.filesList) {
        paths << file.path;
        sizes << QString::number(file.size);
    }
    
    QVariantMap values;
    values["id"] = nextFilesId_++;
    values["hash"] = torrent.hash;
    values["path"] = paths.join("\n");
    values["size"] = sizes.join("\n");
    
    return sphinxQL_->insertValues("files", values);
}

bool TorrentDatabase::updateTorrent(const TorrentInfo& torrent)
{
    if (!isReady() || !torrent.isValid()) {
        return false;
    }
    
    QVariantMap values;
    values["name"] = torrent.name;
    values["size"] = torrent.size;
    values["files"] = torrent.files;
    values["contentType"] = torrent.contentTypeId;
    values["contentCategory"] = torrent.contentCategoryId;
    values["seeders"] = torrent.seeders;
    values["leechers"] = torrent.leechers;
    values["completed"] = torrent.completed;
    values["good"] = torrent.good;
    values["bad"] = torrent.bad;
    
    if (!torrent.info.isEmpty()) {
        values["info"] = torrent.info;
    }
    
    if (!sphinxQL_->updateValues("torrents", values, {{"hash", torrent.hash}})) {
        return false;
    }
    
    emit torrentUpdated(torrent.hash);
    return true;
}

bool TorrentDatabase::removeTorrent(const QString& hash)
{
    if (!isReady()) {
        return false;
    }
    
    // Get torrent info before removal for statistics update
    auto results = sphinxQL_->query("SELECT size, files FROM torrents WHERE hash = ?", {hash});
    qint64 torrentSize = 0;
    int torrentFiles = 0;
    if (!results.isEmpty()) {
        torrentSize = results[0]["size"].toLongLong();
        torrentFiles = results[0]["files"].toInt();
    }
    
    sphinxQL_->deleteFrom("torrents", {{"hash", hash}});
    sphinxQL_->deleteFrom("files", {{"hash", hash}});
    
    // Update cached statistics
    if (torrentSize > 0 || torrentFiles > 0) {
        currentStats_.totalTorrents = qMax(0LL, currentStats_.totalTorrents - 1);
        currentStats_.totalFiles = qMax(0LL, currentStats_.totalFiles - torrentFiles);
        currentStats_.totalSize = qMax(0LL, currentStats_.totalSize - torrentSize);
        emit statisticsChanged(currentStats_.totalTorrents, currentStats_.totalFiles, currentStats_.totalSize);
    }
    
    emit torrentRemoved(hash);
    return true;
}

TorrentInfo TorrentDatabase::getTorrent(const QString& hash, bool includeFiles)
{
    TorrentInfo info;
    
    if (!isReady()) {
        return info;
    }
    
    auto results = sphinxQL_->query("SELECT * FROM torrents WHERE hash = ?", {hash});
    if (results.isEmpty()) {
        return info;
    }
    
    info = rowToTorrent(results[0]);
    
    if (includeFiles) {
        info.filesList = getFilesForTorrent(hash);
    }
    
    return info;
}

bool TorrentDatabase::hasTorrent(const QString& hash)
{
    if (!isReady()) {
        return false;
    }
    
    auto results = sphinxQL_->query("SELECT id FROM torrents WHERE hash = ? LIMIT 1", {hash});
    return !results.isEmpty();
}

QVector<TorrentInfo> TorrentDatabase::searchTorrents(const SearchOptions& options)
{
    QVector<TorrentInfo> results;
    
    qInfo() << "searchTorrents: query=" << options.query 
            << "limit=" << options.limit 
            << "orderBy=" << options.orderBy;
    
    if (!isReady()) {
        qWarning() << "searchTorrents: Database is not ready!";
        return results;
    }
    
    if (options.query.isEmpty()) {
        qWarning() << "searchTorrents: Empty query";
        return results;
    }
    
    // Check if query is a SHA1 hash
    bool isSHA1 = options.query.length() == 40 && 
                  QRegularExpression("^[0-9a-fA-F]+$").match(options.query).hasMatch();
    
    // Build query
    QString sql;
    QVariantList params;
    
    if (isSHA1) {
        sql = "SELECT * FROM torrents WHERE hash = ?";
        params << options.query.toLower();
    } else {
        sql = "SELECT * FROM torrents WHERE MATCH(?)";
        params << options.query;
    }
    
    // Add filters
    if (options.safeSearch) {
        sql += QString(" AND contentCategory != %1").arg(static_cast<int>(ContentCategory::XXX));
    }
    
    if (!options.contentType.isEmpty()) {
        sql += QString(" AND contentType = %1").arg(contentTypeId(options.contentType));
    }
    
    if (options.sizeMin > 0) {
        sql += QString(" AND size > %1").arg(options.sizeMin);
    }
    if (options.sizeMax > 0) {
        sql += QString(" AND size < %1").arg(options.sizeMax);
    }
    
    if (options.filesMin > 0) {
        sql += QString(" AND files > %1").arg(options.filesMin);
    }
    if (options.filesMax > 0) {
        sql += QString(" AND files < %1").arg(options.filesMax);
    }
    
    // Order by
    if (!options.orderBy.isEmpty()) {
        sql += QString(" ORDER BY %1 %2")
            .arg(options.orderBy, options.orderDesc ? "DESC" : "ASC");
    }
    
    // Limit
    sql += QString(" LIMIT %1,%2").arg(options.index).arg(options.limit);
    
    auto rows = sphinxQL_->query(sql, params);
    for (const auto& row : rows) {
        results.append(rowToTorrent(row));
    }
    
    return results;
}

QVector<TorrentInfo> TorrentDatabase::searchFiles(const SearchOptions& options)
{
    QVector<TorrentInfo> results;
    
    if (!isReady() || options.query.isEmpty()) {
        return results;
    }
    
    // Search in files table
    QString sql = "SELECT *, SNIPPET(path, ?, 'around=100', 'force_all_words=1') as snipplet "
                  "FROM files WHERE MATCH(?) LIMIT ?,?";
    
    auto fileRows = sphinxQL_->query(sql, {options.query, options.query, 
                                           options.index, options.limit});
    
    if (fileRows.isEmpty()) {
        return results;
    }
    
    // Collect hashes and snippets
    QMap<QString, QStringList> hashSnippets;
    for (const auto& row : fileRows) {
        QString hash = row["hash"].toString();
        QString snippet = row["snipplet"].toString();
        
        // Extract highlighted parts
        for (const QString& line : snippet.split('\n')) {
            if (line.contains("<b>")) {
                hashSnippets[hash].append(line);
            }
        }
    }
    
    if (hashSnippets.isEmpty()) {
        return results;
    }
    
    // Get torrent info for found hashes
    QStringList escapedHashes;
    for (const QString& hash : hashSnippets.keys()) {
        escapedHashes << sphinxQL_->escapeString(hash);
    }
    
    QString inClause = escapedHashes.join(",");
    auto torrentRows = sphinxQL_->query(
        QString("SELECT * FROM torrents WHERE hash IN(%1)").arg(inClause));
    
    for (const auto& row : torrentRows) {
        TorrentInfo info = rowToTorrent(row);
        
        if (options.safeSearch && info.contentCategoryId == static_cast<int>(ContentCategory::XXX)) {
            continue;
        }
        
        // Mark as file search result and add matched file paths
        info.isFileMatch = true;
        info.matchingPaths = hashSnippets[info.hash];
        
        // Also populate filesList for compatibility
        for (const QString& path : info.matchingPaths) {
            TorrentFile file;
            file.path = path;
            file.size = 0;
            info.filesList.append(file);
        }
        
        results.append(info);
    }
    
    // Sort if needed
    if (!options.orderBy.isEmpty()) {
        std::sort(results.begin(), results.end(), 
            [&options](const TorrentInfo& a, const TorrentInfo& b) {
                if (options.orderBy == "seeders") {
                    return options.orderDesc ? a.seeders > b.seeders : a.seeders < b.seeders;
                } else if (options.orderBy == "size") {
                    return options.orderDesc ? a.size > b.size : a.size < b.size;
                }
                return false;
            });
    }
    
    return results;
}

QVector<TorrentInfo> TorrentDatabase::getRecentTorrents(int limit)
{
    QVector<TorrentInfo> results;
    
    if (!isReady()) {
        return results;
    }
    
    auto rows = sphinxQL_->query(
        QString("SELECT * FROM torrents ORDER BY added DESC LIMIT 0,%1").arg(limit));
    
    for (const auto& row : rows) {
        results.append(rowToTorrent(row));
    }
    
    return results;
}

QVector<TorrentInfo> TorrentDatabase::getTopTorrents(const QString& type, 
                                                       const QString& time,
                                                       int index, 
                                                       int limit)
{
    QVector<TorrentInfo> results;
    
    if (!isReady()) {
        return results;
    }
    
    QString where = QString("seeders > 0 AND contentCategory != %1")
        .arg(static_cast<int>(ContentCategory::XXX));
    
    if (!type.isEmpty()) {
        where += QString(" AND contentType = %1").arg(contentTypeId(type));
    }
    
    if (!time.isEmpty()) {
        qint64 now = QDateTime::currentSecsSinceEpoch();
        qint64 cutoff = 0;
        
        if (time == "hours") {
            cutoff = now - (60 * 60 * 24);
        } else if (time == "week") {
            cutoff = now - (60 * 60 * 24 * 7);
        } else if (time == "month") {
            cutoff = now - (60 * 60 * 24 * 30);
        }
        
        if (cutoff > 0) {
            where += QString(" AND added > %1").arg(cutoff);
        }
    }
    
    QString sql = QString("SELECT * FROM torrents WHERE %1 ORDER BY seeders DESC LIMIT %2,%3")
        .arg(where).arg(index).arg(limit);
    
    auto rows = sphinxQL_->query(sql);
    for (const auto& row : rows) {
        results.append(rowToTorrent(row));
    }
    
    return results;
}

bool TorrentDatabase::updateTrackerInfo(const QString& hash, int seeders, int leechers, int completed)
{
    if (!isReady()) {
        return false;
    }
    
    QVariantMap values;
    values["seeders"] = seeders;
    values["leechers"] = leechers;
    values["completed"] = completed;
    values["trackersChecked"] = QDateTime::currentSecsSinceEpoch();
    
    return sphinxQL_->updateValues("torrents", values, {{"hash", hash}});
}

bool TorrentDatabase::updateTorrentInfoField(const QString& hash, const QJsonObject& info)
{
    if (!isReady() || hash.isEmpty() || info.isEmpty()) {
        return false;
    }
    
    // First, get existing info to merge with
    auto results = sphinxQL_->query("SELECT info FROM torrents WHERE hash = ?", {hash});
    if (results.isEmpty()) {
        return false;
    }
    
    QJsonObject existingInfo;
    QString infoStr = results[0]["info"].toString();
    if (!infoStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(infoStr.toUtf8());
        if (doc.isObject()) {
            existingInfo = doc.object();
        }
    }
    
    // Merge: new values overwrite existing, but arrays (like trackers) are merged
    for (auto it = info.begin(); it != info.end(); ++it) {
        if (it.key() == "trackers" && existingInfo.contains("trackers")) {
            // Merge tracker arrays (deduplicate)
            QJsonArray existingTrackers = existingInfo["trackers"].toArray();
            QJsonArray newTrackers = it.value().toArray();
            QSet<QString> existing;
            for (const QJsonValue& v : existingTrackers) {
                existing.insert(v.toString());
            }
            for (const QJsonValue& v : newTrackers) {
                if (!existing.contains(v.toString())) {
                    existingTrackers.append(v);
                }
            }
            existingInfo["trackers"] = existingTrackers;
        } else {
            existingInfo[it.key()] = it.value();
        }
    }
    
    QVariantMap values;
    QJsonDocument doc(existingInfo);
    values["info"] = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    
    return sphinxQL_->updateValues("torrents", values, {{"hash", hash}});
}

bool TorrentDatabase::updateTorrentContentType(const QString& hash, int contentTypeId, int contentCategoryId)
{
    if (!isReady()) {
        return false;
    }
    
    QVariantMap values;
    values["contentType"] = contentTypeId;
    values["contentCategory"] = contentCategoryId;
    
    return sphinxQL_->updateValues("torrents", values, {{"hash", hash}});
}

TorrentDatabase::Statistics TorrentDatabase::getStatistics() const
{
    // Return cached statistics (no DB query - updated incrementally)
    return currentStats_;
}

qint64 TorrentDatabase::getTorrentCount() const
{
    if (!isReady()) {
        return 0;
    }
    return sphinxQL_->getCount("torrents");
}

qint64 TorrentDatabase::getNextTorrentId()
{
    return nextTorrentId_++;
}

qint64 TorrentDatabase::getNextFilesId()
{
    return nextFilesId_++;
}

TorrentInfo TorrentDatabase::rowToTorrent(const QVariantMap& row)
{
    TorrentInfo info;
    
    // Note: Manticore returns all field names in lowercase!
    info.id = row.value("id").toLongLong();
    info.hash = row.value("hash").toString();
    info.name = row.value("name").toString();
    info.size = row.value("size").toLongLong();
    info.files = row.value("files").toInt();
    info.piecelength = row.value("piecelength").toInt();
    info.added = QDateTime::fromSecsSinceEpoch(row.value("added").toLongLong());
    info.ipv4 = row.value("ipv4").toString();
    info.port = row.value("port").toInt();
    info.contentTypeId = row.value("contenttype").toInt();        // lowercase!
    info.contentCategoryId = row.value("contentcategory").toInt(); // lowercase!
    info.contentType = contentTypeFromId(info.contentTypeId);
    info.contentCategory = contentCategoryFromId(info.contentCategoryId);
    info.seeders = row.value("seeders").toInt();
    info.leechers = row.value("leechers").toInt();
    info.completed = row.value("completed").toInt();
    
    qint64 trackersChecked = row.value("trackerschecked").toLongLong(); // lowercase!
    if (trackersChecked > 0) {
        info.trackersChecked = QDateTime::fromSecsSinceEpoch(trackersChecked);
    }
    
    info.good = row.value("good").toInt();
    info.bad = row.value("bad").toInt();
    
    // Parse JSON info field
    QString infoStr = row.value("info").toString();
    if (!infoStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(infoStr.toUtf8());
        if (doc.isObject()) {
            info.info = doc.object();
        }
    }
    
    return info;
}

QVector<TorrentFile> TorrentDatabase::getFilesForTorrent(const QString& hash)
{
    QVector<TorrentFile> files;
    
    auto results = sphinxQL_->query("SELECT * FROM files WHERE hash = ?", {hash});
    if (results.isEmpty()) {
        return files;
    }
    
    // Parse concatenated path and size
    QString pathStr = results[0].value("path").toString();
    QString sizeStr = results[0].value("size").toString();
    
    QStringList paths = pathStr.split('\n');
    QStringList sizes = sizeStr.split('\n');
    
    for (int i = 0; i < paths.size(); ++i) {
        TorrentFile file;
        file.path = paths[i];
        file.size = (i < sizes.size()) ? sizes[i].toLongLong() : 0;
        files.append(file);
    }
    
    return files;
}

QHash<QString, QVector<TorrentFile>> TorrentDatabase::getFilesForTorrents(const QStringList& hashes)
{
    QHash<QString, QVector<TorrentFile>> result;
    
    if (hashes.isEmpty() || !isReady()) {
        return result;
    }
    
    // Build IN clause with quoted hashes
    QStringList quotedHashes;
    for (const QString& hash : hashes) {
        if (hash.length() == 40) {
            quotedHashes.append(QString("'%1'").arg(hash));
        }
    }
    
    if (quotedHashes.isEmpty()) {
        return result;
    }
    
    // Single batch query for all files
    QString sql = QString("SELECT * FROM files WHERE hash IN (%1)")
        .arg(quotedHashes.join(","));
    
    auto fileRows = sphinxQL_->query(sql);
    
    // Parse files and group by hash
    for (const auto& fileRow : fileRows) {
        QString hash = fileRow.value("hash").toString();
        
        // Parse concatenated path and size (same format as getFilesForTorrent)
        QString pathStr = fileRow.value("path").toString();
        QString sizeStr = fileRow.value("size").toString();
        
        QStringList paths = pathStr.split('\n');
        QStringList sizes = sizeStr.split('\n');
        
        QVector<TorrentFile> files;
        for (int i = 0; i < paths.size(); ++i) {
            TorrentFile file;
            file.path = paths[i];
            file.size = (i < sizes.size()) ? sizes[i].toLongLong() : 0;
            files.append(file);
        }
        
        result[hash] = files;
    }
    
    return result;
}

bool TorrentDatabase::optimize()
{
    if (!isReady()) {
        return false;
    }
    
    sphinxQL_->optimize("torrents");
    sphinxQL_->optimize("files");
    return true;
}

// Static helper methods

QString TorrentDatabase::buildNameIndex(const TorrentInfo& torrent)
{
    QString index = torrent.name;
    
    // Add info name if available
    if (torrent.info.contains("name")) {
        QString infoName = torrent.info["name"].toString();
        if (!infoName.isEmpty() && infoName.length() < 800) {
            index += " " + infoName;
        }
    }
    
    return index;
}

QString TorrentInfo::contentTypeString() const
{
    return ContentDetector::contentTypeFromId(contentTypeId);
}

QString TorrentInfo::contentCategoryString() const
{
    return ContentDetector::contentCategoryFromId(contentCategoryId);
}

void TorrentInfo::setContentTypeFromString(const QString& type)
{
    contentTypeId = ContentDetector::contentTypeId(type);
    contentType = type;
}

void TorrentInfo::setContentCategoryFromString(const QString& category)
{
    contentCategoryId = ContentDetector::contentCategoryId(category);
    contentCategory = category;
}

QVector<TorrentInfo> TorrentDatabase::getRandomTorrents(int limit, bool includeFiles)
{
    QVector<TorrentInfo> results;
    
    if (!isReady()) {
        return results;
    }
    
    // Get random torrents using Manticore's RAND() function
    // Also exclude adult content and require at least some seeders
    QString sql = QString("SELECT * FROM torrents WHERE seeders > 0 AND contentCategory != %1 "
                          "ORDER BY RAND() LIMIT %2")
        .arg(static_cast<int>(ContentCategory::XXX))
        .arg(limit);
    
    auto rows = sphinxQL_->query(sql);
    
    // Build results and collect hashes
    QHash<QString, int> hashToIndex;
    QStringList hashes;
    for (const auto& row : rows) {
        TorrentInfo torrent = rowToTorrent(row);
        hashToIndex[torrent.hash] = results.size();
        hashes.append(torrent.hash);
        results.append(torrent);
    }
    
    // Batch fetch files using shared method
    if (includeFiles && !results.isEmpty()) {
        QHash<QString, QVector<TorrentFile>> filesMap = getFilesForTorrents(hashes);
        
        for (auto it = filesMap.begin(); it != filesMap.end(); ++it) {
            int idx = hashToIndex.value(it.key(), -1);
            if (idx >= 0) {
                results[idx].filesList = it.value();
            }
        }
    }
    
    return results;
}
