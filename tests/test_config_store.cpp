#include "app/config_store.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using rats::app::ConfigStore;

// ConfigStore is the only write path for settings, and Application::applyConfig()
// hangs off configChanged(). These tests pin that contract: every successful
// write notifies, whichever entry point produced it (typed setter from the
// settings dialog, or fromJson from the `config.set` API method).
class TestConfigStore : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void typedSetter_emitsConfigChanged();
    void typedSetter_nestedKey_emitsConfigChanged();
    void unchangedValue_doesNotEmit();
    void fromJson_emitsOneBatchedSignal();
    void p2pConnections_isClamped();
    void replicationImpliesReplicationServer();
    void disablingReplicationServer_disablesReplication();
    void languageAndDarkMode_haveDedicatedSignals();
    void saveThenLoad_roundTrips();

private:
    QTemporaryDir* dir_ = nullptr;
    ConfigStore* config_ = nullptr;
    QString path_ = {};
};

void TestConfigStore::init()
{
    dir_ = new QTemporaryDir();
    QVERIFY(dir_->isValid());
    path_ = dir_->path() + "/rats.json";
    config_ = new ConfigStore(path_);
}

void TestConfigStore::cleanup()
{
    delete config_;
    config_ = nullptr;
    delete dir_;
    dir_ = nullptr;
}

void TestConfigStore::typedSetter_emitsConfigChanged()
{
    QSignalSpy spy(config_, &ConfigStore::configChanged);
    config_->setIndexerEnabled(false);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toStringList(), QStringList { "indexer" });
    QVERIFY(!config_->indexerEnabled());
}

void TestConfigStore::typedSetter_nestedKey_emitsConfigChanged()
{
    QSignalSpy spy(config_, &ConfigStore::configChanged);
    config_->setFiltersAdultFilter(true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toStringList(), QStringList { "filters.adultFilter" });
    QVERIFY(config_->filtersAdultFilter());
}

void TestConfigStore::unchangedValue_doesNotEmit()
{
    config_->setSpiderWalkInterval(250);

    QSignalSpy spy(config_, &ConfigStore::configChanged);
    config_->setSpiderWalkInterval(250); // same value
    QCOMPARE(spy.count(), 0);
}

void TestConfigStore::fromJson_emitsOneBatchedSignal()
{
    QSignalSpy spy(config_, &ConfigStore::configChanged);
    const QStringList changed = config_->fromJson(QJsonObject { { "trackers", false }, { "restApi", true } });

    QCOMPARE(spy.count(), 1); // batched, not one per key
    QCOMPARE(changed.size(), 2);
    QVERIFY(!config_->trackersEnabled());
    QVERIFY(config_->restApiEnabled());
}

void TestConfigStore::p2pConnections_isClamped()
{
    config_->setP2pConnections(5);
    QCOMPARE(config_->p2pConnections(), 10);

    config_->setP2pConnections(50000);
    QCOMPARE(config_->p2pConnections(), 1000);

    // A second out-of-range write dedupes against the already-clamped value.
    QSignalSpy spy(config_, &ConfigStore::configChanged);
    config_->setP2pConnections(50001);
    QCOMPARE(spy.count(), 0);
}

void TestConfigStore::replicationImpliesReplicationServer()
{
    config_->setP2pReplicationServer(false);
    QVERIFY(!config_->p2pReplicationServer());

    config_->setP2pReplication(true);
    QVERIFY(config_->p2pReplication());
    QVERIFY(config_->p2pReplicationServer()); // pulled along
}

void TestConfigStore::disablingReplicationServer_disablesReplication()
{
    config_->setP2pReplication(true);
    config_->setP2pReplicationServer(false);

    QVERIFY(!config_->p2pReplicationServer());
    QVERIFY(!config_->p2pReplication());
}

void TestConfigStore::languageAndDarkMode_haveDedicatedSignals()
{
    QSignalSpy languageSpy(config_, &ConfigStore::languageChanged);
    QSignalSpy darkModeSpy(config_, &ConfigStore::darkModeChanged);

    config_->setLanguage("ru");
    config_->setDarkMode(true);

    QCOMPARE(languageSpy.count(), 1);
    QCOMPARE(languageSpy.at(0).at(0).toString(), QStringLiteral("ru"));
    QCOMPARE(darkModeSpy.count(), 1);
    QCOMPARE(darkModeSpy.at(0).at(0).toBool(), true);
}

void TestConfigStore::saveThenLoad_roundTrips()
{
    config_->setFiltersSizeMin(1024);
    config_->setDownloadPath("/tmp/rats-downloads");
    QVERIFY(config_->save());

    ConfigStore reloaded(path_);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.filtersSizeMin(), 1024LL);
    QCOMPARE(reloaded.downloadPath(), QStringLiteral("/tmp/rats-downloads"));
}

QTEST_MAIN(TestConfigStore)
#include "test_config_store.moc"
