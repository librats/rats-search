/**
 * @file test_torrentinfo.cpp
 * @brief Unit tests for TorrentInfo struct and related functionality
 */

#include <QtTest/QtTest>
#include <QDateTime>
#include "torrentdatabase.h"

class TestTorrentInfo : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    // TorrentInfo validation tests
    void testDefaultConstruction();
    void testIsValid_emptyHash();
    void testIsValid_shortHash();
    void testIsValid_validHash();
    void testIsValid_longHash();
    
    // Content type tests
    void testContentTypeId_video();
    void testContentTypeId_audio();
    void testContentTypeId_unknown();
    void testContentTypeFromId();
    
    // Content category tests
    void testContentCategoryId_movie();
    void testContentCategoryId_music();
    void testContentCategoryFromId();
    
    // SearchOptions tests
    void testSearchOptionsDefaults();
    
    // TorrentFile tests
    void testTorrentFile();
};

void TestTorrentInfo::initTestCase()
{
    qDebug() << "Starting TorrentInfo tests...";
}

void TestTorrentInfo::cleanupTestCase()
{
    qDebug() << "TorrentInfo tests completed.";
}

// ============================================================================
// TorrentInfo validation tests
// ============================================================================

void TestTorrentInfo::testDefaultConstruction()
{
    TorrentInfo info;
    
    QCOMPARE(info.id, (qint64)0);
    QVERIFY(info.hash.isEmpty());
    QVERIFY(info.name.isEmpty());
    QCOMPARE(info.size, (qint64)0);
    QCOMPARE(info.files, 0);
    QCOMPARE(info.seeders, 0);
    QCOMPARE(info.leechers, 0);
    QCOMPARE(info.completed, 0);
    QCOMPARE(info.good, 0);
    QCOMPARE(info.bad, 0);
    QVERIFY(!info.isValid());
}

void TestTorrentInfo::testIsValid_emptyHash()
{
    TorrentInfo info;
    info.hash = "";
    QVERIFY(!info.isValid());
}

void TestTorrentInfo::testIsValid_shortHash()
{
    TorrentInfo info;
    info.hash = "abc123";  // Too short (should be 40 chars)
    QVERIFY(!info.isValid());
}

void TestTorrentInfo::testIsValid_validHash()
{
    TorrentInfo info;
    // Valid 40-character hex hash
    info.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    QVERIFY(info.isValid());
    QCOMPARE(info.hash.length(), 40);
}

void TestTorrentInfo::testIsValid_longHash()
{
    TorrentInfo info;
    // Too long (41 characters)
    info.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3a";
    QVERIFY(!info.isValid());
}

// ============================================================================
// Content type tests
// ============================================================================

void TestTorrentInfo::testContentTypeId_video()
{
    int id = TorrentDatabase::contentTypeId("video");
    QCOMPARE(id, static_cast<int>(ContentType::Video));
}

void TestTorrentInfo::testContentTypeId_audio()
{
    int id = TorrentDatabase::contentTypeId("audio");
    QCOMPARE(id, static_cast<int>(ContentType::Audio));
}

void TestTorrentInfo::testContentTypeId_unknown()
{
    int id = TorrentDatabase::contentTypeId("nonexistent");
    QCOMPARE(id, static_cast<int>(ContentType::Unknown));
}

void TestTorrentInfo::testContentTypeFromId()
{
    QString video = TorrentDatabase::contentTypeFromId(static_cast<int>(ContentType::Video));
    QCOMPARE(video, QString("video"));
    
    QString audio = TorrentDatabase::contentTypeFromId(static_cast<int>(ContentType::Audio));
    QCOMPARE(audio, QString("audio"));
    
    QString unknown = TorrentDatabase::contentTypeFromId(999);
    QCOMPARE(unknown, QString("unknown"));
}

// ============================================================================
// Content category tests
// ============================================================================

void TestTorrentInfo::testContentCategoryId_movie()
{
    int id = TorrentDatabase::contentCategoryId("movie");
    QCOMPARE(id, static_cast<int>(ContentCategory::Movie));
}

void TestTorrentInfo::testContentCategoryId_music()
{
    int id = TorrentDatabase::contentCategoryId("music");
    QCOMPARE(id, static_cast<int>(ContentCategory::Music));
}

void TestTorrentInfo::testContentCategoryFromId()
{
    QString movie = TorrentDatabase::contentCategoryFromId(static_cast<int>(ContentCategory::Movie));
    QCOMPARE(movie, QString("movie"));
    
    QString music = TorrentDatabase::contentCategoryFromId(static_cast<int>(ContentCategory::Music));
    QCOMPARE(music, QString("music"));
}

// ============================================================================
// SearchOptions tests
// ============================================================================

void TestTorrentInfo::testSearchOptionsDefaults()
{
    SearchOptions options;
    
    QVERIFY(options.query.isEmpty());
    QCOMPARE(options.index, 0);
    QCOMPARE(options.limit, 10);
    QVERIFY(options.orderBy.isEmpty());
    QVERIFY(options.orderDesc);
    QVERIFY(!options.safeSearch);
    QCOMPARE(options.sizeMin, (qint64)0);
    QCOMPARE(options.sizeMax, (qint64)0);
    QCOMPARE(options.filesMin, 0);
    QCOMPARE(options.filesMax, 0);
}

// ============================================================================
// TorrentFile tests
// ============================================================================

void TestTorrentInfo::testTorrentFile()
{
    TorrentFile file;
    file.path = "/path/to/file.txt";
    file.size = 1024;
    
    QCOMPARE(file.path, QString("/path/to/file.txt"));
    QCOMPARE(file.size, (qint64)1024);
}

QTEST_MAIN(TestTorrentInfo)
#include "test_torrentinfo.moc"

