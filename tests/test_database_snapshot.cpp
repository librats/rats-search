/**
 * @file test_database_snapshot.cpp
 * @brief The shared dump this node hands to peers: freshness policy and the
 *        commit/load round-trip.
 *
 * The freshness rule is what turns "export the whole index for every peer that
 * asks" into "the file is already on disk", so it is the piece that decides
 * whether a requesting peer waits seconds or minutes. It is also pure policy —
 * no database, no network — so it is tested directly.
 */

#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "services/database_snapshot.h"

using rats::service::DatabaseSnapshot;

namespace {

// Stand in for a generated dump: commit() only cares that the temporary exists.
bool writeTemporary(const DatabaseSnapshot& snapshot, qint64 bytes)
{
    QFile file(snapshot.temporaryPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(QByteArray(static_cast<int>(bytes), 'x')) == bytes;
}

} // namespace

class TestDatabaseSnapshot : public QObject {
    Q_OBJECT

private slots:
    void init();

    void testNoSnapshotIsNeverFresh();
    void testCommitPublishesAndSurvivesReload();
    void testCommitWithoutATemporaryFails();
    void testAgedOutSnapshotIsStale();
    void testDriftMakesASnapshotStale();
    void testSmallAbsoluteDriftIsToleratedOnASmallIndex();
    void testMetadataMismatchDiscardsTheSnapshot();
    void testDiscardRemovesBothFiles();
    void testBytesPerTorrentIsMeasuredFromTheSnapshot();
    void testEachGenerationGetsItsOwnName();
    void testPruneKeepsTheLiveFileAndAnythingInUse();
    void testGenerationCounterSurvivesAReload();
    void testPathForGenerationFindsASupersededDump();
    void testPathForGenerationRefusesAnythingButADump();

private:
    QTemporaryDir dir_;
};

void TestDatabaseSnapshot::init()
{
    // One directory per test: commit() and discard() both touch the same names.
    const QDir root(dir_.path());
    const QStringList entries = root.entryList(QDir::Files);
    for (const QString& name : entries)
        QFile::remove(root.absoluteFilePath(name));
}

void TestDatabaseSnapshot::testNoSnapshotIsNeverFresh()
{
    DatabaseSnapshot snapshot(dir_.path());
    snapshot.load();
    QVERIFY(!snapshot.info().valid);
    QVERIFY(!snapshot.isFresh(0));
    QVERIFY(!snapshot.isFresh(1000));
}

void TestDatabaseSnapshot::testCommitPublishesAndSurvivesReload()
{
    DatabaseSnapshot snapshot(dir_.path());
    QVERIFY(writeTemporary(snapshot, 4096));
    QVERIFY(snapshot.commit(1000));

    QVERIFY(snapshot.info().valid);
    QCOMPARE(snapshot.info().torrents, 1000LL);
    QCOMPARE(snapshot.info().bytes, 4096LL);
    QVERIFY(QFile::exists(snapshot.path()));
    QVERIFY(DatabaseSnapshot::isSnapshotFile(QFileInfo(snapshot.path()).fileName()));
    // The temporary is renamed, not copied: leaving it behind would double the
    // disk cost of every generation.
    QVERIFY(!QFile::exists(snapshot.temporaryPath()));

    DatabaseSnapshot reloaded(dir_.path());
    reloaded.load();
    QVERIFY(reloaded.info().valid);
    QCOMPARE(reloaded.info().torrents, 1000LL);
    QCOMPARE(reloaded.info().bytes, 4096LL);
    QVERIFY(reloaded.isFresh(1000));
}

void TestDatabaseSnapshot::testCommitWithoutATemporaryFails()
{
    DatabaseSnapshot snapshot(dir_.path());
    QString error;
    QVERIFY(!snapshot.commit(1000, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!snapshot.info().valid);
}

void TestDatabaseSnapshot::testAgedOutSnapshotIsStale()
{
    DatabaseSnapshot::Policy policy;
    policy.maxAgeSecs = 0; // anything with a measurable age is too old
    DatabaseSnapshot snapshot(dir_.path(), policy);
    QVERIFY(writeTemporary(snapshot, 1024));
    QVERIFY(snapshot.commit(1000));

    QTest::qWait(1100);
    QVERIFY(!snapshot.isFresh(1000));
}

void TestDatabaseSnapshot::testDriftMakesASnapshotStale()
{
    DatabaseSnapshot::Policy policy;
    policy.maxDriftRatio = 0.10;
    policy.minDriftTorrents = 10;
    DatabaseSnapshot snapshot(dir_.path(), policy);
    QVERIFY(writeTemporary(snapshot, 1024));
    QVERIFY(snapshot.commit(10000));

    QVERIFY(snapshot.isFresh(10000));
    QVERIFY(snapshot.isFresh(10900)); // +9%
    QVERIFY(!snapshot.isFresh(12000)); // +20%
    // Drift is symmetric: an index that shrank has removed rows the snapshot
    // still carries, which is just as stale as one that grew.
    QVERIFY(!snapshot.isFresh(8000));
}

void TestDatabaseSnapshot::testSmallAbsoluteDriftIsToleratedOnASmallIndex()
{
    DatabaseSnapshot::Policy policy;
    policy.maxDriftRatio = 0.10;
    policy.minDriftTorrents = 500;
    DatabaseSnapshot snapshot(dir_.path(), policy);
    QVERIFY(writeTemporary(snapshot, 1024));
    QVERIFY(snapshot.commit(100));

    // 100 -> 400 is a 300% ratio but only 300 torrents. Without the absolute
    // floor a nearly empty index would rebuild its snapshot on every few crawls.
    QVERIFY(snapshot.isFresh(400));
    QVERIFY(!snapshot.isFresh(1000));
}

void TestDatabaseSnapshot::testMetadataMismatchDiscardsTheSnapshot()
{
    DatabaseSnapshot snapshot(dir_.path());
    QVERIFY(writeTemporary(snapshot, 2048));
    QVERIFY(snapshot.commit(500));

    // Simulate a crash between writing the dump and its metadata: the file on
    // disk is not the one the metadata describes, so neither can be trusted.
    QFile dump(snapshot.path());
    QVERIFY(dump.open(QIODevice::Append));
    dump.write(QByteArray(64, 'y'));
    dump.close();

    DatabaseSnapshot reloaded(dir_.path());
    reloaded.load();
    QVERIFY(!reloaded.info().valid);
    QVERIFY(!QFile::exists(reloaded.path()));
    QVERIFY(!QFile::exists(reloaded.metadataPath()));
}

void TestDatabaseSnapshot::testDiscardRemovesBothFiles()
{
    DatabaseSnapshot snapshot(dir_.path());
    QVERIFY(writeTemporary(snapshot, 1024));
    QVERIFY(snapshot.commit(42));
    QVERIFY(QFile::exists(snapshot.metadataPath()));

    snapshot.discard();
    QVERIFY(!snapshot.info().valid);
    QVERIFY(!QFile::exists(snapshot.path()));
    QVERIFY(!QFile::exists(snapshot.metadataPath()));
    QVERIFY(!snapshot.isFresh(42));
}

void TestDatabaseSnapshot::testBytesPerTorrentIsMeasuredFromTheSnapshot()
{
    DatabaseSnapshot snapshot(dir_.path());
    // Before any snapshot exists the estimate is a constant; it only has to be
    // positive, since it is spent on a free-space check.
    QVERIFY(snapshot.bytesPerTorrent() > 0.0);

    QVERIFY(writeTemporary(snapshot, 8000));
    QVERIFY(snapshot.commit(1000));
    QCOMPARE(snapshot.bytesPerTorrent(), 8.0);
}

void TestDatabaseSnapshot::testEachGenerationGetsItsOwnName()
{
    DatabaseSnapshot snapshot(dir_.path());
    QVERIFY(writeTemporary(snapshot, 1024));
    QVERIFY(snapshot.commit(100));
    const QString first = snapshot.path();

    QVERIFY(writeTemporary(snapshot, 2048));
    QVERIFY(snapshot.commit(200));
    const QString second = snapshot.path();

    // Not an aesthetic choice: a peer can still be mid-download from the previous
    // generation, and on Windows that file cannot be renamed over while it is
    // open — a fixed name would fail to publish exactly when the node is busiest.
    QVERIFY(first != second);
    QVERIFY(QFile::exists(first));
    QVERIFY(QFile::exists(second));
    QCOMPARE(snapshot.info().torrents, 200LL);
    QCOMPARE(snapshot.info().bytes, 2048LL);
}

void TestDatabaseSnapshot::testPruneKeepsTheLiveFileAndAnythingInUse()
{
    DatabaseSnapshot snapshot(dir_.path());
    QVERIFY(writeTemporary(snapshot, 1024));
    QVERIFY(snapshot.commit(100));
    const QString first = snapshot.path();

    QVERIFY(writeTemporary(snapshot, 1024));
    QVERIFY(snapshot.commit(200));
    const QString second = snapshot.path();

    QVERIFY(writeTemporary(snapshot, 1024));
    QVERIFY(snapshot.commit(300));
    const QString third = snapshot.path();

    // A transfer is still reading the first generation, so only the middle one is
    // genuinely unreferenced.
    snapshot.pruneSuperseded({ first });
    QVERIFY(QFile::exists(first));
    QVERIFY(!QFile::exists(second));
    QVERIFY(QFile::exists(third));

    // Once that transfer ends, nothing holds it any more.
    snapshot.pruneSuperseded({});
    QVERIFY(!QFile::exists(first));
    QVERIFY(QFile::exists(third));
}

void TestDatabaseSnapshot::testGenerationCounterSurvivesAReload()
{
    QString first;
    {
        DatabaseSnapshot snapshot(dir_.path());
        QVERIFY(writeTemporary(snapshot, 1024));
        QVERIFY(snapshot.commit(100));
        first = snapshot.path();
    }

    // A restart must not reuse a name: a peer that was downloading the previous
    // generation when the process died may still have that file open.
    DatabaseSnapshot reloaded(dir_.path());
    reloaded.load();
    QCOMPARE(reloaded.path(), first);
    QVERIFY(writeTemporary(reloaded, 1024));
    QVERIFY(reloaded.commit(200));
    QVERIFY(reloaded.path() != first);
}

void TestDatabaseSnapshot::testPathForGenerationFindsASupersededDump()
{
    DatabaseSnapshot snapshot(dir_.path());
    QVERIFY(writeTemporary(snapshot, 1024));
    QVERIFY(snapshot.commit(100));
    const QString first = snapshot.path();
    QVERIFY(writeTemporary(snapshot, 2048));
    QVERIFY(snapshot.commit(200));

    // A peer continuing an interrupted download names the generation it has bytes
    // of, and gets that file back even though a newer one has since been published.
    QCOMPARE(snapshot.pathForGeneration(QFileInfo(first).fileName()), first);
    QCOMPARE(snapshot.pathForGeneration(QFileInfo(snapshot.path()).fileName()), snapshot.path());

    // Once it is pruned there is nothing to continue, and saying so is what sends
    // the peer back to a fresh download instead of a file that is not there.
    snapshot.pruneSuperseded({});
    QVERIFY(snapshot.pathForGeneration(QFileInfo(first).fileName()).isEmpty());
}

void TestDatabaseSnapshot::testPathForGenerationRefusesAnythingButADump()
{
    DatabaseSnapshot snapshot(dir_.path());
    QVERIFY(writeTemporary(snapshot, 1024));
    QVERIFY(snapshot.commit(100));

    // The name comes off the wire, so it may be anything at all.
    QVERIFY(snapshot.pathForGeneration(QString()).isEmpty());
    QVERIFY(snapshot.pathForGeneration(QStringLiteral("snapshot.json")).isEmpty());
    QVERIFY(snapshot.pathForGeneration(QStringLiteral("snapshot.ratsdb.part")).isEmpty());
    QVERIFY(snapshot.pathForGeneration(QStringLiteral("snapshot-99.ratsdb")).isEmpty());
    QVERIFY(snapshot.pathForGeneration(QStringLiteral("snapshot-.ratsdb")).isEmpty());
    QVERIFY(snapshot.pathForGeneration(QStringLiteral("snapshot-../../x.ratsdb")).isEmpty());
    QVERIFY(snapshot.pathForGeneration(QStringLiteral("snapshot-1/../x.ratsdb")).isEmpty());
}

QTEST_MAIN(TestDatabaseSnapshot)
#include "test_database_snapshot.moc"
