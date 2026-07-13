/**
 * @file test_filter_policy.cpp
 * @brief Unit tests for rats::service::FilterPolicy — the indexing gate that
 *        decides whether a torrent is allowed into the index. Covers size, file
 *        count, adult, naming-regex and (in particular) the content-type filter,
 *        whose "application" umbrella token must accept Software + Games to stay
 *        consistent with the UI checkboxes and data::TorrentRepository.
 */

#include <QtTest/QtTest>

#include "domain/content.h"
#include "domain/torrent.h"
#include "services/filter_policy.h"

using namespace rats::domain;
using namespace rats::service;

namespace {

Torrent makeTorrent(const QString& name, ContentType type)
{
    Torrent t;
    t.hash = QStringLiteral("0123456789abcdef0123456789abcdef01234567");
    t.name = name;
    t.size = 100;
    t.files = 1;
    t.contentType = type;
    return t;
}

} // namespace

class TestFilterPolicy : public QObject {
    Q_OBJECT

private slots:
    void testEmptyFilterAcceptsEverything();
    void testAllFilterAcceptsEverything();
    void testContentTypeVideoAllowed();
    void testContentTypeVideoRejectsAudio();
    void testApplicationUmbrellaAcceptsSoftware();
    void testApplicationUmbrellaAcceptsGames();
    void testApplicationUmbrellaRejectsVideo();
    void testDiscTokenIsInertNoOp();
    void testWhitespaceAndCaseTolerant();
    void testSizeLimits();
    void testFileCountLimit();
    void testAdultFilter();
    void testNamingRegExp();
};

void TestFilterPolicy::testEmptyFilterAcceptsEverything()
{
    FilterPolicy policy; // default settings, contentTypeFilter empty
    QVERIFY(policy.accepts(makeTorrent("anything", ContentType::Software)));
    QVERIFY(policy.accepts(makeTorrent("anything", ContentType::Unknown)));
}

void TestFilterPolicy::testAllFilterAcceptsEverything()
{
    FilterSettings s;
    s.contentTypeFilter = QStringLiteral("all");
    FilterPolicy policy(s);
    QVERIFY(policy.accepts(makeTorrent("clip", ContentType::Video)));
    QVERIFY(policy.accepts(makeTorrent("game", ContentType::Games)));
}

void TestFilterPolicy::testContentTypeVideoAllowed()
{
    FilterSettings s;
    s.contentTypeFilter = QStringLiteral("video,audio");
    FilterPolicy policy(s);
    QVERIFY(policy.accepts(makeTorrent("movie", ContentType::Video)));
    QVERIFY(policy.accepts(makeTorrent("song", ContentType::Audio)));
}

void TestFilterPolicy::testContentTypeVideoRejectsAudio()
{
    FilterSettings s;
    s.contentTypeFilter = QStringLiteral("video");
    FilterPolicy policy(s);
    QVERIFY(!policy.accepts(makeTorrent("song", ContentType::Audio)));
}

// Regression: the UI emits the umbrella token "application" for the "Apps/Games"
// checkbox. Software torrents must pass when it is present.
void TestFilterPolicy::testApplicationUmbrellaAcceptsSoftware()
{
    FilterSettings s;
    s.contentTypeFilter = QStringLiteral("application");
    FilterPolicy policy(s);
    QVERIFY(policy.accepts(makeTorrent("some app", ContentType::Software)));
}

// Regression: Games share the "application" umbrella and must pass too.
void TestFilterPolicy::testApplicationUmbrellaAcceptsGames()
{
    FilterSettings s;
    s.contentTypeFilter = QStringLiteral("application");
    FilterPolicy policy(s);
    QVERIFY(policy.accepts(makeTorrent("some game", ContentType::Games)));
}

void TestFilterPolicy::testApplicationUmbrellaRejectsVideo()
{
    FilterSettings s;
    s.contentTypeFilter = QStringLiteral("application");
    FilterPolicy policy(s);
    QVERIFY(!policy.accepts(makeTorrent("movie", ContentType::Video)));
}

// "disc" has no matching ContentType; it must not accept anything on its own and
// must not crash. With only "disc" allowed, nothing passes.
void TestFilterPolicy::testDiscTokenIsInertNoOp()
{
    FilterSettings s;
    s.contentTypeFilter = QStringLiteral("disc");
    FilterPolicy policy(s);
    QVERIFY(!policy.accepts(makeTorrent("movie", ContentType::Video)));
    QVERIFY(!policy.accepts(makeTorrent("app", ContentType::Software)));

    // But combined with a real token it stays inert and the real token works.
    s.contentTypeFilter = QStringLiteral("disc,video");
    policy.setSettings(s);
    QVERIFY(policy.accepts(makeTorrent("movie", ContentType::Video)));
}

void TestFilterPolicy::testWhitespaceAndCaseTolerant()
{
    FilterSettings s;
    s.contentTypeFilter = QStringLiteral(" Video , Application ");
    FilterPolicy policy(s);
    QVERIFY(policy.accepts(makeTorrent("movie", ContentType::Video)));
    QVERIFY(policy.accepts(makeTorrent("app", ContentType::Software)));
}

void TestFilterPolicy::testSizeLimits()
{
    FilterSettings s;
    s.sizeMin = 50;
    s.sizeMax = 200;
    FilterPolicy policy(s);

    Torrent small = makeTorrent("small", ContentType::Video);
    small.size = 10;
    QVERIFY(!policy.accepts(small));

    Torrent big = makeTorrent("big", ContentType::Video);
    big.size = 500;
    QVERIFY(!policy.accepts(big));

    Torrent ok = makeTorrent("ok", ContentType::Video);
    ok.size = 100;
    QVERIFY(policy.accepts(ok));
}

void TestFilterPolicy::testFileCountLimit()
{
    FilterSettings s;
    s.maxFiles = 5;
    FilterPolicy policy(s);

    Torrent many = makeTorrent("many", ContentType::Video);
    many.files = 10;
    QVERIFY(!policy.accepts(many));

    Torrent few = makeTorrent("few", ContentType::Video);
    few.files = 3;
    QVERIFY(policy.accepts(few));
}

void TestFilterPolicy::testAdultFilter()
{
    FilterSettings s;
    s.adultFilter = true;
    FilterPolicy policy(s);
    QVERIFY(!policy.accepts(makeTorrent("hot xxx clip", ContentType::Video)));

    Torrent categorized = makeTorrent("clean name", ContentType::Video);
    categorized.contentCategory = ContentCategory::XXX;
    QVERIFY(!policy.accepts(categorized));

    QVERIFY(policy.accepts(makeTorrent("family movie", ContentType::Video)));

    // Disabled -> passes through.
    s.adultFilter = false;
    policy.setSettings(s);
    QVERIFY(policy.accepts(makeTorrent("hot xxx clip", ContentType::Video)));
}

void TestFilterPolicy::testNamingRegExp()
{
    FilterSettings s;
    s.namingRegExp = QStringLiteral("1080p");
    FilterPolicy policy(s);
    QVERIFY(policy.accepts(makeTorrent("movie 1080p", ContentType::Video)));
    QVERIFY(!policy.accepts(makeTorrent("movie 720p", ContentType::Video)));

    // Negative: matching the pattern is what gets rejected.
    s.namingRegExpNegative = true;
    policy.setSettings(s);
    QVERIFY(!policy.accepts(makeTorrent("movie 1080p", ContentType::Video)));
    QVERIFY(policy.accepts(makeTorrent("movie 720p", ContentType::Video)));
}

QTEST_MAIN(TestFilterPolicy)
#include "test_filter_policy.moc"
