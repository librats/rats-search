/**
 * @file test_searchresultmodel.cpp
 * @brief Unit tests for SearchResultModel
 */

#include <QtTest/QtTest>
#include <QApplication>
#include <QDateTime>
#include "searchresultmodel.h"

class TestSearchResultModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // Basic model tests
    void testEmptyModel();
    void testColumnCount();
    void testSetResults();
    void testClearResults();
    void testGetTorrent();
    void testGetTorrent_invalidRow();
    void testGetInfoHash();
    void testGetInfoHash_invalidRow();
    
    // Data display tests
    void testDataDisplayRole_name();
    void testDataDisplayRole_seeders();
    void testDataDisplayRole_invalidColumn();
    void testDataInvalidIndex();
    
    // Header tests
    void testHeaderData();
    void testHeaderData_vertical();
    
    // Custom roles tests
    void testCustomRoles_contentType();
    void testCustomRoles_hash();
    
    // Size formatting tests
    void testSizeFormatting_bytes();
    void testSizeFormatting_kilobytes();
    void testSizeFormatting_megabytes();
    void testSizeFormatting_gigabytes();
    void testSizeFormatting_terabytes();
    
private:
    SearchResultModel *model;
    QVector<TorrentInfo> createTestData(int count);
    TorrentInfo createTestTorrent(const QString& hash, const QString& name, qint64 size);
};

void TestSearchResultModel::initTestCase()
{
    qDebug() << "Starting SearchResultModel tests...";
}

void TestSearchResultModel::cleanupTestCase()
{
    qDebug() << "SearchResultModel tests completed.";
}

void TestSearchResultModel::init()
{
    model = new SearchResultModel();
}

void TestSearchResultModel::cleanup()
{
    delete model;
    model = nullptr;
}

QVector<TorrentInfo> TestSearchResultModel::createTestData(int count)
{
    QVector<TorrentInfo> data;
    for (int i = 0; i < count; ++i) {
        TorrentInfo info;
        info.hash = QString("a94a8fe5ccb19ba61c4c0873d391e987982fbbd%1").arg(i, 1, 16);
        info.name = QString("Test Torrent %1").arg(i);
        info.size = 1024 * 1024 * (i + 1);  // i+1 MB
        info.seeders = 100 - i;
        info.leechers = i * 10;
        info.files = i + 1;
        info.added = QDateTime::currentDateTime().addDays(-i);
        info.contentType = "video";
        info.contentCategory = "movie";
        data.append(info);
    }
    return data;
}

TorrentInfo TestSearchResultModel::createTestTorrent(const QString& hash, const QString& name, qint64 size)
{
    TorrentInfo info;
    info.hash = hash;
    info.name = name;
    info.size = size;
    info.seeders = 50;
    info.leechers = 10;
    info.added = QDateTime::currentDateTime();
    return info;
}

// ============================================================================
// Basic model tests
// ============================================================================

void TestSearchResultModel::testEmptyModel()
{
    QCOMPARE(model->rowCount(), 0);
    QCOMPARE(model->columnCount(), static_cast<int>(SearchResultModel::ColumnCount));
}

void TestSearchResultModel::testColumnCount()
{
    QCOMPARE(model->columnCount(), 5);  // Name, Size, Seeders, Leechers, Date
}

void TestSearchResultModel::testSetResults()
{
    QVector<TorrentInfo> data = createTestData(5);
    model->setResults(data);
    
    QCOMPARE(model->rowCount(), 5);
}

void TestSearchResultModel::testClearResults()
{
    QVector<TorrentInfo> data = createTestData(3);
    model->setResults(data);
    QCOMPARE(model->rowCount(), 3);
    
    model->clearResults();
    QCOMPARE(model->rowCount(), 0);
}

void TestSearchResultModel::testGetTorrent()
{
    QVector<TorrentInfo> data = createTestData(3);
    model->setResults(data);
    
    TorrentInfo torrent = model->getTorrent(0);
    QCOMPARE(torrent.name, QString("Test Torrent 0"));
    
    TorrentInfo torrent2 = model->getTorrent(2);
    QCOMPARE(torrent2.name, QString("Test Torrent 2"));
}

void TestSearchResultModel::testGetTorrent_invalidRow()
{
    QVector<TorrentInfo> data = createTestData(3);
    model->setResults(data);
    
    // Negative row
    TorrentInfo invalid1 = model->getTorrent(-1);
    QVERIFY(!invalid1.isValid());
    
    // Row beyond range
    TorrentInfo invalid2 = model->getTorrent(10);
    QVERIFY(!invalid2.isValid());
}

void TestSearchResultModel::testGetInfoHash()
{
    TorrentInfo info = createTestTorrent(
        "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
        "Test",
        1024
    );
    QVector<TorrentInfo> data;
    data.append(info);
    model->setResults(data);
    
    QString hash = model->getInfoHash(0);
    QCOMPARE(hash, QString("a94a8fe5ccb19ba61c4c0873d391e987982fbbd3"));
}

void TestSearchResultModel::testGetInfoHash_invalidRow()
{
    QVERIFY(model->getInfoHash(-1).isEmpty());
    QVERIFY(model->getInfoHash(100).isEmpty());
}

// ============================================================================
// Data display tests
// ============================================================================

void TestSearchResultModel::testDataDisplayRole_name()
{
    TorrentInfo info = createTestTorrent(
        "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
        "My Test Torrent",
        1024
    );
    QVector<TorrentInfo> data;
    data.append(info);
    model->setResults(data);
    
    QModelIndex index = model->index(0, SearchResultModel::NameColumn);
    QVariant nameData = model->data(index, Qt::DisplayRole);
    
    QCOMPARE(nameData.toString(), QString("My Test Torrent"));
}

void TestSearchResultModel::testDataDisplayRole_seeders()
{
    TorrentInfo info = createTestTorrent(
        "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
        "Test",
        1024
    );
    info.seeders = 42;
    QVector<TorrentInfo> data;
    data.append(info);
    model->setResults(data);
    
    QModelIndex index = model->index(0, SearchResultModel::SeedersColumn);
    QVariant seedersData = model->data(index, Qt::DisplayRole);
    
    QCOMPARE(seedersData.toInt(), 42);
}

void TestSearchResultModel::testDataDisplayRole_invalidColumn()
{
    TorrentInfo info = createTestTorrent(
        "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
        "Test",
        1024
    );
    QVector<TorrentInfo> data;
    data.append(info);
    model->setResults(data);
    
    QModelIndex index = model->index(0, 99);  // Invalid column
    QVariant invalidData = model->data(index, Qt::DisplayRole);
    
    QVERIFY(!invalidData.isValid());
}

void TestSearchResultModel::testDataInvalidIndex()
{
    QVector<TorrentInfo> data = createTestData(3);
    model->setResults(data);
    
    // Invalid row
    QModelIndex invalidIndex = model->index(100, 0);
    QVariant data1 = model->data(invalidIndex, Qt::DisplayRole);
    QVERIFY(!data1.isValid());
}

// ============================================================================
// Header tests
// ============================================================================

void TestSearchResultModel::testHeaderData()
{
    QVariant nameHeader = model->headerData(SearchResultModel::NameColumn, Qt::Horizontal, Qt::DisplayRole);
    QCOMPARE(nameHeader.toString(), QString("Name"));
    
    QVariant sizeHeader = model->headerData(SearchResultModel::SizeColumn, Qt::Horizontal, Qt::DisplayRole);
    QCOMPARE(sizeHeader.toString(), QString("Size"));
    
    QVariant seedersHeader = model->headerData(SearchResultModel::SeedersColumn, Qt::Horizontal, Qt::DisplayRole);
    QCOMPARE(seedersHeader.toString(), QString("Seeders"));
    
    QVariant leechersHeader = model->headerData(SearchResultModel::LeechersColumn, Qt::Horizontal, Qt::DisplayRole);
    QCOMPARE(leechersHeader.toString(), QString("Leechers"));
    
    QVariant dateHeader = model->headerData(SearchResultModel::DateColumn, Qt::Horizontal, Qt::DisplayRole);
    QCOMPARE(dateHeader.toString(), QString("Date"));
}

void TestSearchResultModel::testHeaderData_vertical()
{
    // Vertical headers should return invalid
    QVariant header = model->headerData(0, Qt::Vertical, Qt::DisplayRole);
    QVERIFY(!header.isValid());
}

// ============================================================================
// Custom roles tests
// ============================================================================

void TestSearchResultModel::testCustomRoles_contentType()
{
    TorrentInfo info = createTestTorrent(
        "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
        "Test",
        1024
    );
    info.contentType = "video";
    QVector<TorrentInfo> data;
    data.append(info);
    model->setResults(data);
    
    QModelIndex index = model->index(0, 0);
    QVariant contentType = model->data(index, Qt::UserRole + 1);
    
    QCOMPARE(contentType.toString(), QString("video"));
}

void TestSearchResultModel::testCustomRoles_hash()
{
    TorrentInfo info = createTestTorrent(
        "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
        "Test",
        1024
    );
    QVector<TorrentInfo> data;
    data.append(info);
    model->setResults(data);
    
    QModelIndex index = model->index(0, 0);
    QVariant hash = model->data(index, Qt::UserRole + 5);
    
    QCOMPARE(hash.toString(), QString("a94a8fe5ccb19ba61c4c0873d391e987982fbbd3"));
}

// ============================================================================
// Size formatting tests (using data display)
// ============================================================================

void TestSearchResultModel::testSizeFormatting_bytes()
{
    TorrentInfo info = createTestTorrent(
        "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
        "Test",
        500  // 500 bytes
    );
    QVector<TorrentInfo> data;
    data.append(info);
    model->setResults(data);
    
    QModelIndex index = model->index(0, SearchResultModel::SizeColumn);
    QString size = model->data(index, Qt::DisplayRole).toString();
    
    QCOMPARE(size, QString("500 B"));
}

void TestSearchResultModel::testSizeFormatting_kilobytes()
{
    TorrentInfo info = createTestTorrent(
        "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
        "Test",
        2048  // 2 KB
    );
    QVector<TorrentInfo> data;
    data.append(info);
    model->setResults(data);
    
    QModelIndex index = model->index(0, SearchResultModel::SizeColumn);
    QString size = model->data(index, Qt::DisplayRole).toString();
    
    QCOMPARE(size, QString("2.00 KB"));
}

void TestSearchResultModel::testSizeFormatting_megabytes()
{
    TorrentInfo info = createTestTorrent(
        "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
        "Test",
        5 * 1024 * 1024  // 5 MB
    );
    QVector<TorrentInfo> data;
    data.append(info);
    model->setResults(data);
    
    QModelIndex index = model->index(0, SearchResultModel::SizeColumn);
    QString size = model->data(index, Qt::DisplayRole).toString();
    
    QCOMPARE(size, QString("5.00 MB"));
}

void TestSearchResultModel::testSizeFormatting_gigabytes()
{
    TorrentInfo info = createTestTorrent(
        "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
        "Test",
        (qint64)3 * 1024 * 1024 * 1024  // 3 GB
    );
    QVector<TorrentInfo> data;
    data.append(info);
    model->setResults(data);
    
    QModelIndex index = model->index(0, SearchResultModel::SizeColumn);
    QString size = model->data(index, Qt::DisplayRole).toString();
    
    QCOMPARE(size, QString("3.00 GB"));
}

void TestSearchResultModel::testSizeFormatting_terabytes()
{
    TorrentInfo info = createTestTorrent(
        "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
        "Test",
        (qint64)2 * 1024 * 1024 * 1024 * 1024  // 2 TB
    );
    QVector<TorrentInfo> data;
    data.append(info);
    model->setResults(data);
    
    QModelIndex index = model->index(0, SearchResultModel::SizeColumn);
    QString size = model->data(index, Qt::DisplayRole).toString();
    
    QCOMPARE(size, QString("2.00 TB"));
}

// Need QApplication for model testing
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    TestSearchResultModel test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_searchresultmodel.moc"

