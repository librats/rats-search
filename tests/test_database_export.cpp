/**
 * @file test_database_export.cpp
 * @brief Unit tests for the interchange export formats (JsonLinesWriter).
 *        These files are read by programs that are not Rats — jq, pandas,
 *        DuckDB — so the exact bytes are the contract: one record per physical
 *        line, no preamble, nothing but torrents.
 */

#include <QtTest/QtTest>

#include "domain/torrent.h"
#include "domain/torrent_codec.h"
#include "services/database_export.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTemporaryDir>

using namespace rats::domain;
using namespace rats::service;

namespace {

Torrent makeTorrent(int index)
{
    Torrent t;
    t.hash = QStringLiteral("%1").arg(index, 40, 16, QLatin1Char('0'));
    t.name = QStringLiteral("Torrent number %1").arg(index);
    t.size = 1024LL * 1024LL * (index + 1);
    t.files = 2;
    t.pieceLength = 262144;
    t.added = QDateTime::fromSecsSinceEpoch(1700000000 + index);
    t.contentType = ContentType::Video;
    t.contentCategory = ContentCategory::Movie;
    t.seeders = index;
    t.leechers = index * 2;
    t.good = index % 5;
    t.bad = index % 3;
    t.fileList = { File { QStringLiteral("dir/file%1.mkv").arg(index), t.size - 100 },
        File { QStringLiteral("dir/readme%1.txt").arg(index), 100 } };
    return t;
}

ExportHeader makeHeader(qint64 torrents)
{
    ExportHeader header;
    header.client = QStringLiteral("2.2.4");
    header.peerId = QStringLiteral("abcdef");
    header.created = QDateTime::fromSecsSinceEpoch(1700000000);
    header.torrents = torrents;
    return header;
}

// Split the file into records. A well-formed JSONL file ends with LF, so the
// final split part is empty and is dropped; any *other* empty element survives
// and fails the JSON parse below, which is exactly what a stray blank line
// should do.
QList<QByteArray> readRecordLines(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QList<QByteArray> parts = file.readAll().split('\n');
    if (!parts.isEmpty() && parts.last().isEmpty())
        parts.removeLast();
    return parts;
}

} // namespace

class TestDatabaseExport : public QObject {
    Q_OBJECT

private slots:
    void testJsonLinesRoundTrip();
    void testJsonLinesHasNoPreamble();
    void testJsonLinesKeepsOneRecordPerLine();
    void testJsonLinesEmptyExport();
    void testJsonLinesUnfinishedWriteLeavesNoFile();
    void testJsonLinesFlushesLargeBatches();

private:
    QTemporaryDir dir_;
    QString path(const QString& name) const { return dir_.filePath(name); }
};

void TestDatabaseExport::testJsonLinesRoundTrip()
{
    const QString file = path(QStringLiteral("roundtrip.jsonl"));

    JsonLinesWriter writer;
    QVERIFY(writer.open(file, makeHeader(3)));
    for (int i = 0; i < 3; ++i)
        QVERIFY(writer.write(makeTorrent(i)));
    QVERIFY(writer.finish());
    QCOMPARE(writer.torrentsWritten(), qint64(3));

    const QList<QByteArray> lines = readRecordLines(file);
    QCOMPARE(lines.size(), 3);

    for (int i = 0; i < 3; ++i) {
        QJsonParseError parse {};
        const QJsonDocument doc = QJsonDocument::fromJson(lines.at(i), &parse);
        QCOMPARE(parse.error, QJsonParseError::NoError);
        QVERIFY(doc.isObject());

        const Torrent got = codec::torrentFromJson(doc.object());
        const Torrent expected = makeTorrent(i);
        QCOMPARE(got.hash, expected.hash);
        QCOMPARE(got.name, expected.name);
        QCOMPARE(got.size, expected.size);
        QCOMPARE(got.files, expected.files);
        QVERIFY(got.contentType == expected.contentType);
        QVERIFY(got.contentCategory == expected.contentCategory);
        QCOMPARE(got.seeders, expected.seeders);
        QCOMPARE(got.good, expected.good);
        QCOMPARE(got.bad, expected.bad);
        // JSONL is the lossless view: the file list rides along.
        QCOMPARE(got.fileList.size(), 2);
        QCOMPARE(got.fileList[0].path, expected.fileList[0].path);
        QCOMPARE(got.fileList[0].size, expected.fileList[0].size);
    }
}

void TestDatabaseExport::testJsonLinesHasNoPreamble()
{
    const QString file = path(QStringLiteral("preamble.jsonl"));

    JsonLinesWriter writer;
    QVERIFY(writer.open(file, makeHeader(2)));
    QVERIFY(writer.write(makeTorrent(7)));
    QVERIFY(writer.write(makeTorrent(8)));
    QVERIFY(writer.finish());

    // Two torrents in, two lines out: the provenance header is accepted and
    // dropped, because a metadata line would break every JSONL reader.
    const QList<QByteArray> lines = readRecordLines(file);
    QCOMPARE(lines.size(), 2);

    const QJsonObject first = QJsonDocument::fromJson(lines.first()).object();
    QCOMPARE(first.value(QStringLiteral("hash")).toString(), makeTorrent(7).hash);
    QVERIFY(!first.contains(QStringLiteral("format")));
    QVERIFY(!first.contains(QStringLiteral("client")));
}

void TestDatabaseExport::testJsonLinesKeepsOneRecordPerLine()
{
    const QString file = path(QStringLiteral("specials.jsonl"));

    // A DHT-sourced name can carry anything, newlines included. JSON escaping
    // has to keep the record on one physical line or every reader loses sync.
    Torrent nasty = makeTorrent(1);
    nasty.name = QStringLiteral("Quote\" comma, tab\t newline\n backslash\\ Кириллица 日本語");

    JsonLinesWriter writer;
    QVERIFY(writer.open(file, makeHeader(2)));
    QVERIFY(writer.write(nasty));
    QVERIFY(writer.write(makeTorrent(2)));
    QVERIFY(writer.finish());

    const QList<QByteArray> lines = readRecordLines(file);
    QCOMPARE(lines.size(), 2);

    const Torrent got = codec::torrentFromJson(QJsonDocument::fromJson(lines.first()).object());
    QCOMPARE(got.name, nasty.name);
}

void TestDatabaseExport::testJsonLinesEmptyExport()
{
    const QString file = path(QStringLiteral("empty.jsonl"));

    JsonLinesWriter writer;
    QVERIFY(writer.open(file, makeHeader(0)));
    QVERIFY(writer.finish());

    // An index with nothing in it is a valid, empty JSONL file — not a failure.
    QVERIFY(QFile::exists(file));
    QCOMPARE(QFileInfo(file).size(), qint64(0));
    QCOMPARE(writer.torrentsWritten(), qint64(0));
}

void TestDatabaseExport::testJsonLinesUnfinishedWriteLeavesNoFile()
{
    const QString file = path(QStringLiteral("unfinished.jsonl"));

    {
        JsonLinesWriter writer;
        QVERIFY(writer.open(file, makeHeader(1)));
        QVERIFY(writer.write(makeTorrent(0)));
        // Destroyed without finish() — a cancelled export must not leave a file
        // that looks like a complete one.
    }

    QVERIFY(!QFile::exists(file));
}

void TestDatabaseExport::testJsonLinesFlushesLargeBatches()
{
    const QString file = path(QStringLiteral("large.jsonl"));
    constexpr int kCount = 2000; // comfortably past the 256 KB write buffer

    JsonLinesWriter writer;
    QVERIFY(writer.open(file, makeHeader(kCount)));
    for (int i = 0; i < kCount; ++i)
        QVERIFY(writer.write(makeTorrent(i)));
    QVERIFY(writer.finish());

    const QList<QByteArray> lines = readRecordLines(file);
    QCOMPARE(lines.size(), kCount);

    // The buffered flush must not tear a record: check the seam-prone ones.
    for (int i : { 0, kCount / 2, kCount - 1 }) {
        QJsonParseError parse {};
        const QJsonDocument doc = QJsonDocument::fromJson(lines.at(i), &parse);
        QCOMPARE(parse.error, QJsonParseError::NoError);
        QCOMPARE(codec::torrentFromJson(doc.object()).hash, makeTorrent(i).hash);
    }
}

QTEST_MAIN(TestDatabaseExport)
#include "test_database_export.moc"
