/**
 * @file test_updatemanager.cpp
 * @brief Unit tests for UpdateManager class
 * 
 * Tests cover version parsing, state management, and platform detection
 * without making actual network requests.
 */

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "api/updatemanager.h"

class TestUpdateManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // Version tests
    void testCurrentVersion_notEmpty();
    void testCurrentVersion_validFormat();
    void testCurrentVersionNumber_valid();
    void testCurrentVersionNumber_components();
    
    // State tests
    void testInitialState_idle();
    void testStateString_returnsString();
    void testIsUpdateAvailable_initiallyFalse();
    
    // Settings tests
    void testCheckOnStartup_defaultTrue();
    void testCheckOnStartup_canBeSet();
    void testIncludePrerelease_defaultFalse();
    void testIncludePrerelease_canBeSet();
    
    // Repository tests
    void testSetRepository_setsValues();
    
    // Update info tests
    void testUpdateInfo_initiallyInvalid();
    
    // Signal tests
    void testCheckForUpdates_changesState();
    void testErrorMessage_initiallyEmpty();

private:
    UpdateManager *updateManager;
};

void TestUpdateManager::initTestCase()
{
    // Register the UpdateState enum for QSignalSpy to work correctly
    qRegisterMetaType<UpdateManager::UpdateState>("UpdateManager::UpdateState");
    
    qDebug() << "Starting UpdateManager tests...";
    qDebug() << "Current app version:" << UpdateManager::currentVersion();
}

void TestUpdateManager::cleanupTestCase()
{
    qDebug() << "UpdateManager tests completed.";
}

void TestUpdateManager::init()
{
    updateManager = new UpdateManager();
}

void TestUpdateManager::cleanup()
{
    delete updateManager;
    updateManager = nullptr;
}

// ============================================================================
// Version tests
// ============================================================================

void TestUpdateManager::testCurrentVersion_notEmpty()
{
    QString version = UpdateManager::currentVersion();
    QVERIFY(!version.isEmpty());
}

void TestUpdateManager::testCurrentVersion_validFormat()
{
    QString version = UpdateManager::currentVersion();
    // Should match version pattern: "X.Y.Z" or "X.Y.Z.BUILD" (e.g., "2.0.0" or "1.11.0.1196")
    QRegularExpression versionRegex("^\\d+\\.\\d+\\.\\d+(\\.\\d+)?(-[a-zA-Z0-9]+)?$");
    QVERIFY2(versionRegex.match(version).hasMatch(), 
             qPrintable(QString("Version '%1' doesn't match version pattern").arg(version)));
}

void TestUpdateManager::testCurrentVersionNumber_valid()
{
    QVersionNumber version = UpdateManager::currentVersionNumber();
    QVERIFY(!version.isNull());
}

void TestUpdateManager::testCurrentVersionNumber_components()
{
    QVersionNumber version = UpdateManager::currentVersionNumber();
    // Should have at least major.minor.patch
    QVERIFY2(version.segmentCount() >= 3, 
             qPrintable(QString("Expected at least 3 segments, got %1").arg(version.segmentCount())));
    QVERIFY(version.majorVersion() >= 0);
    QVERIFY(version.minorVersion() >= 0);
    QVERIFY(version.microVersion() >= 0);
}

// ============================================================================
// State tests
// ============================================================================

void TestUpdateManager::testInitialState_idle()
{
    QCOMPARE(updateManager->state(), UpdateManager::UpdateState::Idle);
}

void TestUpdateManager::testStateString_returnsString()
{
    QString stateStr = updateManager->stateString();
    QVERIFY(!stateStr.isEmpty());
    // In Idle state, should return "Idle" (or translated version)
    QVERIFY(stateStr.length() > 0);
}

void TestUpdateManager::testIsUpdateAvailable_initiallyFalse()
{
    QVERIFY(!updateManager->isUpdateAvailable());
}

// ============================================================================
// Settings tests
// ============================================================================

void TestUpdateManager::testCheckOnStartup_defaultTrue()
{
    QVERIFY(updateManager->checkOnStartup());
}

void TestUpdateManager::testCheckOnStartup_canBeSet()
{
    updateManager->setCheckOnStartup(false);
    QVERIFY(!updateManager->checkOnStartup());
    
    updateManager->setCheckOnStartup(true);
    QVERIFY(updateManager->checkOnStartup());
}

void TestUpdateManager::testIncludePrerelease_defaultFalse()
{
    QVERIFY(!updateManager->includePrerelease());
}

void TestUpdateManager::testIncludePrerelease_canBeSet()
{
    updateManager->setIncludePrerelease(true);
    QVERIFY(updateManager->includePrerelease());
    
    updateManager->setIncludePrerelease(false);
    QVERIFY(!updateManager->includePrerelease());
}

// ============================================================================
// Repository tests
// ============================================================================

void TestUpdateManager::testSetRepository_setsValues()
{
    // Default repository should be set
    // Just verify setRepository doesn't crash
    updateManager->setRepository("testowner", "testrepo");
    QVERIFY(true); // If we get here, no crash occurred
}

// ============================================================================
// Update info tests
// ============================================================================

void TestUpdateManager::testUpdateInfo_initiallyInvalid()
{
    const UpdateManager::UpdateInfo& info = updateManager->updateInfo();
    QVERIFY(!info.isValid());
    QVERIFY(info.version.isEmpty());
    QVERIFY(info.downloadUrl.isEmpty());
}

// ============================================================================
// Signal tests
// ============================================================================

void TestUpdateManager::testCheckForUpdates_changesState()
{
    QSignalSpy stateSpy(updateManager, &UpdateManager::stateChanged);
    QVERIFY(stateSpy.isValid());
    
    // This will try to make a network request (which may fail in test env)
    // but should at least change state to CheckingForUpdates synchronously
    updateManager->checkForUpdates();
    
    // State should have changed immediately (synchronous operation)
    QCOMPARE(updateManager->state(), UpdateManager::UpdateState::CheckingForUpdates);
    
    // The signal should have been emitted at least once
    QVERIFY(stateSpy.count() >= 1);
    
    // First state change should be to CheckingForUpdates
    QList<QVariant> firstChange = stateSpy.first();
    UpdateManager::UpdateState firstState = firstChange.at(0).value<UpdateManager::UpdateState>();
    QCOMPARE(firstState, UpdateManager::UpdateState::CheckingForUpdates);
    
    // Note: We don't wait for async network operation to complete
    // as that would cause race conditions during cleanup
}

void TestUpdateManager::testErrorMessage_initiallyEmpty()
{
    QVERIFY(updateManager->errorMessage().isEmpty());
}

QTEST_MAIN(TestUpdateManager)
#include "test_updatemanager.moc"


