/**
 * @file test_sphinxql.cpp
 * @brief Unit tests for SphinxQL escape and SQL building functions
 * 
 * These tests focus on the pure functions that don't require database connectivity
 */

#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "sphinxql.h"

class TestSphinxQL : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // Escape function tests
    void testEscape_simpleString();
    void testEscape_singleQuote();
    void testEscape_doubleQuote();
    void testEscape_backslash();
    void testEscape_newline();
    void testEscape_tab();
    void testEscape_carriageReturn();
    void testEscape_complexString();
    void testEscape_emptyString();
    
    // EscapeString tests
    void testEscapeString_simple();
    void testEscapeString_withQuotes();
    void testEscapeString_cyrillicWithQuotes();
    void testEscapeString_jsonWithQuotes();
    
    // formatSqlValue tests (private, accessed via friend)
    void testFormatSqlValue_int();
    void testFormatSqlValue_longlong();
    void testFormatSqlValue_double();
    void testFormatSqlValue_string();
    void testFormatSqlValue_stringWithQuotes();
    void testFormatSqlValue_nullVariant();
    void testFormatSqlValue_jsonObject();
    
    // buildRawSql tests (private, accessed via friend)
    void testBuildRawSql_noParams();
    void testBuildRawSql_singleString();
    void testBuildRawSql_intAndString();
    void testBuildRawSql_stringWithQuotes();
    void testBuildRawSql_cyrillicJsonData();
    
    // LastError tests
    void testLastError_initial();

private:
    SphinxQL *sphinxql;
};

void TestSphinxQL::initTestCase()
{
    qDebug() << "Starting SphinxQL tests...";
}

void TestSphinxQL::cleanupTestCase()
{
    qDebug() << "SphinxQL tests completed.";
}

void TestSphinxQL::init()
{
    // Create SphinxQL with null manager - only escape functions will work
    sphinxql = new SphinxQL(nullptr);
}

void TestSphinxQL::cleanup()
{
    delete sphinxql;
    sphinxql = nullptr;
}

// ============================================================================
// Escape function tests
// ============================================================================

void TestSphinxQL::testEscape_simpleString()
{
    QString result = sphinxql->escape("hello world");
    QCOMPARE(result, QString("hello world"));
}

void TestSphinxQL::testEscape_singleQuote()
{
    QString result = sphinxql->escape("it's a test");
    QCOMPARE(result, QString("it\\'s a test"));
}

void TestSphinxQL::testEscape_doubleQuote()
{
    QString result = sphinxql->escape("say \"hello\"");
    QCOMPARE(result, QString("say \\\"hello\\\""));
}

void TestSphinxQL::testEscape_backslash()
{
    QString result = sphinxql->escape("path\\to\\file");
    QCOMPARE(result, QString("path\\\\to\\\\file"));
}

void TestSphinxQL::testEscape_newline()
{
    QString result = sphinxql->escape("line1\nline2");
    QCOMPARE(result, QString("line1\\nline2"));
}

void TestSphinxQL::testEscape_tab()
{
    QString result = sphinxql->escape("col1\tcol2");
    QCOMPARE(result, QString("col1\\tcol2"));
}

void TestSphinxQL::testEscape_carriageReturn()
{
    QString result = sphinxql->escape("line1\rline2");
    QCOMPARE(result, QString("line1\\rline2"));
}

void TestSphinxQL::testEscape_complexString()
{
    QString input = "It's a \"complex\" test\nwith\\special\tchars";
    QString expected = "It\\'s a \\\"complex\\\" test\\nwith\\\\special\\tchars";
    QString result = sphinxql->escape(input);
    QCOMPARE(result, expected);
}

void TestSphinxQL::testEscape_emptyString()
{
    QString result = sphinxql->escape("");
    QVERIFY(result.isEmpty());
}

// ============================================================================
// EscapeString tests
// ============================================================================

void TestSphinxQL::testEscapeString_simple()
{
    QString result = sphinxql->escapeString("hello");
    QCOMPARE(result, QString("'hello'"));
}

void TestSphinxQL::testEscapeString_withQuotes()
{
    QString result = sphinxql->escapeString("it's test");
    QCOMPARE(result, QString("'it\\'s test'"));
}

void TestSphinxQL::testEscapeString_cyrillicWithQuotes()
{
    // Regression test: Cyrillic filenames with quotes caused SphinxQL parse errors
    // because QMYSQL driver doubled quotes ('') instead of backslash-escaping (\')
    QString result = sphinxql->escapeString(QString::fromUtf8("Обучение программированию в 1С' -- 2012.pdf"));
    QCOMPARE(result, QString::fromUtf8("'Обучение программированию в 1С\\' -- 2012.pdf'"));
}

void TestSphinxQL::testEscapeString_jsonWithQuotes()
{
    // Regression test: JSON data containing filenames with single quotes
    QString json = R"({"path":"1C/Кашаев -- 'Обучение' в 1С.pdf","size":1144147})";
    QString result = sphinxql->escapeString(json);
    // Single quotes inside must be backslash-escaped
    QVERIFY(result.startsWith("'"));
    QVERIFY(result.endsWith("'"));
    QVERIFY(!result.contains("''"));  // Must NOT contain doubled quotes
    QVERIFY(result.contains("\\'"));  // Must contain backslash-escaped quotes
}

// ============================================================================
// formatSqlValue tests (private method, accessed via friend)
// ============================================================================

void TestSphinxQL::testFormatSqlValue_int()
{
    QString result = sphinxql->formatSqlValue(QVariant(42));
    QCOMPARE(result, QString("42"));
}

void TestSphinxQL::testFormatSqlValue_longlong()
{
    QString result = sphinxql->formatSqlValue(QVariant(static_cast<qint64>(9876543210LL)));
    QCOMPARE(result, QString("9876543210"));
}

void TestSphinxQL::testFormatSqlValue_double()
{
    QString result = sphinxql->formatSqlValue(QVariant(3.14));
    QCOMPARE(result, QString("3.14"));
}

void TestSphinxQL::testFormatSqlValue_string()
{
    QString result = sphinxql->formatSqlValue(QVariant(QString("hello")));
    QCOMPARE(result, QString("'hello'"));
}

void TestSphinxQL::testFormatSqlValue_stringWithQuotes()
{
    QString result = sphinxql->formatSqlValue(QVariant(QString("it's a 'test'")));
    QCOMPARE(result, QString("'it\\'s a \\'test\\''"));
}

void TestSphinxQL::testFormatSqlValue_nullVariant()
{
    // Null/invalid should produce empty string (Manticore doesn't support NULL)
    QString result = sphinxql->formatSqlValue(QVariant());
    QCOMPARE(result, QString("''"));
}

void TestSphinxQL::testFormatSqlValue_jsonObject()
{
    QJsonObject obj;
    obj["name"] = "test's value";
    obj["size"] = 42;
    QString result = sphinxql->formatSqlValue(QVariant(obj));
    // Should be a quoted, escaped JSON string
    QVERIFY(result.startsWith("'"));
    QVERIFY(result.endsWith("'"));
    // The JSON inside should have backslash-escaped quotes
    QVERIFY(!result.contains("''"));
}

// ============================================================================
// buildRawSql tests (private method, accessed via friend)
// ============================================================================

void TestSphinxQL::testBuildRawSql_noParams()
{
    QString result = sphinxql->buildRawSql("SELECT * FROM test", {});
    QCOMPARE(result, QString("SELECT * FROM test"));
}

void TestSphinxQL::testBuildRawSql_singleString()
{
    QVariantList params;
    params << QVariant(QString("hello"));
    QString result = sphinxql->buildRawSql("SELECT * FROM test WHERE name = ?", params);
    QCOMPARE(result, QString("SELECT * FROM test WHERE name = 'hello'"));
}

void TestSphinxQL::testBuildRawSql_intAndString()
{
    QVariantList params;
    params << QVariant(QString("some data")) << QVariant(123);
    QString result = sphinxql->buildRawSql("INSERT INTO feed (data, id) VALUES (?, ?)", params);
    QCOMPARE(result, QString("INSERT INTO feed (data, id) VALUES ('some data', 123)"));
}

void TestSphinxQL::testBuildRawSql_stringWithQuotes()
{
    // This is the exact scenario that caused the original bug:
    // JSON data with filenames containing single quotes
    QVariantList params;
    params << QVariant(QString(R"({"path":"file's name.txt","size":100})")) << QVariant(1);
    QString result = sphinxql->buildRawSql("INSERT INTO feed (data, id) VALUES (?, ?)", params);
    
    // The single quote in "file's" must be backslash-escaped, NOT doubled
    QVERIFY(result.contains("\\'"));
    QVERIFY(!result.contains("''"));
    QCOMPARE(result, QString("INSERT INTO feed (data, id) VALUES ('{\\\"path\\\":\\\"file\\'s name.txt\\\",\\\"size\\\":100}', 1)"));
}

void TestSphinxQL::testBuildRawSql_cyrillicJsonData()
{
    // Regression test matching the exact error scenario from the bug report:
    // JSON with Cyrillic filenames containing single quotes in "1С'"
    QString jsonData = QString::fromUtf8(
        R"({"files":[{"path":"1C/Кашаев -- 'Обучение программированию в 1С' -- 2012.pdf","size":1144147}]})");
    
    QVariantList params;
    params << QVariant(jsonData) << QVariant(1);
    QString result = sphinxql->buildRawSql("INSERT INTO feed (data, id) VALUES (?, ?)", params);
    
    // Must use backslash escaping for quotes
    QVERIFY(!result.contains("''"));
    QVERIFY(result.contains("\\'"));
    // Must be valid SQL with proper quoting
    QVERIFY(result.startsWith("INSERT INTO feed (data, id) VALUES ('"));
    QVERIFY(result.endsWith(", 1)"));
}

// ============================================================================
// LastError tests
// ============================================================================

void TestSphinxQL::testLastError_initial()
{
    QString error = sphinxql->lastError();
    QVERIFY(error.isEmpty());
}

QTEST_MAIN(TestSphinxQL)
#include "test_sphinxql.moc"
