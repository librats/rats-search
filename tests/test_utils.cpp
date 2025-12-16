/**
 * @file test_utils.cpp
 * @brief Utility function tests and data structure tests
 */

#include <QtTest/QtTest>
#include <QDateTime>
#include "torrentdatabase.h"

class TestUtils : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    // TorrentInfo copy and assignment tests
    void testTorrentInfoCopy();
    void testTorrentInfoAssignment();
    
    // TorrentInfo with files
    void testTorrentInfoWithFiles();
    
    // SearchOptions customization tests
    void testSearchOptionsCustom();
    
    // Content type detection helpers
    void testContentTypeMapping_allTypes();
    void testContentCategoryMapping_allCategories();
    
    // Edge cases
    void testHashCaseSensitivity();
    void testEmptyTorrentName();
    void testZeroSizeTorrent();
    void testNegativeValues();
    void testDateTimeHandling();
};

void TestUtils::initTestCase()
{
    qDebug() << "Starting Utility tests...";
}

void TestUtils::cleanupTestCase()
{
    qDebug() << "Utility tests completed.";
}

// ============================================================================
// TorrentInfo copy and assignment tests
// ============================================================================

void TestUtils::testTorrentInfoCopy()
{
    TorrentInfo original;
    original.id = 123;
    original.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    original.name = "Test Torrent";
    original.size = 1024 * 1024 * 100;  // 100 MB
    original.seeders = 50;
    original.leechers = 10;
    original.contentType = "video";
    
    // Copy constructor
    TorrentInfo copy(original);
    
    QCOMPARE(copy.id, original.id);
    QCOMPARE(copy.hash, original.hash);
    QCOMPARE(copy.name, original.name);
    QCOMPARE(copy.size, original.size);
    QCOMPARE(copy.seeders, original.seeders);
    QCOMPARE(copy.leechers, original.leechers);
    QCOMPARE(copy.contentType, original.contentType);
}

void TestUtils::testTorrentInfoAssignment()
{
    TorrentInfo original;
    original.id = 456;
    original.hash = "b94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    original.name = "Another Torrent";
    original.size = 1024 * 1024 * 500;
    
    TorrentInfo assigned;
    assigned = original;
    
    QCOMPARE(assigned.id, original.id);
    QCOMPARE(assigned.hash, original.hash);
    QCOMPARE(assigned.name, original.name);
    QCOMPARE(assigned.size, original.size);
}

void TestUtils::testTorrentInfoWithFiles()
{
    TorrentInfo torrent;
    torrent.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    torrent.name = "Multi-file Torrent";
    
    // Add files
    TorrentFile file1;
    file1.path = "folder/file1.txt";
    file1.size = 1024;
    
    TorrentFile file2;
    file2.path = "folder/file2.mp4";
    file2.size = 1024 * 1024 * 100;
    
    torrent.filesList.append(file1);
    torrent.filesList.append(file2);
    torrent.files = torrent.filesList.size();
    
    QCOMPARE(torrent.files, 2);
    QCOMPARE(torrent.filesList.size(), 2);
    QCOMPARE(torrent.filesList[0].path, QString("folder/file1.txt"));
    QCOMPARE(torrent.filesList[1].size, (qint64)(1024 * 1024 * 100));
}

// ============================================================================
// SearchOptions customization tests
// ============================================================================

void TestUtils::testSearchOptionsCustom()
{
    SearchOptions options;
    options.query = "test query";
    options.index = 20;
    options.limit = 50;
    options.orderBy = "seeders";
    options.orderDesc = false;
    options.safeSearch = true;
    options.contentType = "video";
    options.sizeMin = 1024 * 1024;
    options.sizeMax = 1024 * 1024 * 1024;
    options.filesMin = 1;
    options.filesMax = 100;
    
    QCOMPARE(options.query, QString("test query"));
    QCOMPARE(options.index, 20);
    QCOMPARE(options.limit, 50);
    QCOMPARE(options.orderBy, QString("seeders"));
    QVERIFY(!options.orderDesc);
    QVERIFY(options.safeSearch);
    QCOMPARE(options.contentType, QString("video"));
    QCOMPARE(options.sizeMin, (qint64)(1024 * 1024));
    QCOMPARE(options.sizeMax, (qint64)(1024 * 1024 * 1024));
    QCOMPARE(options.filesMin, 1);
    QCOMPARE(options.filesMax, 100);
}

// ============================================================================
// Content type detection helpers
// ============================================================================

void TestUtils::testContentTypeMapping_allTypes()
{
    // Test all content type mappings
    QCOMPARE(TorrentDatabase::contentTypeId("video"), 1);
    QCOMPARE(TorrentDatabase::contentTypeId("audio"), 2);
    QCOMPARE(TorrentDatabase::contentTypeId("books"), 3);
    QCOMPARE(TorrentDatabase::contentTypeId("pictures"), 4);
    QCOMPARE(TorrentDatabase::contentTypeId("software"), 5);
    QCOMPARE(TorrentDatabase::contentTypeId("games"), 6);
    QCOMPARE(TorrentDatabase::contentTypeId("archive"), 7);
    QCOMPARE(TorrentDatabase::contentTypeId("bad"), 100);
    
    // Reverse mapping
    QCOMPARE(TorrentDatabase::contentTypeFromId(1), QString("video"));
    QCOMPARE(TorrentDatabase::contentTypeFromId(2), QString("audio"));
    QCOMPARE(TorrentDatabase::contentTypeFromId(3), QString("books"));
    QCOMPARE(TorrentDatabase::contentTypeFromId(4), QString("pictures"));
    QCOMPARE(TorrentDatabase::contentTypeFromId(5), QString("software"));
    QCOMPARE(TorrentDatabase::contentTypeFromId(6), QString("games"));
    QCOMPARE(TorrentDatabase::contentTypeFromId(7), QString("archive"));
    QCOMPARE(TorrentDatabase::contentTypeFromId(100), QString("bad"));
}

void TestUtils::testContentCategoryMapping_allCategories()
{
    // Test all content category mappings
    QCOMPARE(TorrentDatabase::contentCategoryId("movie"), 1);
    QCOMPARE(TorrentDatabase::contentCategoryId("series"), 2);
    QCOMPARE(TorrentDatabase::contentCategoryId("documentary"), 3);
    QCOMPARE(TorrentDatabase::contentCategoryId("anime"), 4);
    QCOMPARE(TorrentDatabase::contentCategoryId("music"), 5);
    QCOMPARE(TorrentDatabase::contentCategoryId("ebook"), 6);
    QCOMPARE(TorrentDatabase::contentCategoryId("comics"), 7);
    QCOMPARE(TorrentDatabase::contentCategoryId("software"), 8);
    QCOMPARE(TorrentDatabase::contentCategoryId("game"), 9);
    QCOMPARE(TorrentDatabase::contentCategoryId("xxx"), 100);
    
    // Reverse mapping
    QCOMPARE(TorrentDatabase::contentCategoryFromId(1), QString("movie"));
    QCOMPARE(TorrentDatabase::contentCategoryFromId(2), QString("series"));
    QCOMPARE(TorrentDatabase::contentCategoryFromId(3), QString("documentary"));
    QCOMPARE(TorrentDatabase::contentCategoryFromId(4), QString("anime"));
    QCOMPARE(TorrentDatabase::contentCategoryFromId(5), QString("music"));
    QCOMPARE(TorrentDatabase::contentCategoryFromId(6), QString("ebook"));
    QCOMPARE(TorrentDatabase::contentCategoryFromId(7), QString("comics"));
    QCOMPARE(TorrentDatabase::contentCategoryFromId(8), QString("software"));
    QCOMPARE(TorrentDatabase::contentCategoryFromId(9), QString("game"));
    QCOMPARE(TorrentDatabase::contentCategoryFromId(100), QString("xxx"));
}

// ============================================================================
// Edge cases
// ============================================================================

void TestUtils::testHashCaseSensitivity()
{
    TorrentInfo info1;
    info1.hash = "A94A8FE5CCB19BA61C4C0873D391E987982FBBD3";  // Uppercase
    QVERIFY(info1.isValid());
    
    TorrentInfo info2;
    info2.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";  // Lowercase
    QVERIFY(info2.isValid());
    
    // Both should be valid (just checking length)
    QCOMPARE(info1.hash.length(), info2.hash.length());
}

void TestUtils::testEmptyTorrentName()
{
    TorrentInfo info;
    info.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    info.name = "";  // Empty name
    
    // Should still be valid (only hash matters for isValid)
    QVERIFY(info.isValid());
    QVERIFY(info.name.isEmpty());
}

void TestUtils::testZeroSizeTorrent()
{
    TorrentInfo info;
    info.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    info.name = "Zero Size Torrent";
    info.size = 0;
    
    QVERIFY(info.isValid());
    QCOMPARE(info.size, (qint64)0);
}

void TestUtils::testNegativeValues()
{
    TorrentInfo info;
    info.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    info.seeders = -1;  // Invalid but struct allows it
    info.leechers = -1;
    
    // Struct doesn't validate these values
    QCOMPARE(info.seeders, -1);
    QCOMPARE(info.leechers, -1);
}

void TestUtils::testDateTimeHandling()
{
    TorrentInfo info;
    info.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    
    // Test with current date
    info.added = QDateTime::currentDateTime();
    QVERIFY(info.added.isValid());
    
    // Test with specific date
    info.added = QDateTime(QDate(2024, 1, 15), QTime(12, 0, 0));
    QCOMPARE(info.added.date().year(), 2024);
    QCOMPARE(info.added.date().month(), 1);
    QCOMPARE(info.added.date().day(), 15);
    
    // Test with invalid date
    TorrentInfo info2;
    QVERIFY(!info2.added.isValid());
}

QTEST_MAIN(TestUtils)
#include "test_utils.moc"

