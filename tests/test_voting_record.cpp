/**
 * @file test_voting_record.cpp
 * @brief Pins the shape of the record a vote is replicated as.
 *
 * This is a size test as much as a format test. The vote store replicates to
 * every peer and its whole contents are re-sent on every snapshot exchange, so
 * a field added to a vote is paid for by the entire network, on every sync, for
 * as long as the vote exists.
 *
 * That is not hypothetical. A vote used to embed the complete torrent — every
 * file name included — under a "_torrent" key, put there so a peer that did not
 * have the torrent could pick it up from the vote. Nothing ever read it back:
 * VotingService::onRecordStored() uses torrentHash alone and returns early when
 * the torrent is unknown locally, which is precisely the case the payload was
 * meant to serve. The cost was an entry of ~24 KB instead of ~300 bytes, and a
 * few hundred votes were enough to push a peer's snapshot past what a connection
 * could carry.
 */

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QtTest>

#include "services/voting_service.h"

using rats::service::VotingService;

namespace {

const QString kHash = QStringLiteral("0123456789abcdef0123456789abcdef01234567");

} // namespace

class TestVotingRecord : public QObject {
    Q_OBJECT

private slots:
    void testGoodVoteFields();
    void testBadVoteFields();
    void testCarriesNothingBeyondTheVote();
    void testStaysSmall();
    void testIndexGroupsVotesByTorrent();
};

// A good vote carries the four fields that make it a vote, and says "good".
void TestVotingRecord::testGoodVoteFields()
{
    const QJsonObject r = VotingService::voteRecord(kHash, true);

    QCOMPARE(r.value("type").toString(), QStringLiteral("vote"));
    QCOMPARE(r.value("torrentHash").toString(), kHash);
    QCOMPARE(r.value("vote").toString(), QStringLiteral("good"));
    QCOMPARE(r.value("_index").toString(), QStringLiteral("vote:") + kHash);
}

void TestVotingRecord::testBadVoteFields()
{
    const QJsonObject r = VotingService::voteRecord(kHash, false);

    QCOMPARE(r.value("vote").toString(), QStringLiteral("bad"));
    QCOMPARE(r.value("torrentHash").toString(), kHash);
}

// The regression guard: exactly these keys, and no others. Written as an exact
// key set rather than an absence check for "_torrent" specifically, so that the
// next field someone is tempted to smuggle into a replicated vote also has to
// come past this test.
void TestVotingRecord::testCarriesNothingBeyondTheVote()
{
    for (bool good : { true, false }) {
        const QJsonObject r = VotingService::voteRecord(kHash, good);

        QStringList keys = r.keys();
        keys.sort();

        QStringList expected { QStringLiteral("_index"), QStringLiteral("torrentHash"), QStringLiteral("type"),
            QStringLiteral("vote") };
        expected.sort();

        QCOMPARE(keys, expected);
        QVERIFY2(!r.contains(QStringLiteral("_torrent")),
            "a vote must not carry torrent data: it is replicated to every peer "
            "and re-sent on every snapshot");
    }

    // Nothing nested either — a vote is flat, so no field can quietly grow with
    // the torrent it refers to.
    const QJsonObject r = VotingService::voteRecord(kHash, true);
    for (const QString& key : r.keys()) {
        const QJsonValue v = r.value(key);
        QVERIFY2(v.isString(), qPrintable(QStringLiteral("field '%1' is not a plain string").arg(key)));
    }
}

// The serialized size is what actually crosses the wire, once per peer per sync.
// A few hundred bytes is the budget; the old record was two orders out.
void TestVotingRecord::testStaysSmall()
{
    const QByteArray encoded = QJsonDocument(VotingService::voteRecord(kHash, true)).toJson(QJsonDocument::Compact);

    QVERIFY2(
        encoded.size() < 512, qPrintable(QStringLiteral("a replicated vote grew to %1 bytes").arg(encoded.size())));
}

// _index is the prefix VotingService::aggregate() scans to collect every peer's
// vote on one torrent, so it must name the torrent and nothing peer-specific.
void TestVotingRecord::testIndexGroupsVotesByTorrent()
{
    const QJsonObject a = VotingService::voteRecord(kHash, true);
    const QJsonObject b = VotingService::voteRecord(kHash, false);

    QCOMPARE(a.value("_index").toString(), b.value("_index").toString());
    QVERIFY(a.value("_index").toString().contains(kHash));
}

QTEST_MAIN(TestVotingRecord)
#include "test_voting_record.moc"
