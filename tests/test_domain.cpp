/**
 * @file test_domain.cpp
 * @brief Unit tests for the rats::domain Torrent entity, its JSON codec and the
 *        content-enum conversions. Replaces the old test_torrentinfo.cpp.
 */

#include <QDateTime>
#include <QJsonObject>
#include <QtTest/QtTest>

#include "domain/content.h"
#include "domain/torrent.h"
#include "domain/torrent_codec.h"

using namespace rats::domain;

class TestDomain : public QObject {
    Q_OBJECT

private slots:
    // Torrent entity
    void testDefaultConstruction();
    void testIsValid_emptyHash();
    void testIsValid_shortHash();
    void testIsValid_validHash();
    void testIsValid_longHash();
    void testIsValid_caseInsensitive();
    void testIsValid_nonHex();
    void testMagnetLink();
    void testFile();

    // Content enum <-> string
    void testContentTypeToString();
    void testContentTypeFromString();
    void testContentTypeFromString_caseInsensitive();
    void testContentTypeFromString_unknown();
    void testContentCategoryToString();
    void testContentCategoryFromString();

    // Content enum <-> id (numeric values are stored in Manticore columns)
    void testContentTypeIds();
    void testContentCategoryIds();
    void testContentTypeFromId();
    void testContentCategoryFromId();

    // Codec round-trip
    void testCodecToJson_basicFields();
    void testCodecRoundTrip();
    void testCodecRoundTrip_withFiles();
    void testCodecFromJson_legacyInfoHash();
    void testFilesCodecRoundTrip();
};

// ============================================================================
// Torrent entity
// ============================================================================

void TestDomain::testDefaultConstruction()
{
    Torrent t;
    QCOMPARE(t.id, (qint64)0);
    QVERIFY(t.hash.isEmpty());
    QVERIFY(t.name.isEmpty());
    QCOMPARE(t.size, (qint64)0);
    QCOMPARE(t.files, 0);
    QCOMPARE(t.seeders, 0);
    QCOMPARE(t.leechers, 0);
    QCOMPARE(t.completed, 0);
    QCOMPARE(t.good, 0);
    QCOMPARE(t.bad, 0);
    QCOMPARE(toId(t.contentType), toId(ContentType::Unknown));
    QCOMPARE(toId(t.contentCategory), toId(ContentCategory::Unknown));
    QVERIFY(!t.isValid());
}

void TestDomain::testIsValid_emptyHash()
{
    Torrent t;
    t.hash = "";
    QVERIFY(!t.isValid());
}

void TestDomain::testIsValid_shortHash()
{
    Torrent t;
    t.hash = "abc123";
    QVERIFY(!t.isValid());
}

void TestDomain::testIsValid_validHash()
{
    Torrent t;
    t.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    QVERIFY(t.isValid());
    QCOMPARE(t.hash.length(), 40);
}

void TestDomain::testIsValid_longHash()
{
    Torrent t;
    t.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3a";
    QVERIFY(!t.isValid());
}

void TestDomain::testIsValid_caseInsensitive()
{
    Torrent lower;
    lower.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    Torrent upper;
    upper.hash = "A94A8FE5CCB19BA61C4C0873D391E987982FBBD3";
    QVERIFY(lower.isValid());
    QVERIFY(upper.isValid());
}

void TestDomain::testIsValid_nonHex()
{
    Torrent t;
    // 40 chars but contains non-hex characters
    t.hash = "z94a8fe5ccb19ba61c4c0873d391e987982fbbdg";
    QVERIFY(!t.isValid());
}

void TestDomain::testMagnetLink()
{
    Torrent t;
    t.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    t.name = "Test Torrent";
    const QString magnet = t.magnetLink();
    QVERIFY(magnet.startsWith("magnet:?xt=urn:btih:a94a8fe5ccb19ba61c4c0873d391e987982fbbd3"));
    // Name is percent-encoded (space -> %20)
    QVERIFY(magnet.contains("dn=Test%20Torrent"));
}

void TestDomain::testFile()
{
    File f;
    f.path = "/path/to/file.txt";
    f.size = 1024;
    QCOMPARE(f.path, QString("/path/to/file.txt"));
    QCOMPARE(f.size, (qint64)1024);
}

// ============================================================================
// Content enum <-> string
// ============================================================================

void TestDomain::testContentTypeToString()
{
    QCOMPARE(toString(ContentType::Video), QString("video"));
    QCOMPARE(toString(ContentType::Audio), QString("audio"));
    QCOMPARE(toString(ContentType::Books), QString("books"));
    QCOMPARE(toString(ContentType::Pictures), QString("pictures"));
    QCOMPARE(toString(ContentType::Software), QString("software"));
    QCOMPARE(toString(ContentType::Games), QString("games"));
    QCOMPARE(toString(ContentType::Archive), QString("archive"));
    QCOMPARE(toString(ContentType::Bad), QString("bad"));
    QVERIFY(toString(ContentType::Unknown).isEmpty());
}

void TestDomain::testContentTypeFromString()
{
    QCOMPARE(toId(contentTypeFromString("video")), toId(ContentType::Video));
    QCOMPARE(toId(contentTypeFromString("audio")), toId(ContentType::Audio));
    QCOMPARE(toId(contentTypeFromString("books")), toId(ContentType::Books));
    QCOMPARE(toId(contentTypeFromString("pictures")), toId(ContentType::Pictures));
    QCOMPARE(toId(contentTypeFromString("software")), toId(ContentType::Software));
    QCOMPARE(toId(contentTypeFromString("games")), toId(ContentType::Games));
    QCOMPARE(toId(contentTypeFromString("archive")), toId(ContentType::Archive));
    QCOMPARE(toId(contentTypeFromString("bad")), toId(ContentType::Bad));
}

void TestDomain::testContentTypeFromString_caseInsensitive()
{
    QCOMPARE(toId(contentTypeFromString("VIDEO")), toId(ContentType::Video));
    QCOMPARE(toId(contentTypeFromString("Audio")), toId(ContentType::Audio));
}

void TestDomain::testContentTypeFromString_unknown()
{
    QCOMPARE(toId(contentTypeFromString("nonexistent")), toId(ContentType::Unknown));
    QCOMPARE(toId(contentTypeFromString("")), toId(ContentType::Unknown));
}

void TestDomain::testContentCategoryToString()
{
    QCOMPARE(toString(ContentCategory::Movie), QString("movie"));
    QCOMPARE(toString(ContentCategory::Music), QString("music"));
    QCOMPARE(toString(ContentCategory::XXX), QString("xxx"));
    QVERIFY(toString(ContentCategory::Unknown).isEmpty());
}

void TestDomain::testContentCategoryFromString()
{
    QCOMPARE(toId(contentCategoryFromString("movie")), toId(ContentCategory::Movie));
    QCOMPARE(toId(contentCategoryFromString("series")), toId(ContentCategory::Series));
    QCOMPARE(toId(contentCategoryFromString("music")), toId(ContentCategory::Music));
    QCOMPARE(toId(contentCategoryFromString("xxx")), toId(ContentCategory::XXX));
    QCOMPARE(toId(contentCategoryFromString("nope")), toId(ContentCategory::Unknown));
}

// ============================================================================
// Content enum <-> id
// ============================================================================

void TestDomain::testContentTypeIds()
{
    QCOMPARE(toId(ContentType::Unknown), 0);
    QCOMPARE(toId(ContentType::Video), 1);
    QCOMPARE(toId(ContentType::Audio), 2);
    QCOMPARE(toId(ContentType::Books), 3);
    QCOMPARE(toId(ContentType::Pictures), 4);
    QCOMPARE(toId(ContentType::Software), 5);
    QCOMPARE(toId(ContentType::Games), 6);
    QCOMPARE(toId(ContentType::Archive), 7);
    QCOMPARE(toId(ContentType::Bad), 100);
}

void TestDomain::testContentCategoryIds()
{
    QCOMPARE(toId(ContentCategory::Unknown), 0);
    QCOMPARE(toId(ContentCategory::Movie), 1);
    QCOMPARE(toId(ContentCategory::Series), 2);
    QCOMPARE(toId(ContentCategory::Documentary), 3);
    QCOMPARE(toId(ContentCategory::Anime), 4);
    QCOMPARE(toId(ContentCategory::Music), 5);
    QCOMPARE(toId(ContentCategory::Ebook), 6);
    QCOMPARE(toId(ContentCategory::Comics), 7);
    QCOMPARE(toId(ContentCategory::Software), 8);
    QCOMPARE(toId(ContentCategory::Game), 9);
    QCOMPARE(toId(ContentCategory::XXX), 100);
}

void TestDomain::testContentTypeFromId()
{
    QCOMPARE(toId(contentTypeFromId(1)), toId(ContentType::Video));
    QCOMPARE(toId(contentTypeFromId(7)), toId(ContentType::Archive));
    QCOMPARE(toId(contentTypeFromId(100)), toId(ContentType::Bad));
    QCOMPARE(toId(contentTypeFromId(999)), toId(ContentType::Unknown));
}

void TestDomain::testContentCategoryFromId()
{
    QCOMPARE(toId(contentCategoryFromId(1)), toId(ContentCategory::Movie));
    QCOMPARE(toId(contentCategoryFromId(5)), toId(ContentCategory::Music));
    QCOMPARE(toId(contentCategoryFromId(100)), toId(ContentCategory::XXX));
    QCOMPARE(toId(contentCategoryFromId(999)), toId(ContentCategory::Unknown));
}

// ============================================================================
// Codec round-trip
// ============================================================================

void TestDomain::testCodecToJson_basicFields()
{
    Torrent t;
    t.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    t.name = "Some Torrent";
    t.size = 123456;
    t.files = 3;
    t.seeders = 12;
    t.leechers = 4;
    t.contentType = ContentType::Video;
    t.contentCategory = ContentCategory::Movie;

    const QJsonObject obj = codec::toJson(t);
    QCOMPARE(obj["hash"].toString(), t.hash);
    QCOMPARE(obj["name"].toString(), t.name);
    QCOMPARE(obj["size"].toVariant().toLongLong(), t.size);
    QCOMPARE(obj["files"].toInt(), 3);
    QCOMPARE(obj["seeders"].toInt(), 12);
    QCOMPARE(obj["contentType"].toString(), QString("video"));
    QCOMPARE(obj["contentCategory"].toString(), QString("movie"));
}

void TestDomain::testCodecRoundTrip()
{
    Torrent original;
    original.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    original.name = "Round Trip Torrent";
    original.size = 999888777;
    original.files = 5;
    original.pieceLength = 262144;
    original.seeders = 50;
    original.leechers = 10;
    original.completed = 7;
    original.good = 3;
    original.bad = 1;
    original.contentType = ContentType::Audio;
    original.contentCategory = ContentCategory::Music;
    original.added = QDateTime::fromMSecsSinceEpoch(1700000000000LL);

    const Torrent restored = codec::torrentFromJson(codec::toJson(original));

    QCOMPARE(restored.hash, original.hash);
    QCOMPARE(restored.name, original.name);
    QCOMPARE(restored.size, original.size);
    QCOMPARE(restored.files, original.files);
    QCOMPARE(restored.pieceLength, original.pieceLength);
    QCOMPARE(restored.seeders, original.seeders);
    QCOMPARE(restored.leechers, original.leechers);
    QCOMPARE(restored.completed, original.completed);
    QCOMPARE(restored.good, original.good);
    QCOMPARE(restored.bad, original.bad);
    QCOMPARE(toId(restored.contentType), toId(original.contentType));
    QCOMPARE(toId(restored.contentCategory), toId(original.contentCategory));
    QCOMPARE(restored.added, original.added);
}

void TestDomain::testCodecRoundTrip_withFiles()
{
    Torrent original;
    original.hash = "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3";
    original.name = "Files Torrent";
    original.fileList.append(File { "folder/a.mkv", 4000000000LL });
    original.fileList.append(File { "folder/b.srt", 1024 });
    original.files = original.fileList.size();

    codec::ToJsonOptions opts;
    opts.includeFiles = true;
    const QJsonObject obj = codec::toJson(original, opts);
    QVERIFY(obj.contains("files_list"));

    const Torrent restored = codec::torrentFromJson(obj);
    QCOMPARE(restored.fileList.size(), 2);
    QCOMPARE(restored.fileList[0].path, QString("folder/a.mkv"));
    QCOMPARE(restored.fileList[0].size, (qint64)4000000000LL);
    QCOMPARE(restored.fileList[1].path, QString("folder/b.srt"));
    QCOMPARE(restored.fileList[1].size, (qint64)1024);
}

void TestDomain::testCodecFromJson_legacyInfoHash()
{
    // The tolerant parser accepts the legacy "info_hash" key.
    QJsonObject obj;
    obj["info_hash"] = "A94A8FE5CCB19BA61C4C0873D391E987982FBBD3";
    obj["name"] = "Legacy";
    const Torrent t = codec::torrentFromJson(obj);
    // Hash is normalised to lower case.
    QCOMPARE(t.hash, QString("a94a8fe5ccb19ba61c4c0873d391e987982fbbd3"));
    QVERIFY(t.isValid());
}

void TestDomain::testFilesCodecRoundTrip()
{
    QVector<File> files;
    files.append(File { "x/y.txt", 10 });
    files.append(File { "z.bin", 20 });
    const QVector<File> restored = codec::filesFromJson(codec::filesToJson(files));
    QCOMPARE(restored.size(), 2);
    QCOMPARE(restored[0].path, QString("x/y.txt"));
    QCOMPARE(restored[1].size, (qint64)20);
}

QTEST_MAIN(TestDomain)
#include "test_domain.moc"
