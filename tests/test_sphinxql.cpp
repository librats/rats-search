/**
 * @file test_sphinxql.cpp
 * @brief Unit tests for SphinxQL escape and SQL building functions
 * 
 * These tests focus on the pure functions that don't require database connectivity
 */

#include <QtTest/QtTest>
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
