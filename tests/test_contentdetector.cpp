/**
 * @file test_contentdetector.cpp
 * @brief Unit tests for ContentDetector - verifies parity with legacy content.js
 */

#include <QtTest/QtTest>
#include "contentdetector.h"
#include "torrentdatabase.h"

class TestContentDetector : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    // Content type ID conversion tests
    void testContentTypeId_allTypes();
    void testContentTypeFromId_allTypes();
    void testContentTypeId_caseInsensitive();
    
    // Content category ID conversion tests
    void testContentCategoryId_xxx();
    void testContentCategoryFromId();
    
    // Extension-based file type detection tests
    void testDetectFromFiles_video();
    void testDetectFromFiles_audio();
    void testDetectFromFiles_pictures();
    void testDetectFromFiles_books();
    void testDetectFromFiles_software();
    void testDetectFromFiles_archive();
    void testDetectFromFiles_empty();
    void testDetectFromFiles_unknown();
    
    // Weighted detection tests (legacy parity)
    void testWeightedDetection_largeVideoFile();
    void testWeightedDetection_manySmallFiles();
    void testWeightedDetection_mixedContent();
    
    // Full torrent detection tests
    void testDetectContentType_videoTorrent();
    void testDetectContentType_musicAlbum();
    void testDetectContentType_softwareWithDocs();
    
    // Adult content detection tests
    void testIsAdultContent_positive();
    void testIsAdultContent_negative();
    void testDetectContentType_adultInName();
    void testDetectContentType_adultInFileName();
    
    // Disc images (mapped to archive)
    void testDetectFromFiles_discImages();
};

void TestContentDetector::initTestCase()
{
    qDebug() << "Starting ContentDetector tests...";
}

void TestContentDetector::cleanupTestCase()
{
    qDebug() << "ContentDetector tests completed.";
}

// ============================================================================
// Content type ID conversion tests
// ============================================================================

void TestContentDetector::testContentTypeId_allTypes()
{
    // Verify all type mappings
    QCOMPARE(ContentDetector::contentTypeId("video"), 1);
    QCOMPARE(ContentDetector::contentTypeId("audio"), 2);
    QCOMPARE(ContentDetector::contentTypeId("books"), 3);
    QCOMPARE(ContentDetector::contentTypeId("pictures"), 4);
    QCOMPARE(ContentDetector::contentTypeId("software"), 5);
    QCOMPARE(ContentDetector::contentTypeId("games"), 6);
    QCOMPARE(ContentDetector::contentTypeId("archive"), 7);
    QCOMPARE(ContentDetector::contentTypeId("bad"), 100);
    
    // Unknown type should return 0
    QCOMPARE(ContentDetector::contentTypeId("nonexistent"), 0);
    QCOMPARE(ContentDetector::contentTypeId(""), 0);
}

void TestContentDetector::testContentTypeFromId_allTypes()
{
    QCOMPARE(ContentDetector::contentTypeFromId(1), QString("video"));
    QCOMPARE(ContentDetector::contentTypeFromId(2), QString("audio"));
    QCOMPARE(ContentDetector::contentTypeFromId(3), QString("books"));
    QCOMPARE(ContentDetector::contentTypeFromId(4), QString("pictures"));
    QCOMPARE(ContentDetector::contentTypeFromId(5), QString("software"));
    QCOMPARE(ContentDetector::contentTypeFromId(6), QString("games"));
    QCOMPARE(ContentDetector::contentTypeFromId(7), QString("archive"));
    QCOMPARE(ContentDetector::contentTypeFromId(100), QString("bad"));
    
    // Unknown ID should return "unknown"
    QCOMPARE(ContentDetector::contentTypeFromId(0), QString("unknown"));
    QCOMPARE(ContentDetector::contentTypeFromId(999), QString("unknown"));
}

void TestContentDetector::testContentTypeId_caseInsensitive()
{
    QCOMPARE(ContentDetector::contentTypeId("VIDEO"), 1);
    QCOMPARE(ContentDetector::contentTypeId("Video"), 1);
    QCOMPARE(ContentDetector::contentTypeId("AUDIO"), 2);
    QCOMPARE(ContentDetector::contentTypeId("Pictures"), 4);
}

// ============================================================================
// Content category ID conversion tests
// ============================================================================

void TestContentDetector::testContentCategoryId_xxx()
{
    QCOMPARE(ContentDetector::contentCategoryId("xxx"), 100);
    QCOMPARE(ContentDetector::contentCategoryId("XXX"), 100);
}

void TestContentDetector::testContentCategoryFromId()
{
    QCOMPARE(ContentDetector::contentCategoryFromId(100), QString("xxx"));
    QCOMPARE(ContentDetector::contentCategoryFromId(1), QString("movie"));
    QCOMPARE(ContentDetector::contentCategoryFromId(0), QString("unknown"));
}

// ============================================================================
// Extension-based file type detection tests
// ============================================================================

void TestContentDetector::testDetectFromFiles_video()
{
    QVector<TorrentFile> files;
    files.append({"Movie.mkv", 1500000000});  // 1.5GB video
    files.append({"subs.srt", 50000});         // Subtitle file (unknown type)
    
    int typeId = ContentDetector::detectContentTypeFromFiles(files);
    QCOMPARE(typeId, static_cast<int>(ContentType::Video));
    
    // Test various video extensions
    QVector<TorrentFile> videoFiles = {
        {"test.mp4", 1000}, {"test.avi", 1000}, {"test.mkv", 1000},
        {"test.mov", 1000}, {"test.wmv", 1000}, {"test.flv", 1000},
        {"test.m4v", 1000}, {"test.webm", 1000}, {"test.m2ts", 1000}
    };
    
    for (const auto& file : videoFiles) {
        QVector<TorrentFile> single = {file};
        int id = ContentDetector::detectContentTypeFromFiles(single);
        QCOMPARE(id, static_cast<int>(ContentType::Video));
    }
}

void TestContentDetector::testDetectFromFiles_audio()
{
    QVector<TorrentFile> files;
    files.append({"track01.mp3", 5000000});
    files.append({"track02.mp3", 5000000});
    files.append({"cover.jpg", 100000});  // Small image
    
    int typeId = ContentDetector::detectContentTypeFromFiles(files);
    QCOMPARE(typeId, static_cast<int>(ContentType::Audio));
    
    // Test various audio extensions
    QVector<TorrentFile> audioFiles = {
        {"test.mp3", 1000}, {"test.flac", 1000}, {"test.wav", 1000},
        {"test.ogg", 1000}, {"test.aac", 1000}, {"test.m4a", 1000},
        {"test.opus", 1000}, {"test.wma", 1000}, {"test.ac3", 1000}
    };
    
    for (const auto& file : audioFiles) {
        QVector<TorrentFile> single = {file};
        int id = ContentDetector::detectContentTypeFromFiles(single);
        QCOMPARE(id, static_cast<int>(ContentType::Audio));
    }
}

void TestContentDetector::testDetectFromFiles_pictures()
{
    QVector<TorrentFile> files;
    files.append({"photo1.jpg", 2000000});
    files.append({"photo2.png", 3000000});
    files.append({"photo3.gif", 1000000});
    
    int typeId = ContentDetector::detectContentTypeFromFiles(files);
    QCOMPARE(typeId, static_cast<int>(ContentType::Pictures));
    
    // Test various image extensions
    QVector<TorrentFile> imageFiles = {
        {"test.jpg", 1000}, {"test.png", 1000}, {"test.gif", 1000},
        {"test.bmp", 1000}, {"test.webp", 1000}, {"test.psd", 1000},
        {"test.svg", 1000}, {"test.tiff", 1000}, {"test.avif", 1000}
    };
    
    for (const auto& file : imageFiles) {
        QVector<TorrentFile> single = {file};
        int id = ContentDetector::detectContentTypeFromFiles(single);
        QCOMPARE(id, static_cast<int>(ContentType::Pictures));
    }
}

void TestContentDetector::testDetectFromFiles_books()
{
    QVector<TorrentFile> files;
    files.append({"ebook.epub", 500000});
    files.append({"cover.jpg", 50000});
    
    int typeId = ContentDetector::detectContentTypeFromFiles(files);
    QCOMPARE(typeId, static_cast<int>(ContentType::Books));
    
    // Test various book extensions
    QVector<TorrentFile> bookFiles = {
        {"test.pdf", 1000}, {"test.epub", 1000}, {"test.mobi", 1000},
        {"test.fb2", 1000}, {"test.djvu", 1000}, {"test.azw3", 1000},
        {"test.cbr", 1000}, {"test.cbz", 1000}, {"test.doc", 1000}
    };
    
    for (const auto& file : bookFiles) {
        QVector<TorrentFile> single = {file};
        int id = ContentDetector::detectContentTypeFromFiles(single);
        QCOMPARE(id, static_cast<int>(ContentType::Books));
    }
}

void TestContentDetector::testDetectFromFiles_software()
{
    QVector<TorrentFile> files;
    files.append({"setup.exe", 50000000});
    files.append({"readme.txt", 5000});
    
    int typeId = ContentDetector::detectContentTypeFromFiles(files);
    QCOMPARE(typeId, static_cast<int>(ContentType::Software));
    
    // Test various software extensions
    QVector<TorrentFile> softwareFiles = {
        {"test.exe", 1000}, {"test.msi", 1000}, {"test.dmg", 1000},
        {"test.apk", 1000}, {"test.deb", 1000}, {"test.rpm", 1000},
        {"test.jar", 1000}, {"test.dll", 1000}, {"test.ipa", 1000}
    };
    
    for (const auto& file : softwareFiles) {
        QVector<TorrentFile> single = {file};
        int id = ContentDetector::detectContentTypeFromFiles(single);
        QCOMPARE(id, static_cast<int>(ContentType::Software));
    }
}

void TestContentDetector::testDetectFromFiles_archive()
{
    QVector<TorrentFile> files;
    files.append({"archive.zip", 100000000});
    
    int typeId = ContentDetector::detectContentTypeFromFiles(files);
    QCOMPARE(typeId, static_cast<int>(ContentType::Archive));
    
    // Test various archive extensions
    QVector<TorrentFile> archiveFiles = {
        {"test.zip", 1000}, {"test.rar", 1000}, {"test.7z", 1000},
        {"test.tar", 1000}, {"test.gz", 1000}, {"test.bz2", 1000},
        {"test.xz", 1000}, {"test.tgz", 1000}
    };
    
    for (const auto& file : archiveFiles) {
        QVector<TorrentFile> single = {file};
        int id = ContentDetector::detectContentTypeFromFiles(single);
        QCOMPARE(id, static_cast<int>(ContentType::Archive));
    }
}

void TestContentDetector::testDetectFromFiles_discImages()
{
    // Disc images should be detected as archive (mapped for compatibility)
    QVector<TorrentFile> discFiles = {
        {"game.iso", 1000}, {"image.mdf", 1000}, {"disc.nrg", 1000},
        {"backup.vhd", 1000}, {"system.wim", 1000}
    };
    
    for (const auto& file : discFiles) {
        QVector<TorrentFile> single = {file};
        int id = ContentDetector::detectContentTypeFromFiles(single);
        QCOMPARE(id, static_cast<int>(ContentType::Archive));
    }
}

void TestContentDetector::testDetectFromFiles_empty()
{
    QVector<TorrentFile> files;
    int typeId = ContentDetector::detectContentTypeFromFiles(files);
    QCOMPARE(typeId, static_cast<int>(ContentType::Unknown));
}

void TestContentDetector::testDetectFromFiles_unknown()
{
    QVector<TorrentFile> files;
    files.append({"readme", 1000});      // No extension
    files.append({"data.xyz", 1000});    // Unknown extension
    
    int typeId = ContentDetector::detectContentTypeFromFiles(files);
    QCOMPARE(typeId, static_cast<int>(ContentType::Unknown));
}

// ============================================================================
// Weighted detection tests (legacy parity)
// ============================================================================

void TestContentDetector::testWeightedDetection_largeVideoFile()
{
    // Like legacy: typesPriority[type] += file.size / torrent.size
    // Large video file should win even if there are many small files
    QVector<TorrentFile> files;
    files.append({"movie.mkv", 4000000000LL});  // 4GB video
    files.append({"sub1.srt", 100000});
    files.append({"sub2.srt", 100000});
    files.append({"cover.jpg", 500000});
    files.append({"nfo.txt", 1000});
    
    int typeId = ContentDetector::detectContentTypeFromFiles(files);
    QCOMPARE(typeId, static_cast<int>(ContentType::Video));
}

void TestContentDetector::testWeightedDetection_manySmallFiles()
{
    // Many small audio files should detect as audio
    QVector<TorrentFile> files;
    for (int i = 0; i < 20; i++) {
        files.append({QString("track%1.mp3").arg(i), 5000000});  // 5MB each
    }
    files.append({"cover.jpg", 500000});
    files.append({"playlist.m3u", 1000});
    
    int typeId = ContentDetector::detectContentTypeFromFiles(files);
    QCOMPARE(typeId, static_cast<int>(ContentType::Audio));
}

void TestContentDetector::testWeightedDetection_mixedContent()
{
    // Weighted detection: video should win because of larger size
    QVector<TorrentFile> files;
    files.append({"song1.mp3", 5000000});      // 5MB audio
    files.append({"song2.mp3", 5000000});      // 5MB audio
    files.append({"song3.mp3", 5000000});      // 5MB audio
    files.append({"movie.mp4", 700000000});    // 700MB video
    files.append({"cover.jpg", 500000});       // 0.5MB image
    
    int typeId = ContentDetector::detectContentTypeFromFiles(files);
    QCOMPARE(typeId, static_cast<int>(ContentType::Video));
}

// ============================================================================
// Full torrent detection tests
// ============================================================================

void TestContentDetector::testDetectContentType_videoTorrent()
{
    TorrentInfo torrent;
    torrent.name = "The.Movie.2024.1080p.BluRay";
    torrent.size = 4500000000LL;
    torrent.filesList.append({"The.Movie.2024.1080p.BluRay.mkv", 4400000000LL});
    torrent.filesList.append({"subs/english.srt", 100000});
    
    ContentDetector::detectContentType(torrent);
    
    QCOMPARE(torrent.contentTypeId, static_cast<int>(ContentType::Video));
    QCOMPARE(torrent.contentType, QString("video"));
    QCOMPARE(torrent.contentCategoryId, static_cast<int>(ContentCategory::Unknown));
}

void TestContentDetector::testDetectContentType_musicAlbum()
{
    TorrentInfo torrent;
    torrent.name = "Artist - Album (2024) [FLAC]";
    torrent.size = 500000000;
    
    for (int i = 1; i <= 12; i++) {
        torrent.filesList.append({QString("Track %1.flac").arg(i), 40000000});
    }
    torrent.filesList.append({"cover.jpg", 2000000});
    
    ContentDetector::detectContentType(torrent);
    
    QCOMPARE(torrent.contentTypeId, static_cast<int>(ContentType::Audio));
    QCOMPARE(torrent.contentType, QString("audio"));
}

void TestContentDetector::testDetectContentType_softwareWithDocs()
{
    TorrentInfo torrent;
    torrent.name = "Adobe.Photoshop.2024";
    torrent.size = 3000000000LL;
    torrent.filesList.append({"Setup.exe", 2500000000LL});
    torrent.filesList.append({"Crack/patch.exe", 400000000});
    torrent.filesList.append({"readme.txt", 5000});
    torrent.filesList.append({"install.pdf", 500000});
    
    ContentDetector::detectContentType(torrent);
    
    QCOMPARE(torrent.contentTypeId, static_cast<int>(ContentType::Software));
    QCOMPARE(torrent.contentType, QString("software"));
}

// ============================================================================
// Adult content detection tests
// ============================================================================

void TestContentDetector::testIsAdultContent_positive()
{
    QVERIFY(ContentDetector::isAdultContent("XXX Video Collection"));
    QVERIFY(ContentDetector::isAdultContent("Some Porn Movie"));
    QVERIFY(ContentDetector::isAdultContent("Adult Content Pack"));
    QVERIFY(ContentDetector::isAdultContent("18+ Compilation"));
    QVERIFY(ContentDetector::isAdultContent("NSFW Photos"));
    QVERIFY(ContentDetector::isAdultContent("Hentai Collection"));
}

void TestContentDetector::testIsAdultContent_negative()
{
    QVERIFY(!ContentDetector::isAdultContent("The Matrix 1999"));
    QVERIFY(!ContentDetector::isAdultContent("Ubuntu 24.04 LTS"));
    QVERIFY(!ContentDetector::isAdultContent("Classical Music Album"));
    QVERIFY(!ContentDetector::isAdultContent("Documentary About Nature"));
}

void TestContentDetector::testDetectContentType_adultInName()
{
    TorrentInfo torrent;
    torrent.name = "XXX Adult Collection";
    torrent.size = 5000000000LL;
    torrent.filesList.append({"video1.mp4", 2500000000LL});
    torrent.filesList.append({"video2.mp4", 2500000000LL});
    
    ContentDetector::detectContentType(torrent);
    
    QCOMPARE(torrent.contentTypeId, static_cast<int>(ContentType::Video));
    QCOMPARE(torrent.contentCategoryId, static_cast<int>(ContentCategory::XXX));
    QCOMPARE(torrent.contentCategory, QString("xxx"));
}

void TestContentDetector::testDetectContentType_adultInFileName()
{
    TorrentInfo torrent;
    torrent.name = "Video Collection";  // Clean name
    torrent.size = 5000000000LL;
    torrent.filesList.append({"xxx_video1.mp4", 2500000000LL});  // Adult keyword in filename
    torrent.filesList.append({"regular_video.mp4", 2500000000LL});
    
    ContentDetector::detectContentType(torrent);
    
    QCOMPARE(torrent.contentTypeId, static_cast<int>(ContentType::Video));
    QCOMPARE(torrent.contentCategoryId, static_cast<int>(ContentCategory::XXX));
}

QTEST_MAIN(TestContentDetector)
#include "test_contentdetector.moc"
