#include "torrentdatabase.h"
#include "manticoremanager.h"
#include "sphinxql.h"
#include <QDebug>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QMimeDatabase>

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
    
    // Get statistics
    Statistics stats = getStatistics();
    qInfo() << "Database initialized:";
    qInfo() << "  Torrents:" << stats.totalTorrents;
    qInfo() << "  Files:" << stats.totalFiles;
    qInfo() << "  Total size:" << (stats.totalSize / (1024*1024*1024)) << "GB";
    
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
        qDebug() << "Torrent already exists:" << torrent.hash;
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
    values["ipv4"] = torrent.ipv4;
    values["port"] = torrent.port;
    values["contentType"] = torrent.contentType;
    values["contentCategory"] = torrent.contentCategory;
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
    
    emit torrentAdded(torrent.hash);
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
    values["contentType"] = torrent.contentType;
    values["contentCategory"] = torrent.contentCategory;
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
    
    sphinxQL_->deleteFrom("torrents", {{"hash", hash}});
    sphinxQL_->deleteFrom("files", {{"hash", hash}});
    
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
    
    if (!isReady() || options.query.isEmpty()) {
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
        
        if (options.safeSearch && info.contentCategory == static_cast<int>(ContentCategory::XXX)) {
            continue;
        }
        
        // Add matched file paths
        for (const QString& path : hashSnippets[info.hash]) {
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

TorrentDatabase::Statistics TorrentDatabase::getStatistics() const
{
    Statistics stats;
    
    if (!isReady()) {
        return stats;
    }
    
    auto results = sphinxQL_->query(
        "SELECT MAX(id) as maxid, COUNT(*) as cnt, SUM(files) as numfiles, SUM(size) as totalsize FROM torrents");
    
    if (!results.isEmpty()) {
        stats.totalTorrents = results[0]["cnt"].toLongLong();
        stats.totalFiles = results[0]["numfiles"].toLongLong();
        stats.totalSize = results[0]["totalsize"].toLongLong();
    }
    
    return stats;
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
    
    info.id = row.value("id").toLongLong();
    info.hash = row.value("hash").toString();
    info.name = row.value("name").toString();
    info.size = row.value("size").toLongLong();
    info.files = row.value("files").toInt();
    info.piecelength = row.value("piecelength").toInt();
    info.added = QDateTime::fromSecsSinceEpoch(row.value("added").toLongLong());
    info.ipv4 = row.value("ipv4").toString();
    info.port = row.value("port").toInt();
    info.contentType = row.value("contentType").toInt();
    info.contentCategory = row.value("contentCategory").toInt();
    info.seeders = row.value("seeders").toInt();
    info.leechers = row.value("leechers").toInt();
    info.completed = row.value("completed").toInt();
    
    qint64 trackersChecked = row.value("trackersChecked").toLongLong();
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

int TorrentDatabase::contentTypeId(const QString& type)
{
    static QMap<QString, int> types = {
        {"video", 1}, {"audio", 2}, {"books", 3}, {"pictures", 4},
        {"software", 5}, {"games", 6}, {"archive", 7}, {"bad", 100}
    };
    return types.value(type.toLower(), 0);
}

QString TorrentDatabase::contentTypeFromId(int id)
{
    static QMap<int, QString> types = {
        {1, "video"}, {2, "audio"}, {3, "books"}, {4, "pictures"},
        {5, "software"}, {6, "games"}, {7, "archive"}, {100, "bad"}
    };
    return types.value(id, "unknown");
}

int TorrentDatabase::contentCategoryId(const QString& category)
{
    static QMap<QString, int> categories = {
        {"movie", 1}, {"series", 2}, {"documentary", 3}, {"anime", 4},
        {"music", 5}, {"ebook", 6}, {"comics", 7}, {"software", 8},
        {"game", 9}, {"xxx", 100}
    };
    return categories.value(category.toLower(), 0);
}

QString TorrentDatabase::contentCategoryFromId(int id)
{
    static QMap<int, QString> categories = {
        {1, "movie"}, {2, "series"}, {3, "documentary"}, {4, "anime"},
        {5, "music"}, {6, "ebook"}, {7, "comics"}, {8, "software"},
        {9, "game"}, {100, "xxx"}
    };
    return categories.value(id, "unknown");
}

void TorrentDatabase::detectContentType(TorrentInfo& torrent)
{
    QMimeDatabase mimeDb;
    
    // Count file types
    int videoCount = 0, audioCount = 0, imageCount = 0;
    int archiveCount = 0, executableCount = 0, docCount = 0;
    
    for (const TorrentFile& file : torrent.filesList) {
        QString mimeType = mimeDb.mimeTypeForFile(file.path, QMimeDatabase::MatchExtension).name();
        
        if (mimeType.startsWith("video/")) videoCount++;
        else if (mimeType.startsWith("audio/")) audioCount++;
        else if (mimeType.startsWith("image/")) imageCount++;
        else if (mimeType.contains("zip") || mimeType.contains("rar") || 
                 mimeType.contains("7z") || mimeType.contains("tar")) archiveCount++;
        else if (mimeType.contains("executable") || file.path.endsWith(".exe") ||
                 file.path.endsWith(".msi") || file.path.endsWith(".dmg")) executableCount++;
        else if (mimeType.contains("pdf") || mimeType.contains("epub") ||
                 file.path.endsWith(".mobi") || file.path.endsWith(".fb2")) docCount++;
    }
    
    // Determine type
    if (videoCount > 0) {
        torrent.contentType = static_cast<int>(ContentType::Video);
    } else if (audioCount > 0) {
        torrent.contentType = static_cast<int>(ContentType::Audio);
    } else if (docCount > 0) {
        torrent.contentType = static_cast<int>(ContentType::Books);
    } else if (imageCount > 0) {
        torrent.contentType = static_cast<int>(ContentType::Pictures);
    } else if (executableCount > 0) {
        torrent.contentType = static_cast<int>(ContentType::Software);
    } else if (archiveCount > 0) {
        torrent.contentType = static_cast<int>(ContentType::Archive);
    }
    
    // Check for adult content in name
    QString nameLower = torrent.name.toLower();
    static QStringList adultKeywords = {"xxx", "porn", "sex", "adult", "18+"};
    for (const QString& keyword : adultKeywords) {
        if (nameLower.contains(keyword)) {
            torrent.contentCategory = static_cast<int>(ContentCategory::XXX);
            break;
        }
    }
}

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
    return TorrentDatabase::contentTypeFromId(contentType);
}

QString TorrentInfo::contentCategoryString() const
{
    return TorrentDatabase::contentCategoryFromId(contentCategory);
}
