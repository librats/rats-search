/**
 * @file test_manticore_queries.cpp
 * @brief Integration test that starts Manticore and verifies all query types used in the application
 * 
 * This test starts a real Manticore Search instance and runs all the query patterns
 * used throughout the application to verify their SphinxQL syntax is correct.
 * 
 * Requires: searchd executable accessible via the standard import paths
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include "manticoremanager.h"
#include "sphinxql.h"

class TestManticoreQueries : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // ========================================================================
    // Connection & Status
    // ========================================================================
    void testShowStatus();
    void testIsConnected();

    // ========================================================================
    // INSERT operations (torrents, files, feed, version, store)
    // ========================================================================
    void testInsertTorrent();
    void testInsertFiles();
    void testInsertFeedItem();
    void testInsertVersionRecord();
    void testInsertStoreRecord();

    // ========================================================================
    // SELECT / read operations
    // ========================================================================
    void testSelectAllTorrents();
    void testSelectTorrentByHash();
    void testSelectTorrentById();
    void testSelectIdByHash();
    void testSelectSpecificColumns();
    void testSelectInfoByHash();
    void testSelectMaxId();
    void testSelectCount();
    void testSelectCountWithWhere();
    void testSelectAggregation();
    void testSelectOrderByDesc();
    void testSelectOrderByAsc();
    void testSelectWithOffsetLimit();
    void testSelectTopTorrents();
    void testSelectTopTorrentsWithTimeFilter();
    void testSelectTopTorrentsWithTypeFilter();
    void testSelectRandomTorrents();
    void testSelectWithInClause();
    void testSelectFilesFromFiles();
    void testSelectFromFeed();
    void testSelectPaginatedById();
    void testSelectContentTypeMigrationQuery();

    // ========================================================================
    // Full-text MATCH search
    // ========================================================================
    void testMatchSearch();
    void testMatchSearchWithFilters();
    void testMatchSearchWithOrderAndLimit();
    void testMatchFileSearch();
    void testMatchFileSearchWithSnippet();

    // ========================================================================
    // Data integrity verification (from legacy speed.txt tests)
    // ========================================================================
    // Internal of fields
    void testIsFieldReady();
    void testInsertFilesAndVerifyFields();
    void testSearchFilesAndVerifyFields();
    void testBulkInsertFeedAndCount();
    void testInsertFeedJsonAndVerifyReadback();

    // ========================================================================
    // REPLACE operations
    // ========================================================================
    void testReplaceIntoStore();

    // ========================================================================
    // UPDATE operations
    // ========================================================================
    void testUpdateTorrentByHash();
    void testUpdateTrackerInfo();
    void testUpdateContentType();
    void testUpdateInfoField();

    // ========================================================================
    // DELETE operations
    // ========================================================================
    void testDeleteTorrentByHash();
    void testDeleteFilesByHash();
    void testDeleteFeedAll();

    // ========================================================================
    // Maintenance operations
    // ========================================================================
    void testOptimizeIndex();
    void testFlushRamchunk();
    void testTruncateTable();

    // ========================================================================
    // SphinxQL helper methods
    // ========================================================================
    void testInsertValues();
    void testReplaceValues();
    void testUpdateValues();
    void testDeleteFrom();
    void testGetMaxId();
    void testGetCount();

private:
    // Helper to insert a test torrent with given parameters
    bool insertTestTorrent(qint64 id, const QString& hash, const QString& name,
                           qint64 size = 1024*1024, int files = 1, int seeders = 10,
                           int contentType = 1, int contentCategory = 0);
    
    // Helper to insert a test file record
    bool insertTestFiles(qint64 id, const QString& hash, const QString& path, const QString& sizeStr);
    SphinxQL::Results queryUntilFieldReady(
        const QString& sql, const QVariantList& params,
        const QString& field, int maxRetries = 10, int delayMs = 10);

    QTemporaryDir *tempDir_ = nullptr;
    ManticoreManager *manager_ = nullptr;
    SphinxQL *sphinxql_ = nullptr;
};

SphinxQL::Results TestManticoreQueries::queryUntilFieldReady(
    const QString& sql, const QVariantList& params,
    const QString& field, int maxRetries, int delayMs)
{
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        auto results = sphinxql_->query(sql, params);
        if (!results.isEmpty() && results[0].contains(field)
            && results[0][field].isValid() && !results[0][field].toString().isEmpty()) {
            if (attempt > 0)
                qInfo() << "RT attribute" << field << "became ready after" << attempt
                        << "retries (" << (attempt * delayMs) << "ms)";
            return results;
        }
        QThread::msleep(delayMs);
    }
    // Return last attempt even if still empty — test will fail with clear assertion
    return sphinxql_->query(sql, params);
}

void TestManticoreQueries::initTestCase()
{
    // Create temporary directory for Manticore data
    tempDir_ = new QTemporaryDir();
    QVERIFY2(tempDir_->isValid(), "Failed to create temporary directory");
    
    qInfo() << "Test data directory:" << tempDir_->path();
    
    // Start Manticore
    manager_ = new ManticoreManager(tempDir_->path());
    
    bool started = manager_->start();
    QVERIFY2(started, "Failed to start Manticore Search. Is searchd available?");
    QVERIFY(manager_->isRunning());
    
    // Create SphinxQL interface
    sphinxql_ = new SphinxQL(manager_);
    QVERIFY(sphinxql_->isConnected());
    
    qInfo() << "Manticore started successfully on port" << manager_->port();
}

void TestManticoreQueries::cleanupTestCase()
{
    delete sphinxql_;
    sphinxql_ = nullptr;
    
    if (manager_) {
        manager_->stop();
        delete manager_;
        manager_ = nullptr;
    }
    
    delete tempDir_;
    tempDir_ = nullptr;
}

// ============================================================================
// Helpers
// ============================================================================

bool TestManticoreQueries::insertTestTorrent(qint64 id, const QString& hash,
                                              const QString& name, qint64 size,
                                              int files, int seeders,
                                              int contentType, int contentCategory)
{
    QVariantMap values;
    values["id"] = id;
    values["hash"] = hash;
    values["name"] = name;
    values["nameIndex"] = name;
    values["size"] = size;
    values["files"] = files;
    values["piecelength"] = 262144;
    values["added"] = QDateTime::currentSecsSinceEpoch();
    values["ipv4"] = QString("192.168.1.1");
    values["port"] = 6881;
    values["contentType"] = contentType;
    values["contentCategory"] = contentCategory;
    values["seeders"] = seeders;
    values["leechers"] = 5;
    values["completed"] = 100;
    values["trackersChecked"] = QDateTime::currentSecsSinceEpoch();
    values["good"] = 3;
    values["bad"] = 0;
    values["info"] = QString(R"({"trackers":["udp://tracker.example.com:6969"]})");
    
    return sphinxql_->insertValues("torrents", values);
}

bool TestManticoreQueries::insertTestFiles(qint64 id, const QString& hash,
                                            const QString& path, const QString& sizeStr)
{
    QVariantMap values;
    values["id"] = id;
    values["hash"] = hash;
    values["path"] = path;
    values["size"] = sizeStr;
    
    return sphinxql_->insertValues("files", values);
}

// ============================================================================
// Connection & Status
// ============================================================================

void TestManticoreQueries::testShowStatus()
{
    // Used by ManticoreManager::testConnection()
    auto results = sphinxql_->query("SHOW STATUS");
    QVERIFY2(!results.isEmpty(), "SHOW STATUS should return results");
}

void TestManticoreQueries::testIsConnected()
{
    QVERIFY(sphinxql_->isConnected());
}

// ============================================================================
// Internal of fields
// ============================================================================
void TestManticoreQueries::testIsFieldReady()
{
    // DESCRIBE returns one row per field with columns: Field, Type, Properties
    auto results = sphinxql_->query("DESCRIBE files");
    QVERIFY2(!results.isEmpty(), "DESCRIBE files returned no results");
    
    // Collect all field names
    QStringList fields;
    for (const auto& row : results) {
        fields << row["Field"].toString();
    }
    qInfo() << "files fields:" << fields;
    
    QVERIFY2(fields.contains("hash"), "Table should contain 'hash' field");
    QVERIFY2(fields.contains("path"), "Table should contain 'path' field");
    QVERIFY2(fields.contains("size"), "Table should contain 'size' field");
    QVERIFY2(fields.contains("id"), "Table should contain 'id' field");
}

// ============================================================================
// INSERT operations
// ============================================================================

void TestManticoreQueries::testInsertTorrent()
{
    // Pattern from TorrentDatabase::addTorrent()
    bool ok = insertTestTorrent(1, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                "Test Torrent - Ubuntu 24.04 ISO");
    QVERIFY2(ok, qPrintable("INSERT torrent failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testInsertFiles()
{
    // Pattern from TorrentDatabase::addFilesToDatabase()
    bool ok = insertTestFiles(1, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              "ubuntu-24.04-desktop-amd64.iso\nREADME.txt",
                              "4700000000\n1024");
    QVERIFY2(ok, qPrintable("INSERT files failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testInsertFeedItem()
{
    // Pattern from FeedManager::save()
    QJsonObject feedData;
    feedData["hash"] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    feedData["name"] = "Test Feed Item";
    feedData["size"] = 1024;
    feedData["seeders"] = 10;
    
    QVariantMap values;
    values["id"] = static_cast<qint64>(1);
    values["data"] = QString::fromUtf8(QJsonDocument(feedData).toJson(QJsonDocument::Compact));
    
    bool ok = sphinxql_->insertValues("feed", values);
    QVERIFY2(ok, qPrintable("INSERT feed failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testInsertVersionRecord()
{
    // version table: rt_attr_uint version, rt_field versionIndex
    QVariantMap values;
    values["id"] = static_cast<qint64>(1);
    values["version"] = 1;
    values["versionIndex"] = QString("v1");
    
    bool ok = sphinxql_->insertValues("version", values);
    QVERIFY2(ok, qPrintable("INSERT version failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testInsertStoreRecord()
{
    // store table: rt_field storeIndex, rt_attr_json data, rt_attr_string hash, rt_attr_string peerId
    QJsonObject storeData;
    storeData["type"] = "vote";
    storeData["torrentHash"] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    storeData["vote"] = "good";
    
    QVariantMap values;
    values["id"] = static_cast<qint64>(1);
    values["storeIndex"] = QString("vote:aaaa");
    values["data"] = QString::fromUtf8(QJsonDocument(storeData).toJson(QJsonDocument::Compact));
    values["hash"] = QString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    values["peerId"] = QString("peer123");
    
    bool ok = sphinxql_->insertValues("store", values);
    QVERIFY2(ok, qPrintable("INSERT store failed: " + sphinxql_->lastError()));
}

// ============================================================================
// SELECT / read operations
// ============================================================================

void TestManticoreQueries::testSelectAllTorrents()
{
    auto results = sphinxql_->query("SELECT * FROM torrents");
    QVERIFY2(sphinxql_->lastError().isEmpty() || !results.isEmpty(),
             qPrintable("SELECT * FROM torrents failed: " + sphinxql_->lastError()));
    QVERIFY(results.size() >= 1);
}

void TestManticoreQueries::testSelectTorrentByHash()
{
    // Pattern from TorrentDatabase::getTorrent() and addTorrent() existence check
    auto results = sphinxql_->query("SELECT * FROM torrents WHERE hash = ?",
                                    {QString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")});
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT by hash failed: " + sphinxql_->lastError()));
    QCOMPARE(results.size(), 1);
}

void TestManticoreQueries::testSelectTorrentById()
{
    auto results = sphinxql_->query("SELECT * FROM torrents WHERE id = 1");
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT by id failed: " + sphinxql_->lastError()));
    QCOMPARE(results.size(), 1);
}

void TestManticoreQueries::testSelectIdByHash()
{
    // Pattern from TorrentDatabase::addTorrent() - checking existence
    auto results = sphinxql_->query("SELECT id FROM torrents WHERE hash = ?",
                                    {QString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")});
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT id by hash failed: " + sphinxql_->lastError()));
    QCOMPARE(results.size(), 1);
    QVERIFY(results[0].contains("id"));
}

void TestManticoreQueries::testSelectSpecificColumns()
{
    // Pattern from TorrentDatabase::removeTorrent()
    auto results = sphinxql_->query("SELECT size, files FROM torrents WHERE hash = ?",
                                    {QString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")});
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT specific columns failed: " + sphinxql_->lastError()));
    QCOMPARE(results.size(), 1);
}

void TestManticoreQueries::testSelectInfoByHash()
{
    // Pattern from TorrentDatabase::updateTorrentInfoField()
    auto results = sphinxql_->query("SELECT info FROM torrents WHERE hash = ?",
                                    {QString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")});
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT info failed: " + sphinxql_->lastError()));
    QCOMPARE(results.size(), 1);
}

void TestManticoreQueries::testSelectMaxId()
{
    // Pattern from SphinxQL::getMaxId()
    auto results = sphinxql_->query("SELECT MAX(id) as maxid FROM torrents");
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT MAX(id) failed: " + sphinxql_->lastError()));
    QVERIFY(!results.isEmpty());
    QVERIFY(results[0].contains("maxid"));
}

void TestManticoreQueries::testSelectCount()
{
    // Pattern from SphinxQL::getCount()
    auto results = sphinxql_->query("SELECT COUNT(*) as cnt FROM torrents");
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT COUNT(*) failed: " + sphinxql_->lastError()));
    QVERIFY(!results.isEmpty());
    QVERIFY(results[0]["cnt"].toLongLong() >= 1);
}

void TestManticoreQueries::testSelectCountWithWhere()
{
    // Pattern from MigrationManager - count torrents with contenttype = 0
    auto results = sphinxql_->query("SELECT COUNT(*) as cnt FROM torrents WHERE contenttype = 0");
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT COUNT(*) WHERE failed: " + sphinxql_->lastError()));
    QVERIFY(!results.isEmpty());
}

void TestManticoreQueries::testSelectAggregation()
{
    // Pattern from TorrentDatabase::initialize() - statistics
    auto results = sphinxql_->query(
        "SELECT COUNT(*) as cnt, SUM(files) as numfiles, SUM(size) as totalsize FROM torrents");
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT aggregation failed: " + sphinxql_->lastError()));
    QVERIFY(!results.isEmpty());
    QVERIFY(results[0].contains("cnt"));
    QVERIFY(results[0].contains("numfiles"));
    QVERIFY(results[0].contains("totalsize"));
}

void TestManticoreQueries::testSelectOrderByDesc()
{
    // Pattern from TorrentDatabase::getRecentTorrents()
    auto results = sphinxql_->query("SELECT * FROM torrents ORDER BY added DESC LIMIT 0,10");
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT ORDER BY DESC failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testSelectOrderByAsc()
{
    // Pattern used in search with ASC sort
    auto results = sphinxql_->query("SELECT * FROM torrents ORDER BY seeders ASC LIMIT 0,10");
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT ORDER BY ASC failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testSelectWithOffsetLimit()
{
    // Pattern from TorrentDatabase::searchTorrents() with pagination
    auto results = sphinxql_->query("SELECT * FROM torrents LIMIT 0,50");
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT with LIMIT offset failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testSelectTopTorrents()
{
    // Pattern from TorrentDatabase::getTopTorrents() - base query
    int xxxCategory = 6;  // ContentCategory::XXX
    QString sql = QString("SELECT * FROM torrents WHERE seeders > 0 AND contentCategory != %1 "
                          "ORDER BY seeders DESC LIMIT 0,10").arg(xxxCategory);
    auto results = sphinxql_->query(sql);
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT top torrents failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testSelectTopTorrentsWithTimeFilter()
{
    // Pattern from TorrentDatabase::getTopTorrents() with time filter
    int xxxCategory = 6;
    qint64 cutoff = QDateTime::currentSecsSinceEpoch() - (60 * 60 * 24);  // 24h ago
    
    QString sql = QString("SELECT * FROM torrents WHERE seeders > 0 AND contentCategory != %1 "
                          "AND added > %2 ORDER BY seeders DESC LIMIT 0,10")
        .arg(xxxCategory).arg(cutoff);
    auto results = sphinxql_->query(sql);
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT top torrents with time failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testSelectTopTorrentsWithTypeFilter()
{
    // Pattern from TorrentDatabase::getTopTorrents() with content type filter
    int xxxCategory = 6;
    int videoType = 1;  // contentType for video
    
    QString sql = QString("SELECT * FROM torrents WHERE seeders > 0 AND contentCategory != %1 "
                          "AND contentType = %2 ORDER BY seeders DESC LIMIT 0,10")
        .arg(xxxCategory).arg(videoType);
    auto results = sphinxql_->query(sql);
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT top torrents with type failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testSelectRandomTorrents()
{
    // Pattern from TorrentDatabase::getRandomTorrents()
    int xxxCategory = 6;
    QString sql = QString("SELECT * FROM torrents WHERE seeders > 0 AND contentCategory != %1 "
                          "ORDER BY RAND() LIMIT 5")
        .arg(xxxCategory);
    auto results = sphinxql_->query(sql);
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT RAND() failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testSelectWithInClause()
{
    // Pattern from TorrentDatabase::searchFiles() - IN clause
    QString hash1 = sphinxql_->escapeString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    QString sql = QString("SELECT * FROM torrents WHERE hash IN(%1)").arg(hash1);
    auto results = sphinxql_->query(sql);
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT with IN clause failed: " + sphinxql_->lastError()));
    QCOMPARE(results.size(), 1);
}

void TestManticoreQueries::testSelectFilesFromFiles()
{
    // Pattern from TorrentDatabase::getFilesForTorrent()
    auto results = sphinxql_->query("SELECT * FROM files WHERE hash = ?",
                                    {QString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")});
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT from files failed: " + sphinxql_->lastError()));
    QCOMPARE(results.size(), 1);
}

void TestManticoreQueries::testSelectFromFeed()
{
    // Pattern from FeedManager::load()
    auto results = sphinxql_->query("SELECT * FROM feed LIMIT 100");
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT from feed failed: " + sphinxql_->lastError()));
    QVERIFY(results.size() >= 1);
}

void TestManticoreQueries::testSelectPaginatedById()
{
    // Pattern from RatsAPI replication and MigrationManager batch processing
    QString sql = QString("SELECT * FROM torrents WHERE id > %1 ORDER BY id ASC LIMIT %2")
        .arg(0).arg(100);
    auto results = sphinxql_->query(sql);
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SELECT paginated by id failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testSelectContentTypeMigrationQuery()
{
    // Pattern from MigrationManager::migrateContentTypeBatch()
    QString sql = QString("SELECT id, hash, name FROM torrents "
                          "WHERE contenttype = 0 AND id > %1 ORDER BY id ASC LIMIT %2")
        .arg(0).arg(100);
    auto results = sphinxql_->query(sql);
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("Migration query failed: " + sphinxql_->lastError()));
}

// ============================================================================
// Full-text MATCH search
// ============================================================================

void TestManticoreQueries::testMatchSearch()
{
    // Pattern from TorrentDatabase::searchTorrents() - full text search
    auto results = sphinxql_->query("SELECT * FROM torrents WHERE MATCH(?)",
                                    {QString("Ubuntu")});
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("MATCH search failed: " + sphinxql_->lastError()));
    // Should find our "Test Torrent - Ubuntu 24.04 ISO"
    QVERIFY(results.size() >= 1);
}

void TestManticoreQueries::testMatchSearchWithFilters()
{
    // Pattern from TorrentDatabase::searchTorrents() with safe search and size filters
    int xxxCategory = 6;
    QString sql = QString("SELECT * FROM torrents WHERE MATCH(?) "
                          "AND contentCategory != %1 "
                          "AND size > %2 AND size < %3 AND files > %4")
        .arg(xxxCategory).arg(0).arg(999999999999LL).arg(0);
    auto results = sphinxql_->query(sql, {QString("Ubuntu")});
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("MATCH with filters failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testMatchSearchWithOrderAndLimit()
{
    // Pattern from TorrentDatabase::searchTorrents() - full query with order and limit
    auto results = sphinxql_->query(
        "SELECT * FROM torrents WHERE MATCH(?) ORDER BY seeders DESC LIMIT 0,50",
        {QString("Ubuntu")});
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("MATCH with order/limit failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testMatchFileSearch()
{
    // Pattern from TorrentDatabase::searchFiles() - search in files
    auto results = sphinxql_->query("SELECT * FROM files WHERE MATCH(?) LIMIT 0,50",
                                    {QString("ubuntu")});
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("MATCH files search failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testMatchFileSearchWithSnippet()
{
    // Pattern from TorrentDatabase::searchFiles() - SNIPPET function
    QString sql = "SELECT *, SNIPPET(path, ?, 'around=100', 'force_all_words=1') as snipplet "
                  "FROM files WHERE MATCH(?) LIMIT ?,?";
    auto results = sphinxql_->query(sql, {QString("ubuntu"), QString("ubuntu"), 0, 50});
    QVERIFY2(sphinxql_->lastError().isEmpty(),
             qPrintable("SNIPPET search failed: " + sphinxql_->lastError()));
}

// ============================================================================
// Data integrity verification (from legacy speed.txt tests)
// ============================================================================

void TestManticoreQueries::testInsertFilesAndVerifyFields()
{
    // Legacy test: insert into files, then select by hash and verify field values
    insertTestFiles(100, "dddddddddddddddddddddddddddddddddddddd", "bashaa", "50");
    insertTestFiles(101, "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", "biotu", "30");

    // sphinxql_->flushRamchunk("files");
    
    auto results = sphinxql_->query("SELECT * FROM files WHERE hash = ?",
                                    {QString("dddddddddddddddddddddddddddddddddddddd")});
    QVERIFY2(!results.isEmpty(), "Should find inserted file record");
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0]["size"].toString(), QString("50"));
}

void TestManticoreQueries::testSearchFilesAndVerifyFields()
{
    // Legacy test: MATCH search in files and verify returned hash/size
    auto results = sphinxql_->query("SELECT * FROM files WHERE MATCH(?)",
                                    {QString("bashaa")});
    QVERIFY2(!results.isEmpty(), "MATCH search should find 'bashaa'");
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0]["hash"].toString(), QString("dddddddddddddddddddddddddddddddddddddd"));
    QCOMPARE(results[0]["size"].toString(), QString("50"));
}

void TestManticoreQueries::testBulkInsertFeedAndCount()
{
    // Legacy test: insert 500 records into feed, verify count, then clean up
    sphinxql_->exec("DELETE FROM feed WHERE id > 0");
    
    const int bulkCount = 500;
    for (int i = 1; i <= bulkCount; i++) {
        QVariantMap values;
        values["id"] = static_cast<qint64>(i);
        values["data"] = QString("{\"idx\":%1}").arg(i);
        bool ok = sphinxql_->insertValues("feed", values);
        QVERIFY2(ok, qPrintable(QString("Bulk insert feed item %1 failed: %2")
                                .arg(i).arg(sphinxql_->lastError())));
    }
    
    // Verify count matches
    qint64 count = sphinxql_->getCount("feed");
    QCOMPARE(count, static_cast<qint64>(bulkCount));
    
    // Clean up
    sphinxql_->exec("DELETE FROM feed WHERE id > 0");
    qint64 countAfter = sphinxql_->getCount("feed");
    QCOMPARE(countAfter, static_cast<qint64>(0));
}

void TestManticoreQueries::testInsertFeedJsonAndVerifyReadback()
{
    // Legacy test: insert JSON object into feed, verify it reads back correctly
    QJsonObject obj;
    obj["a"] = 1;
    obj["v"] = 2;
    
    QVariantMap values;
    values["id"] = static_cast<qint64>(3);
    values["data"] = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    
    bool ok = sphinxql_->insertValues("feed", values);
    QVERIFY2(ok, qPrintable("INSERT feed JSON failed: " + sphinxql_->lastError()));
    
    sphinxql_->flushRamchunk("feed");
    
    auto results = queryUntilFieldReady("SELECT * FROM feed WHERE id = 3", {}, "data");
    QVERIFY2(!results.isEmpty(), "Should find inserted feed record");
    
    QString storedData = results[0]["data"].toString();
    QVERIFY2(!storedData.isEmpty(), "data field should not be empty");
    
    QJsonDocument doc = QJsonDocument::fromJson(storedData.toUtf8());
    QVERIFY(doc.isObject());
    QCOMPARE(doc.object()["a"].toInt(), 1);
    QCOMPARE(doc.object()["v"].toInt(), 2);
    
    // Clean up
    sphinxql_->exec("DELETE FROM feed WHERE id > 0");
}

// ============================================================================
// REPLACE operations
// ============================================================================

void TestManticoreQueries::testReplaceIntoStore()
{
    // Pattern from SphinxQL::replaceValues() - used for store table
    QJsonObject data;
    data["type"] = "vote";
    data["vote"] = "bad";
    
    QVariantMap values;
    values["id"] = static_cast<qint64>(1);
    values["storeIndex"] = QString("vote:bbbb");
    values["data"] = QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact));
    values["hash"] = QString("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    values["peerId"] = QString("peer456");
    
    bool ok = sphinxql_->replaceValues("store", values);
    QVERIFY2(ok, qPrintable("REPLACE INTO store failed: " + sphinxql_->lastError()));
}

// ============================================================================
// UPDATE operations
// ============================================================================

void TestManticoreQueries::testUpdateTorrentByHash()
{
    // Pattern from TorrentDatabase::updateTorrent()
    QVariantMap values;
    values["name"] = QString("Updated Torrent Name");
    values["size"] = static_cast<qint64>(2048*1024);
    values["files"] = 2;
    values["contentType"] = 1;
    values["contentCategory"] = 0;
    values["seeders"] = 20;
    values["leechers"] = 3;
    values["completed"] = 150;
    values["good"] = 5;
    values["bad"] = 1;
    
    bool ok = sphinxql_->updateValues("torrents", values,
                                       {{"hash", QString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")}});
    QVERIFY2(ok, qPrintable("UPDATE torrent failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testUpdateTrackerInfo()
{
    // Pattern from TorrentDatabase::updateTrackerInfo()
    QVariantMap values;
    values["seeders"] = 50;
    values["leechers"] = 10;
    values["completed"] = 200;
    values["trackersChecked"] = QDateTime::currentSecsSinceEpoch();
    
    bool ok = sphinxql_->updateValues("torrents", values,
                                       {{"hash", QString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")}});
    QVERIFY2(ok, qPrintable("UPDATE tracker info failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testUpdateContentType()
{
    // Pattern from TorrentDatabase::updateTorrentContentType()
    QVariantMap values;
    values["contentType"] = 2;
    values["contentCategory"] = 1;
    
    bool ok = sphinxql_->updateValues("torrents", values,
                                       {{"hash", QString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")}});
    QVERIFY2(ok, qPrintable("UPDATE content type failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testUpdateInfoField()
{
    // Pattern from TorrentDatabase::updateTorrentInfoField() - JSON info update
    QJsonObject info;
    info["trackers"] = QJsonArray({"udp://tracker1.com:6969", "udp://tracker2.com:6969"});
    info["source"] = "manual";
    
    QVariantMap values;
    values["info"] = QString::fromUtf8(QJsonDocument(info).toJson(QJsonDocument::Compact));
    
    bool ok = sphinxql_->updateValues("torrents", values,
                                       {{"hash", QString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")}});
    QVERIFY2(ok, qPrintable("UPDATE info field failed: " + sphinxql_->lastError()));
    
    // Verify the JSON was stored correctly
    auto results = queryUntilFieldReady("SELECT info FROM torrents WHERE hash = ?",
                                    {QString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")},
                                    "info");
    QVERIFY(!results.isEmpty());
    QString storedInfo = results[0]["info"].toString();
    QVERIFY(!storedInfo.isEmpty());
    QJsonDocument doc = QJsonDocument::fromJson(storedInfo.toUtf8());
    QVERIFY(doc.isObject());
    QVERIFY(doc.object().contains("trackers"));
}

// ============================================================================
// DELETE operations
// ============================================================================

void TestManticoreQueries::testDeleteTorrentByHash()
{
    // Insert a second torrent for deletion test
    insertTestTorrent(2, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", "Delete Me Torrent");
    
    // Pattern from TorrentDatabase::removeTorrent()
    bool ok = sphinxql_->deleteFrom("torrents",
                                     {{"hash", QString("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")}});
    QVERIFY2(ok, qPrintable("DELETE torrent failed: " + sphinxql_->lastError()));
    
    // Verify deletion
    auto results = sphinxql_->query("SELECT id FROM torrents WHERE hash = ?",
                                    {QString("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")});
    QCOMPARE(results.size(), 0);
}

void TestManticoreQueries::testDeleteFilesByHash()
{
    // Insert files for deletion test
    insertTestFiles(2, "cccccccccccccccccccccccccccccccccccccccc", "test.txt", "100");
    
    // Pattern from TorrentDatabase::addFilesToDatabase() - delete before re-insert
    bool ok = sphinxql_->deleteFrom("files",
                                     {{"hash", QString("cccccccccccccccccccccccccccccccccccccccc")}});
    QVERIFY2(ok, qPrintable("DELETE files failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testDeleteFeedAll()
{
    // Pattern from FeedManager::save() and FeedManager::clear()
    bool ok = sphinxql_->exec("DELETE FROM feed WHERE id > 0");
    QVERIFY2(ok, qPrintable("DELETE feed all failed: " + sphinxql_->lastError()));
}

// ============================================================================
// Maintenance operations
// ============================================================================

void TestManticoreQueries::testOptimizeIndex()
{
    // Pattern from SphinxQL::optimize()
    bool ok = sphinxql_->exec("OPTIMIZE INDEX torrents");
    QVERIFY2(ok, qPrintable("OPTIMIZE INDEX torrents failed: " + sphinxql_->lastError()));
    
    ok = sphinxql_->exec("OPTIMIZE INDEX files");
    QVERIFY2(ok, qPrintable("OPTIMIZE INDEX files failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testFlushRamchunk()
{
    // Pattern from SphinxQL::flushRamchunk()
    bool ok = sphinxql_->exec("FLUSH RAMCHUNK torrents");
    QVERIFY2(ok, qPrintable("FLUSH RAMCHUNK torrents failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testTruncateTable()
{
    // Pattern from MigrationManager - clearing feed table
    // Use a non-critical table for testing
    bool ok = sphinxql_->exec("TRUNCATE TABLE version");
    QVERIFY2(ok, qPrintable("TRUNCATE TABLE failed: " + sphinxql_->lastError()));
}

// ============================================================================
// SphinxQL helper methods
// ============================================================================

void TestManticoreQueries::testInsertValues()
{
    // Test the insertValues() high-level method
    QVariantMap values;
    values["id"] = static_cast<qint64>(10);
    values["version"] = 2;
    values["versionIndex"] = QString("v2");
    
    bool ok = sphinxql_->insertValues("version", values);
    QVERIFY2(ok, qPrintable("insertValues failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testReplaceValues()
{
    // Test the replaceValues() high-level method
    QVariantMap values;
    values["id"] = static_cast<qint64>(10);
    values["version"] = 3;
    values["versionIndex"] = QString("v3");
    
    bool ok = sphinxql_->replaceValues("version", values);
    QVERIFY2(ok, qPrintable("replaceValues failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testUpdateValues()
{
    // Test the updateValues() high-level method
    QVariantMap setValues;
    setValues["seeders"] = 99;
    
    QVariantMap where;
    where["hash"] = QString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    
    bool ok = sphinxql_->updateValues("torrents", setValues, where);
    QVERIFY2(ok, qPrintable("updateValues failed: " + sphinxql_->lastError()));
    
    // Verify
    auto results = sphinxql_->query("SELECT seeders FROM torrents WHERE hash = ?",
                                    {QString("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")});
    QVERIFY(!results.isEmpty());
}

void TestManticoreQueries::testDeleteFrom()
{
    // Insert and delete
    QVariantMap insValues;
    insValues["id"] = static_cast<qint64>(20);
    insValues["version"] = 99;
    insValues["versionIndex"] = QString("v99");
    sphinxql_->insertValues("version", insValues);
    
    bool ok = sphinxql_->deleteFrom("version", {{"id", static_cast<qint64>(20)}});
    QVERIFY2(ok, qPrintable("deleteFrom failed: " + sphinxql_->lastError()));
}

void TestManticoreQueries::testGetMaxId()
{
    // Test getMaxId() helper
    qint64 maxId = sphinxql_->getMaxId("torrents");
    QVERIFY(maxId >= 1);
}

void TestManticoreQueries::testGetCount()
{
    // Test getCount() helper
    qint64 count = sphinxql_->getCount("torrents");
    QVERIFY(count >= 1);
    
    // With WHERE clause
    qint64 countFiltered = sphinxql_->getCount("torrents", "seeders > 0");
    QVERIFY(countFiltered >= 0);
}

QTEST_MAIN(TestManticoreQueries)
#include "test_manticore_queries.moc"
