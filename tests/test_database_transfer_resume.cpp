/**
 * @file test_database_transfer_resume.cpp
 * @brief The note beside a half-received peer dump, and the rule that decides
 *        whether the bytes under it are worth continuing.
 *
 * This is the piece that decides whether gigabytes are downloaded twice. Say yes
 * to a partial of a *different* dump and the mistake only surfaces as a SHA-256
 * mismatch after the whole file has been fetched; say no when it was the right
 * one and every dropped connection costs the download from the first byte. It
 * needs no peer, no transport and no event loop, so it is tested directly.
 */

#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "services/database_transfer_resume.h"

using rats::service::DatabaseTransferResume;

namespace {

constexpr char kPeer[] = "7739c510107975a5d240ef3eb4ac9b4186e6a7260e012f18ee4d2c673d0e56a0";
constexpr char kSnapshot[] = "snapshot-7.ratsdb";
constexpr qint64 kSize = 4 * 1024 * 1024;

DatabaseTransferResume note()
{
    return DatabaseTransferResume { QString::fromLatin1(kPeer), QString::fromLatin1(kSnapshot), kSize };
}

} // namespace

class TestDatabaseTransferResume : public QObject {
    Q_OBJECT

private slots:
    // One directory for the whole class, so each test starts from no note at all.
    void init() { QFile::remove(path()); }

    void testRoundTripThroughAFile();
    void testMissingOrMalformedFileIsNotAResumePoint();
    void testMatchingOfferResumesAtWhatIsOnDisk();
    void testAnotherGenerationStartsOver();
    void testAnotherPeerStartsOver();
    void testAChangedSizeStartsOver();
    void testAPeerThatNamesNoGenerationStartsOver();
    void testAPartialAsLongAsTheFileStartsOver();
    void testAnEmptyPartialIsNotAResumePoint();
    void testClearRemovesTheNote();

private:
    QTemporaryDir dir_;
    QString path() const { return QDir(dir_.path()).absoluteFilePath(QStringLiteral("incoming.part.json")); }
};

void TestDatabaseTransferResume::testRoundTripThroughAFile()
{
    QVERIFY(note().save(path()));
    const DatabaseTransferResume loaded = DatabaseTransferResume::load(path());
    QVERIFY(loaded.isValid());
    QCOMPARE(loaded.peerId, QString::fromLatin1(kPeer));
    QCOMPARE(loaded.snapshot, QString::fromLatin1(kSnapshot));
    QCOMPARE(loaded.size, kSize);
}

void TestDatabaseTransferResume::testMissingOrMalformedFileIsNotAResumePoint()
{
    QVERIFY(!DatabaseTransferResume::load(path()).isValid());

    QFile file(path());
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not json at all");
    file.close();
    QVERIFY(!DatabaseTransferResume::load(path()).isValid());
}

void TestDatabaseTransferResume::testMatchingOfferResumesAtWhatIsOnDisk()
{
    // Same peer, same generation, same total size: the bytes on disk are a prefix
    // of what is being offered, so the transfer picks up where it stopped.
    QCOMPARE(
        note().offsetFor(1024, QString::fromLatin1(kPeer), QString::fromLatin1(kSnapshot), kSize), Q_INT64_C(1024));
}

void TestDatabaseTransferResume::testAnotherGenerationStartsOver()
{
    // The peer rebuilt its snapshot. Same size is possible and means nothing — a
    // different generation is a different file.
    QCOMPARE(
        note().offsetFor(1024, QString::fromLatin1(kPeer), QStringLiteral("snapshot-8.ratsdb"), kSize), Q_INT64_C(0));
}

void TestDatabaseTransferResume::testAnotherPeerStartsOver()
{
    QCOMPARE(note().offsetFor(1024, QStringLiteral("ab12"), QString::fromLatin1(kSnapshot), kSize), Q_INT64_C(0));
}

void TestDatabaseTransferResume::testAChangedSizeStartsOver()
{
    // The name survived a rebuild but the file did not: one of the two is lying,
    // and neither is worth a multi-gigabyte download to find out which.
    QCOMPARE(
        note().offsetFor(1024, QString::fromLatin1(kPeer), QString::fromLatin1(kSnapshot), kSize + 1), Q_INT64_C(0));
}

void TestDatabaseTransferResume::testAPeerThatNamesNoGenerationStartsOver()
{
    // A client too old to name what it serves. It may well be the same file; there
    // is no way to tell, and guessing wrong costs the whole download twice.
    QCOMPARE(note().offsetFor(1024, QString::fromLatin1(kPeer), QString(), kSize), Q_INT64_C(0));
}

void TestDatabaseTransferResume::testAPartialAsLongAsTheFileStartsOver()
{
    // Not a prefix to continue: a finished download of something else, or the
    // wreckage of one.
    QCOMPARE(note().offsetFor(kSize, QString::fromLatin1(kPeer), QString::fromLatin1(kSnapshot), kSize), Q_INT64_C(0));
    QCOMPARE(
        note().offsetFor(kSize + 1, QString::fromLatin1(kPeer), QString::fromLatin1(kSnapshot), kSize), Q_INT64_C(0));
}

void TestDatabaseTransferResume::testAnEmptyPartialIsNotAResumePoint()
{
    QCOMPARE(note().offsetFor(0, QString::fromLatin1(kPeer), QString::fromLatin1(kSnapshot), kSize), Q_INT64_C(0));
}

void TestDatabaseTransferResume::testClearRemovesTheNote()
{
    QVERIFY(note().save(path()));
    DatabaseTransferResume::clear(path());
    QVERIFY(!QFileInfo::exists(path()));
    QVERIFY(!DatabaseTransferResume::load(path()).isValid());
}

QTEST_MAIN(TestDatabaseTransferResume)
#include "test_database_transfer_resume.moc"
