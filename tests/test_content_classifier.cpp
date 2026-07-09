/**
 * @file test_content_classifier.cpp
 * @brief Unit tests for rats::domain::ContentClassifier. Replaces
 *        test_contentdetector.cpp. The classifier loads its extension/bad-word
 *        tables from :/content/*.json; the resources.qrc is compiled into this
 *        test executable (see tests/CMakeLists.txt) so the data is available.
 *
 * Note: ContentType/ContentCategory are compared through toId() because QCOMPARE
 * would otherwise instantiate QTest's stringifier, which resolves (via ADL) to
 * rats::domain::toString(ContentType) — a QString, not the char* QTest needs.
 */

#include <QtTest/QtTest>

#include "domain/content.h"
#include "domain/content_classifier.h"
#include "domain/torrent.h"

using namespace rats::domain;

namespace {
QVector<File> single(const QString& path, qint64 size = 1000)
{
    return QVector<File> { File { path, size } };
}
}

class TestContentClassifier : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();

    // detectTypeFromFiles — single/representative extensions
    void testDetect_video();
    void testDetect_audio();
    void testDetect_pictures();
    void testDetect_books();
    void testDetect_software();
    void testDetect_archive();
    void testDetect_discImages();
    void testDetect_empty();
    void testDetect_unknown();

    // Weighted voting (size-weighted)
    void testWeighted_largeVideoWins();
    void testWeighted_manySmallAudio();
    void testWeighted_mixedVideoWins();

    // Full classify() on a torrent
    void testClassify_videoTorrent();
    void testClassify_musicAlbum();
    void testClassify_softwareWithDocs();

    // Bad-word blocking (only applies to video/pictures/archive types)
    void testBadWords_xxxCategory();
    void testBadWords_badType();
    void testBadWords_caseInsensitive();
    void testBadWords_noMatch();
    void testBadWords_notAppliedToNonMedia();
};

void TestContentClassifier::initTestCase()
{
    // Sanity: the classifier data must actually load from resources. If the
    // resource bundle were missing, every detection would return Unknown.
    QVERIFY2(toId(ContentClassifier::detectTypeFromFiles(single("movie.mkv"))) == toId(ContentType::Video),
        "Content resources (:/content/*.json) not available to test executable");
}

// ============================================================================
// detectTypeFromFiles
// ============================================================================

void TestContentClassifier::testDetect_video()
{
    QVector<File> files { File { "movie.mkv", 4000000000LL }, File { "sub.srt", 100000 } };
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(files)), toId(ContentType::Video));
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(single("clip.mp4"))), toId(ContentType::Video));
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(single("clip.avi"))), toId(ContentType::Video));
}

void TestContentClassifier::testDetect_audio()
{
    QVector<File> files { File { "track01.mp3", 5000000 }, File { "track02.mp3", 5000000 },
        File { "cover.jpg", 100000 } };
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(files)), toId(ContentType::Audio));
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(single("song.flac"))), toId(ContentType::Audio));
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(single("song.ogg"))), toId(ContentType::Audio));
}

void TestContentClassifier::testDetect_pictures()
{
    QVector<File> files { File { "photo1.jpg", 2000000 }, File { "photo2.png", 3000000 } };
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(files)), toId(ContentType::Pictures));
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(single("img.gif"))), toId(ContentType::Pictures));
}

void TestContentClassifier::testDetect_books()
{
    QVector<File> files { File { "ebook.epub", 500000 }, File { "cover.jpg", 50000 } };
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(files)), toId(ContentType::Books));
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(single("doc.pdf"))), toId(ContentType::Books));
}

void TestContentClassifier::testDetect_software()
{
    QVector<File> files { File { "setup.exe", 50000000 }, File { "readme.txt", 5000 } };
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(files)), toId(ContentType::Software));
}

void TestContentClassifier::testDetect_archive()
{
    QCOMPARE(
        toId(ContentClassifier::detectTypeFromFiles(single("archive.zip", 100000000))), toId(ContentType::Archive));
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(single("a.rar"))), toId(ContentType::Archive));
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(single("a.7z"))), toId(ContentType::Archive));
}

void TestContentClassifier::testDetect_discImages()
{
    // Disc images map to archive.
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(single("game.iso"))), toId(ContentType::Archive));
}

void TestContentClassifier::testDetect_empty()
{
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles({})), toId(ContentType::Unknown));
}

void TestContentClassifier::testDetect_unknown()
{
    QVector<File> files { File { "readme", 1000 }, File { "data.xyz", 1000 } };
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(files)), toId(ContentType::Unknown));
}

// ============================================================================
// Weighted voting
// ============================================================================

void TestContentClassifier::testWeighted_largeVideoWins()
{
    QVector<File> files {
        File { "movie.mkv", 4000000000LL },
        File { "sub1.srt", 100000 },
        File { "sub2.srt", 100000 },
        File { "cover.jpg", 500000 },
        File { "nfo.txt", 1000 },
    };
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(files)), toId(ContentType::Video));
}

void TestContentClassifier::testWeighted_manySmallAudio()
{
    QVector<File> files;
    for (int i = 0; i < 20; ++i)
        files.append(File { QString("track%1.mp3").arg(i), 5000000 });
    files.append(File { "cover.jpg", 500000 });
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(files)), toId(ContentType::Audio));
}

void TestContentClassifier::testWeighted_mixedVideoWins()
{
    QVector<File> files {
        File { "song1.mp3", 5000000 },
        File { "song2.mp3", 5000000 },
        File { "song3.mp3", 5000000 },
        File { "movie.mp4", 700000000 },
        File { "cover.jpg", 500000 },
    };
    QCOMPARE(toId(ContentClassifier::detectTypeFromFiles(files)), toId(ContentType::Video));
}

// ============================================================================
// Full classify()
// ============================================================================

void TestContentClassifier::testClassify_videoTorrent()
{
    Torrent t;
    t.name = "The.Movie.2024.1080p.BluRay";
    t.size = 4500000000LL;
    t.fileList.append(File { "The.Movie.2024.1080p.BluRay.mkv", 4400000000LL });
    t.fileList.append(File { "subs/english.srt", 100000 });

    ContentClassifier::classify(t);
    QCOMPARE(toId(t.contentType), toId(ContentType::Video));
    QCOMPARE(toId(t.contentCategory), toId(ContentCategory::Unknown));
}

void TestContentClassifier::testClassify_musicAlbum()
{
    Torrent t;
    t.name = "Artist - Album (2024) [FLAC]";
    for (int i = 1; i <= 12; ++i)
        t.fileList.append(File { QString("Track %1.flac").arg(i), 40000000 });
    t.fileList.append(File { "cover.jpg", 2000000 });

    ContentClassifier::classify(t);
    QCOMPARE(toId(t.contentType), toId(ContentType::Audio));
}

void TestContentClassifier::testClassify_softwareWithDocs()
{
    Torrent t;
    t.name = "Adobe.Photoshop.2024";
    t.fileList.append(File { "Setup.exe", 2500000000LL });
    t.fileList.append(File { "Crack/patch.exe", 400000000 });
    t.fileList.append(File { "readme.txt", 5000 });
    t.fileList.append(File { "install.pdf", 500000 });

    ContentClassifier::classify(t);
    QCOMPARE(toId(t.contentType), toId(ContentType::Software));
}

// ============================================================================
// Bad-word blocking
// ============================================================================

void TestContentClassifier::testBadWords_xxxCategory()
{
    // "porn" is a block word -> XXX category, type stays Video.
    Classification c = ContentClassifier::classify("Some.Porn.Movie.2024", single("movie.mkv"));
    QCOMPARE(toId(c.type), toId(ContentType::Video));
    QCOMPARE(toId(c.category), toId(ContentCategory::XXX));

    // "xxx" token
    c = ContentClassifier::classify("XXX-Video-Collection", single("movie.mkv"));
    QCOMPARE(toId(c.category), toId(ContentCategory::XXX));

    // "hentai" token
    c = ContentClassifier::classify("Anime_Hentai_Collection", single("movie.mkv"));
    QCOMPARE(toId(c.category), toId(ContentCategory::XXX));

    // Russian "порно"
    c = ContentClassifier::classify(QString::fromUtf8("Фильм.порно.2024"), single("movie.mkv"));
    QCOMPARE(toId(c.category), toId(ContentCategory::XXX));
}

void TestContentClassifier::testBadWords_badType()
{
    // A very-bad word marks the whole torrent Bad (terminal).
    Classification c = ContentClassifier::classify("some collection 16yo", single("movie.mkv"));
    QCOMPARE(toId(c.type), toId(ContentType::Bad));

    c = ContentClassifier::classify("Some.PTHC.content", single("photo.jpg"));
    QCOMPARE(toId(c.type), toId(ContentType::Bad));
}

void TestContentClassifier::testBadWords_caseInsensitive()
{
    Classification c = ContentClassifier::classify("Some.PORN.Movie", single("movie.mkv"));
    QCOMPARE(toId(c.category), toId(ContentCategory::XXX));

    c = ContentClassifier::classify("Some.PoRn.Movie", single("movie.mkv"));
    QCOMPARE(toId(c.category), toId(ContentCategory::XXX));

    c = ContentClassifier::classify("Some.PTHC.content", single("movie.mkv"));
    QCOMPARE(toId(c.type), toId(ContentType::Bad));
}

void TestContentClassifier::testBadWords_noMatch()
{
    // Clean video name — no category change.
    Classification c = ContentClassifier::classify("Some.Video.Movie", single("movie.mkv"));
    QCOMPARE(toId(c.type), toId(ContentType::Video));
    QCOMPARE(toId(c.category), toId(ContentCategory::Unknown));
}

void TestContentClassifier::testBadWords_notAppliedToNonMedia()
{
    // Bad-word scanning only runs for Video/Pictures/Archive. A software torrent
    // whose name happens to contain a block word keeps Unknown category.
    Classification c = ContentClassifier::classify("Some.Porn.Utility", single("setup.exe"));
    QCOMPARE(toId(c.type), toId(ContentType::Software));
    QCOMPARE(toId(c.category), toId(ContentCategory::Unknown));
}

QTEST_MAIN(TestContentClassifier)
#include "test_content_classifier.moc"
