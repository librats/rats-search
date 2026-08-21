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
#include "services/export_sink.h"

#include <QFile>
#include <QFileInfo>
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

QByteArray readAllBytes(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

// Rows of a CSV file, BOM stripped, split on CRLF. Splitting on CRLF is itself
// the line-ending check: an LF-only writer would come back as one giant row.
QStringList readCsvRows(const QString& path)
{
    QByteArray all = readAllBytes(path);
    if (all.startsWith(QByteArray("\xEF\xBB\xBF")))
        all.remove(0, 3);
    QStringList rows = QString::fromUtf8(all).split(QStringLiteral("\r\n"));
    if (!rows.isEmpty() && rows.last().isEmpty())
        rows.removeLast();
    return rows;
}

// An independent RFC 4180 field reader, so the writer is checked against a
// different implementation instead of against itself.
QStringList splitCsvRow(const QString& row)
{
    QStringList fields;
    QString current;
    bool quoted = false;
    for (int i = 0; i < row.size(); ++i) {
        const QChar c = row.at(i);
        if (quoted) {
            if (c != QLatin1Char('"')) {
                current += c;
            } else if (i + 1 < row.size() && row.at(i + 1) == QLatin1Char('"')) {
                current += QLatin1Char('"'); // "" is one escaped quote
                ++i;
            } else {
                quoted = false;
            }
        } else if (c == QLatin1Char('"')) {
            quoted = true;
        } else if (c == QLatin1Char(',')) {
            fields << current;
            current.clear();
        } else {
            current += c;
        }
    }
    fields << current;
    return fields;
}

// Column order of the torrents CSV.
enum Col {
    ColHash = 0,
    ColName,
    ColSize,
    ColFiles,
    ColPieceLength,
    ColAdded,
    ColType,
    ColCategory,
    ColSeeders,
    ColLeechers,
    ColCompleted,
    ColTrackersChecked,
    ColGood,
    ColBad,
    ColMagnet,
    ColPoster,
    ColDescription,
    ColCount
};

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

    void testCsvHeaderAndBom();
    void testCsvRoundTripsQuotedText();
    void testCsvDefusesFormulaInjection();
    void testCsvFlattensControlCharacters();
    void testCsvWritesIsoDates();
    void testCsvCompanionFileList();
    void testCsvWithoutFileListWritesNoCompanion();
    void testCsvUnfinishedWriteLeavesNoFiles();
    void testCsvEmptyExportKeepsHeader();

    void testFormatFollowsFileExtension();
    void testFormatNamesRoundTrip();
    void testFactoryBuildsTheRightSink();

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

void TestDatabaseExport::testCsvHeaderAndBom()
{
    const QString file = path(QStringLiteral("header.csv"));

    CsvWriter writer;
    QVERIFY(writer.open(file, makeHeader(1)));
    QVERIFY(writer.write(makeTorrent(1)));
    QVERIFY(writer.finish());

    // Without the BOM, Excel on Windows mojibakes every non-ASCII name.
    const QByteArray raw = readAllBytes(file);
    QVERIFY(raw.startsWith(QByteArray("\xEF\xBB\xBF")));
    QVERIFY(raw.endsWith(QByteArray("\r\n")));

    const QStringList rows = readCsvRows(file);
    QCOMPARE(rows.size(), 2); // header + one torrent

    const QStringList header = splitCsvRow(rows.first());
    QCOMPARE(header.size(), qsizetype(ColCount));
    QCOMPARE(header.at(ColHash), QStringLiteral("hash"));
    QCOMPARE(header.at(ColName), QStringLiteral("name"));
    QCOMPARE(header.at(ColMagnet), QStringLiteral("magnet"));
    QCOMPARE(header.at(ColDescription), QStringLiteral("description"));
}

void TestDatabaseExport::testCsvRoundTripsQuotedText()
{
    const QString file = path(QStringLiteral("quoting.csv"));

    Torrent torrent = makeTorrent(4);
    torrent.name = QStringLiteral("Comma, quote\" and \"\"double\"\" Кириллица");
    torrent.info = QJsonObject { { QStringLiteral("description"), QStringLiteral("multi, field \"text\"") } };

    CsvWriter writer;
    QVERIFY(writer.open(file, makeHeader(1)));
    QVERIFY(writer.write(torrent));
    QVERIFY(writer.finish());

    const QStringList rows = readCsvRows(file);
    QCOMPARE(rows.size(), 2);

    const QStringList fields = splitCsvRow(rows.at(1));
    QCOMPARE(fields.size(), qsizetype(ColCount));
    QCOMPARE(fields.at(ColName), torrent.name);
    QCOMPARE(fields.at(ColDescription), QStringLiteral("multi, field \"text\""));
    QCOMPARE(fields.at(ColHash), torrent.hash);
    QCOMPARE(fields.at(ColSize), QString::number(torrent.size));
    QCOMPARE(fields.at(ColType), QStringLiteral("video"));
    QCOMPARE(fields.at(ColCategory), QStringLiteral("movie"));
}

void TestDatabaseExport::testCsvDefusesFormulaInjection()
{
    const QString file = path(QStringLiteral("injection.csv"));

    // Every one of these is a name a stranger can publish on the DHT.
    const QStringList dangerous = { QStringLiteral("=cmd|' /c calc'!A1"), QStringLiteral("+1+1"),
        QStringLiteral("-2+3"), QStringLiteral("@SUM(A1)") };

    CsvWriter writer;
    QVERIFY(writer.open(file, makeHeader(dangerous.size())));
    for (int i = 0; i < dangerous.size(); ++i) {
        Torrent torrent = makeTorrent(i);
        torrent.name = dangerous.at(i);
        QVERIFY(writer.write(torrent));
    }
    QVERIFY(writer.finish());

    const QStringList rows = readCsvRows(file);
    QCOMPARE(rows.size(), dangerous.size() + 1);
    for (int i = 0; i < dangerous.size(); ++i) {
        // The apostrophe is what stops Excel executing the cell.
        QCOMPARE(splitCsvRow(rows.at(i + 1)).at(ColName), QStringLiteral("'") + dangerous.at(i));
    }
}

void TestDatabaseExport::testCsvFlattensControlCharacters()
{
    const QString file = path(QStringLiteral("controls.csv"));

    Torrent torrent = makeTorrent(5);
    torrent.name = QStringLiteral("line\nbreak\ttab\rreturn");

    CsvWriter writer;
    QVERIFY(writer.open(file, makeHeader(2)));
    QVERIFY(writer.write(torrent));
    QVERIFY(writer.write(makeTorrent(6)));
    QVERIFY(writer.finish());

    // The RFC would allow the newline inside quotes; too many readers mishandle
    // it, so one torrent stays one physical row.
    const QStringList rows = readCsvRows(file);
    QCOMPARE(rows.size(), 3);
    QCOMPARE(splitCsvRow(rows.at(1)).at(ColName), QStringLiteral("line break tab return"));
}

void TestDatabaseExport::testCsvWritesIsoDates()
{
    const QString file = path(QStringLiteral("dates.csv"));

    const Torrent torrent = makeTorrent(0);
    QVERIFY(!torrent.trackersChecked.isValid()); // never scraped

    CsvWriter writer;
    QVERIFY(writer.open(file, makeHeader(1)));
    QVERIFY(writer.write(torrent));
    QVERIFY(writer.finish());

    const QStringList fields = splitCsvRow(readCsvRows(file).at(1));
    QCOMPARE(fields.at(ColAdded), torrent.added.toUTC().toString(Qt::ISODate));
    QVERIFY(fields.at(ColAdded).endsWith(QLatin1Char('Z')));
    // "Never scraped" is an empty cell, not an event in 1970.
    QVERIFY(fields.at(ColTrackersChecked).isEmpty());
}

void TestDatabaseExport::testCsvCompanionFileList()
{
    const QString file = path(QStringLiteral("with-files.csv"));

    CsvWriter::Options options;
    options.includeFiles = true;
    CsvWriter writer(options);
    QVERIFY(writer.needsFiles());
    QVERIFY(writer.open(file, makeHeader(3)));
    for (int i = 0; i < 3; ++i)
        QVERIFY(writer.write(makeTorrent(i)));
    QVERIFY(writer.finish());

    QCOMPARE(writer.torrentsWritten(), qint64(3));
    QCOMPARE(writer.filesWritten(), qint64(6)); // two files each

    const QString companion = writer.filesPath();
    QCOMPARE(QFileInfo(companion).fileName(), QStringLiteral("with-files.files.csv"));
    QVERIFY(QFile::exists(companion));

    const QStringList rows = readCsvRows(companion);
    QCOMPARE(rows.size(), 7); // header + six files
    QCOMPARE(splitCsvRow(rows.first()),
        QStringList({ QStringLiteral("hash"), QStringLiteral("path"), QStringLiteral("size") }));

    // A file row joins back to its torrent on hash — that is the whole contract
    // of splitting the export in two.
    const Torrent expected = makeTorrent(0);
    const QStringList first = splitCsvRow(rows.at(1));
    QCOMPARE(first.at(0), expected.hash);
    QCOMPARE(first.at(1), expected.fileList[0].path);
    QCOMPARE(first.at(2), QString::number(expected.fileList[0].size));
}

void TestDatabaseExport::testCsvWithoutFileListWritesNoCompanion()
{
    const QString file = path(QStringLiteral("flat.csv"));

    CsvWriter writer; // default: torrents only
    QVERIFY(!writer.needsFiles());
    QVERIFY(writer.open(file, makeHeader(1)));
    QVERIFY(writer.write(makeTorrent(2))); // carries a file list that must be ignored
    QVERIFY(writer.finish());

    QVERIFY(writer.filesPath().isEmpty());
    QCOMPARE(writer.filesWritten(), qint64(0));
    QVERIFY(!QFile::exists(path(QStringLiteral("flat.files.csv"))));

    // The file *count* still lands: it lives on the torrent row, so skipping the
    // file query costs the flat export nothing.
    QCOMPARE(splitCsvRow(readCsvRows(file).at(1)).at(ColFiles), QStringLiteral("2"));
}

void TestDatabaseExport::testCsvUnfinishedWriteLeavesNoFiles()
{
    const QString file = path(QStringLiteral("unfinished.csv"));
    const QString companion = path(QStringLiteral("unfinished.files.csv"));

    {
        CsvWriter::Options options;
        options.includeFiles = true;
        CsvWriter writer(options);
        QVERIFY(writer.open(file, makeHeader(1)));
        QVERIFY(writer.write(makeTorrent(0)));
        // Destroyed without finish().
    }

    // Both halves go: a companion outliving its export would be worse than none.
    QVERIFY(!QFile::exists(file));
    QVERIFY(!QFile::exists(companion));
}

void TestDatabaseExport::testCsvEmptyExportKeepsHeader()
{
    const QString file = path(QStringLiteral("empty.csv"));

    CsvWriter writer;
    QVERIFY(writer.open(file, makeHeader(0)));
    QVERIFY(writer.finish());

    // A header-only sheet is the right answer for an empty index.
    const QStringList rows = readCsvRows(file);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(splitCsvRow(rows.first()).at(ColHash), QStringLiteral("hash"));
}

void TestDatabaseExport::testFormatFollowsFileExtension()
{
    QVERIFY(exportFormatForPath(QStringLiteral("/tmp/index.csv")) == ExportFormat::Csv);
    QVERIFY(exportFormatForPath(QStringLiteral("/tmp/index.CSV")) == ExportFormat::Csv);
    QVERIFY(exportFormatForPath(QStringLiteral("/tmp/index.jsonl")) == ExportFormat::JsonLines);
    QVERIFY(exportFormatForPath(QStringLiteral("/tmp/index.ndjson")) == ExportFormat::JsonLines);
    QVERIFY(exportFormatForPath(QStringLiteral("/tmp/index.json")) == ExportFormat::JsonLines);
    QVERIFY(exportFormatForPath(QStringLiteral("/tmp/index.ratsdb")) == ExportFormat::Ratsdb);

    // A dotted base name is not an extension.
    QVERIFY(exportFormatForPath(QStringLiteral("/tmp/my.index.csv")) == ExportFormat::Csv);
    // Anything unrecognised stays the format that can be imported back.
    QVERIFY(exportFormatForPath(QStringLiteral("/tmp/index")) == ExportFormat::Ratsdb);
    QVERIFY(exportFormatForPath(QStringLiteral("/tmp/index.bin")) == ExportFormat::Ratsdb);
}

void TestDatabaseExport::testFormatNamesRoundTrip()
{
    for (const ExportFormat format : { ExportFormat::Ratsdb, ExportFormat::Csv, ExportFormat::JsonLines })
        QVERIFY(exportFormatFromName(exportFormatName(format)) == format);

    QCOMPARE(exportFormatName(ExportFormat::Csv), QStringLiteral("csv"));
    QCOMPARE(exportFormatName(ExportFormat::JsonLines), QStringLiteral("jsonl"));
    QVERIFY(exportFormatFromName(QStringLiteral("  CSV ")) == ExportFormat::Csv);
    // An unknown name is rejected, not quietly turned into some other format.
    QVERIFY(!exportFormatFromName(QStringLiteral("xlsx")).has_value());
}

void TestDatabaseExport::testFactoryBuildsTheRightSink()
{
    struct Case {
        ExportFormat format;
        bool includeFiles;
        bool needsFiles;
        QByteArray magic;
        QString name;
    };

    const QList<Case> cases = {
        { ExportFormat::Ratsdb, false, true, QByteArray("RATSDB"), QStringLiteral("sink.ratsdb") },
        { ExportFormat::JsonLines, false, true, QByteArray("{"), QStringLiteral("sink.jsonl") },
        // Only the flat CSV can skip the file query; that is the whole point of
        // needsFiles().
        { ExportFormat::Csv, false, false, QByteArray("\xEF\xBB\xBF"), QStringLiteral("sink-flat.csv") },
        { ExportFormat::Csv, true, true, QByteArray("\xEF\xBB\xBF"), QStringLiteral("sink-files.csv") },
    };

    for (const Case& testCase : cases) {
        ExportOptions options;
        options.includeFiles = testCase.includeFiles;
        const std::unique_ptr<TorrentSink> sink = makeExportSink(testCase.format, options);
        QVERIFY(sink != nullptr);
        QCOMPARE(sink->needsFiles(), testCase.needsFiles);

        // The bytes prove which sink came back, not just its interface.
        const QString file = path(testCase.name);
        QVERIFY(sink->open(file, makeHeader(1)));
        QVERIFY(sink->write(makeTorrent(0)));
        QVERIFY(sink->finish());
        QVERIFY(readAllBytes(file).startsWith(testCase.magic));
    }
}

QTEST_MAIN(TestDatabaseExport)
#include "test_database_export.moc"
