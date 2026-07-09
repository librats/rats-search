/**
 * @file test_infohash.cpp
 * @brief Unit tests for rats::infohash validation/normalisation helpers.
 *        Replaces the old test_utils.cpp (which tested hash length on the
 *        now-deleted TorrentInfo struct).
 */

#include <QtTest/QtTest>

#include "common/infohash.h"

class TestInfohash : public QObject {
    Q_OBJECT

private slots:
    void testIsValid_valid();
    void testIsValid_validUppercase();
    void testIsValid_empty();
    void testIsValid_tooShort();
    void testIsValid_tooLong();
    void testIsValid_nonHex();
    void testIsValid_nonHexBoundary();
    void testNormalize_lowercases();
    void testNormalize_trims();
    void testLengthConstant();
};

void TestInfohash::testIsValid_valid()
{
    QVERIFY(rats::infohash::isValid("a94a8fe5ccb19ba61c4c0873d391e987982fbbd3"));
}

void TestInfohash::testIsValid_validUppercase()
{
    QVERIFY(rats::infohash::isValid("A94A8FE5CCB19BA61C4C0873D391E987982FBBD3"));
}

void TestInfohash::testIsValid_empty()
{
    QVERIFY(!rats::infohash::isValid(""));
}

void TestInfohash::testIsValid_tooShort()
{
    QVERIFY(!rats::infohash::isValid("abc123"));
    QVERIFY(!rats::infohash::isValid("a94a8fe5ccb19ba61c4c0873d391e987982fbbd")); // 39
}

void TestInfohash::testIsValid_tooLong()
{
    QVERIFY(!rats::infohash::isValid("a94a8fe5ccb19ba61c4c0873d391e987982fbbd3a")); // 41
}

void TestInfohash::testIsValid_nonHex()
{
    // 40 chars but with a non-hex letter 'g'/'z'
    QVERIFY(!rats::infohash::isValid("z94a8fe5ccb19ba61c4c0873d391e987982fbbdg"));
}

void TestInfohash::testIsValid_nonHexBoundary()
{
    // 'g' is just past 'f' — must be rejected
    QString h(40, 'g');
    QVERIFY(!rats::infohash::isValid(h));
    // all 'f' is valid hex
    QVERIFY(rats::infohash::isValid(QString(40, 'f')));
    // all digits is valid hex
    QVERIFY(rats::infohash::isValid(QString(40, '0')));
}

void TestInfohash::testNormalize_lowercases()
{
    QCOMPARE(rats::infohash::normalize("A94A8FE5CCB19BA61C4C0873D391E987982FBBD3"),
        QString("a94a8fe5ccb19ba61c4c0873d391e987982fbbd3"));
}

void TestInfohash::testNormalize_trims()
{
    QCOMPARE(rats::infohash::normalize("  ABCDEF  "), QString("abcdef"));
}

void TestInfohash::testLengthConstant()
{
    QCOMPARE(rats::infohash::kLength, 40);
}

QTEST_MAIN(TestInfohash)
#include "test_infohash.moc"
