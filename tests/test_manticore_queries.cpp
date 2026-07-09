/**
 * @file test_manticore_queries.cpp
 * @brief Integration test that starts a real Manticore instance and drives the
 *        new data layer (rats::data::Manticore + Database + TorrentRepository).
 *
 * Verifies the end-to-end CRUD / search / statistics path against a live
 * searchd, replacing the old raw-SphinxQL query test. Requires the bundled
 * searchd executable to be discoverable via the standard import paths.
 */

#include <QJsonObject>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest/QtTest>

#include "data/database.h"
#include "data/manticore.h"
#include "data/query.h"
#include "data/torrent_repository.h"
#include "domain/content.h"
#include "domain/torrent.h"

using rats::data::Database;
using rats::data::Manticore;
using rats::data::SelectQuery;
using rats::data::TorrentRepository;
using rats::domain::ContentCategory;
using rats::domain::ContentType;
using rats::domain::File;
using rats::domain::Torrent;

class TestManticoreQueries : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testConnected();
    void testAddAndGet();
    void testExists();
    void testGetWithFiles();
    void testSearchByName();
    void testSearchByHash();
    void testUpdateTrackerCounts();
    void testStatistics();
    void testRecent();
    void testTop();
    void testRemove();
    void testSelectQueryAgainstLiveIndex();

private:
    // Build a valid torrent with a distinct 40-hex hash derived from `n`.
    Torrent makeTorrent(
        int n, const QString& name, ContentType type = ContentType::Video, qint64 size = 1024 * 1024, int seeders = 10);
    // Poll get() until the RT row is visible (RT attributes settle asynchronously).
    bool waitForTorrent(const QString& hash, int maxRetries = 50, int delayMs = 20);

    QTemporaryDir* tempDir_ = nullptr;
    Manticore* manticore_ = nullptr;
    Database* db_ = nullptr;
    TorrentRepository* repo_ = nullptr;
};

Torrent TestManticoreQueries::makeTorrent(int n, const QString& name, ContentType type, qint64 size, int seeders)
{
    Torrent t;
    // 40-char hex hash: an 8-hex prefix from n, padded with 'a'.
    t.hash = QString("%1").arg(n, 8, 16, QChar('0')) + QString(32, 'a');
    t.name = name;
    t.size = size;
    t.files = 1;
    t.pieceLength = 262144;
    t.added = QDateTime::currentDateTime();
    t.ipv4 = "192.168.1.1";
    t.port = 6881;
    t.contentType = type;
    t.contentCategory = ContentCategory::Unknown;
    t.seeders = seeders;
    t.leechers = 5;
    t.completed = 100;
    return t;
}

bool TestManticoreQueries::waitForTorrent(const QString& hash, int maxRetries, int delayMs)
{
    for (int i = 0; i < maxRetries; ++i) {
        if (repo_->exists(hash))
            return true;
        QThread::msleep(delayMs);
    }
    return repo_->exists(hash);
}

void TestManticoreQueries::initTestCase()
{
    tempDir_ = new QTemporaryDir();
    QVERIFY2(tempDir_->isValid(), "Failed to create temporary directory");
    qInfo() << "Test data directory:" << tempDir_->path();

    manticore_ = new Manticore(tempDir_->path());
    const bool started = manticore_->start();
    QVERIFY2(started, "Failed to start Manticore Search. Is searchd available?");
    QVERIFY(manticore_->isRunning());
    qInfo() << "Manticore started on port" << manticore_->port();

    db_ = new Database(manticore_);
    QVERIFY(db_->isConnected());

    repo_ = new TorrentRepository(db_);
    repo_->primeFromDatabase();
}

void TestManticoreQueries::cleanupTestCase()
{
    delete repo_;
    repo_ = nullptr;
    delete db_;
    db_ = nullptr;
    if (manticore_) {
        manticore_->stop();
        delete manticore_;
        manticore_ = nullptr;
    }
    delete tempDir_;
    tempDir_ = nullptr;
}

void TestManticoreQueries::testConnected()
{
    QVERIFY(db_->isConnected());
    QVERIFY(manticore_->isRunning());
}

void TestManticoreQueries::testAddAndGet()
{
    const Torrent t = makeTorrent(1, "Ubuntu Linux ISO", ContentType::Archive, 4LL * 1024 * 1024 * 1024, 42);
    QVERIFY(repo_->add(t));
    QVERIFY(waitForTorrent(t.hash));

    const auto got = repo_->get(t.hash);
    QVERIFY(got.has_value());
    QCOMPARE(got->hash, t.hash);
    QCOMPARE(got->name, QString("Ubuntu Linux ISO"));
    QCOMPARE(got->size, (qint64)(4LL * 1024 * 1024 * 1024));
    QCOMPARE(got->seeders, 42);
    QCOMPARE(rats::domain::toId(got->contentType), rats::domain::toId(ContentType::Archive));
}

void TestManticoreQueries::testExists()
{
    QVERIFY(repo_->exists(makeTorrent(1, "x").hash)); // added in testAddAndGet
    QVERIFY(!repo_->exists(QString(40, 'f'))); // never inserted
}

void TestManticoreQueries::testGetWithFiles()
{
    Torrent t = makeTorrent(2, "Multi File Pack", ContentType::Video, 2000, 5);
    t.files = 2;
    t.fileList.append(File { "folder/movie.mkv", 1900 });
    t.fileList.append(File { "folder/readme.txt", 100 });
    QVERIFY(repo_->add(t));
    QVERIFY(waitForTorrent(t.hash));

    const auto got = repo_->get(t.hash, /*includeFiles=*/true);
    QVERIFY(got.has_value());
    QCOMPARE(got->fileList.size(), 2);
    QCOMPARE(got->fileList[0].path, QString("folder/movie.mkv"));
    QCOMPARE(got->fileList[0].size, (qint64)1900);
}

void TestManticoreQueries::testSearchByName()
{
    QVERIFY(repo_->add(makeTorrent(3, "Debian Server Edition", ContentType::Archive)));
    QVERIFY(waitForTorrent(makeTorrent(3, "x").hash));

    TorrentRepository::SearchQuery q;
    q.text = "Debian";
    q.limit = 10;
    const auto hits = repo_->searchTorrents(q);
    bool found = false;
    for (const auto& h : hits) {
        if (h.torrent.name.contains("Debian"))
            found = true;
    }
    QVERIFY2(found, "Full-text search should find the 'Debian' torrent");
}

void TestManticoreQueries::testSearchByHash()
{
    const Torrent t = makeTorrent(1, "x"); // Ubuntu torrent's hash
    TorrentRepository::SearchQuery q;
    q.text = t.hash; // a 40-hex string is treated as an exact hash lookup
    const auto hits = repo_->searchTorrents(q);
    QCOMPARE(hits.size(), 1);
    QCOMPARE(hits.first().torrent.hash, t.hash);
}

void TestManticoreQueries::testUpdateTrackerCounts()
{
    const Torrent t = makeTorrent(1, "x");
    QVERIFY(repo_->updateTrackerCounts(t.hash, 111, 22, 3));

    const auto got = repo_->get(t.hash);
    QVERIFY(got.has_value());
    QCOMPARE(got->seeders, 111);
    QCOMPARE(got->leechers, 22);
    QCOMPARE(got->completed, 3);
}

void TestManticoreQueries::testStatistics()
{
    const auto stats = repo_->statistics();
    // At least the three torrents added above.
    QVERIFY2(stats.torrents >= 3, qPrintable(QString("torrents=%1").arg(stats.torrents)));
    QVERIFY(stats.totalSize > 0);
}

void TestManticoreQueries::testRecent()
{
    const auto recent = repo_->recent(10);
    QVERIFY(!recent.isEmpty());
}

void TestManticoreQueries::testTop()
{
    // Ubuntu torrent now has 111 seeders; it should appear in the top list.
    const auto top = repo_->top(QString(), QString(), 0, 10);
    QVERIFY(!top.isEmpty());
    // Ordered by seeders descending.
    for (int i = 1; i < top.size(); ++i)
        QVERIFY(top[i - 1].seeders >= top[i].seeders);
}

void TestManticoreQueries::testRemove()
{
    const Torrent t = makeTorrent(3, "x"); // Debian torrent
    QVERIFY(repo_->exists(t.hash));
    QVERIFY(repo_->remove(t.hash));
    // Removal should eventually be visible.
    bool gone = false;
    for (int i = 0; i < 50; ++i) {
        if (!repo_->exists(t.hash)) {
            gone = true;
            break;
        }
        QThread::msleep(20);
    }
    QVERIFY2(gone, "Torrent should be removed");
}

void TestManticoreQueries::testSelectQueryAgainstLiveIndex()
{
    // Exercise the SelectQuery builder end-to-end against the live index.
    const QString sql = SelectQuery("torrents")
                            .columns("id, hash, name")
                            .whereRaw("seeders > 0")
                            .orderBy("seeders", true)
                            .limit(0, 5)
                            .build();
    bool ok = false;
    const auto rows = db_->query(sql, {}, &ok);
    QVERIFY(ok);
    // The Ubuntu torrent (111 seeders) guarantees at least one row.
    QVERIFY(!rows.isEmpty());
}

QTEST_MAIN(TestManticoreQueries)
#include "test_manticore_queries.moc"
