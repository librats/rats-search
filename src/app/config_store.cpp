#include "app/config_store.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStandardPaths>
#include <functional>

namespace rats {
namespace app {

namespace {

// Per-key metadata driving the single write path. A key only needs an entry if
// it clamps, emits a specific signal, or belongs to a signal group. Silent keys
// (restApi, upnp, spider.*, ...) have no entry and fall
// through to the plain dedupe/write/dirty path in setValue().
using EmitFn = std::function<void(ConfigStore*, const QVariant&)>;
using GroupFn = std::function<void(ConfigStore*)>;

struct KeyMeta {
    bool hasClamp = false;
    qint64 clampMin = 0;
    qint64 clampMax = 0;
    EmitFn emitSignal; // per-key NOTIFY signal, or null
    GroupFn emitGroup; // group signal (e.g. filtersChanged), or null
};

const QHash<QString, KeyMeta>& keyMetadata()
{
    static const QHash<QString, KeyMeta> table = [] {
        QHash<QString, KeyMeta> m;

        auto emitOnly = [](EmitFn fn) -> KeyMeta {
            KeyMeta k;
            k.emitSignal = std::move(fn);
            return k;
        };
        auto clampInt = [](qint64 lo, qint64 hi, EmitFn fn) -> KeyMeta {
            KeyMeta k;
            k.hasClamp = true;
            k.clampMin = lo;
            k.clampMax = hi;
            k.emitSignal = std::move(fn);
            return k;
        };
        auto emitFilter = [](EmitFn fn) -> KeyMeta {
            KeyMeta k;
            k.emitSignal = std::move(fn);
            k.emitGroup = [](ConfigStore* c) { emit c->filtersChanged(); };
            return k;
        };

        // Network
        m.insert("httpPort", emitOnly([](ConfigStore* c, const QVariant& v) { emit c->httpPortChanged(v.toInt()); }));
        m.insert("p2pPort", emitOnly([](ConfigStore* c, const QVariant& v) { emit c->p2pPortChanged(v.toInt()); }));
        m.insert("dhtPort", emitOnly([](ConfigStore* c, const QVariant& v) { emit c->dhtPortChanged(v.toInt()); }));

        // P2P
        m.insert("p2p", emitOnly([](ConfigStore* c, const QVariant& v) { emit c->p2pEnabledChanged(v.toBool()); }));
        m.insert("p2pConnections",
            clampInt(10, 1000, [](ConfigStore* c, const QVariant& v) { emit c->p2pConnectionsChanged(v.toInt()); }));
        m.insert("p2pReplication",
            emitOnly([](ConfigStore* c, const QVariant& v) { emit c->p2pReplicationChanged(v.toBool()); }));

        // Indexer
        m.insert(
            "indexer", emitOnly([](ConfigStore* c, const QVariant& v) { emit c->indexerEnabledChanged(v.toBool()); }));
        m.insert("trackers",
            emitOnly([](ConfigStore* c, const QVariant& v) { emit c->trackersEnabledChanged(v.toBool()); }));

        // Filters (each also fires the filtersChanged group signal)
        m.insert("filters.maxFiles",
            emitFilter([](ConfigStore* c, const QVariant& v) { emit c->filtersMaxFilesChanged(v.toInt()); }));
        m.insert("filters.namingRegExp",
            emitFilter([](ConfigStore* c, const QVariant& v) { emit c->filtersNamingRegExpChanged(v.toString()); }));
        m.insert("filters.namingRegExpNegative", emitFilter([](ConfigStore* c, const QVariant& v) {
            emit c->filtersNamingRegExpNegativeChanged(v.toBool());
        }));
        m.insert("filters.adultFilter",
            emitFilter([](ConfigStore* c, const QVariant& v) { emit c->filtersAdultFilterChanged(v.toBool()); }));
        m.insert("filters.sizeMin",
            emitFilter([](ConfigStore* c, const QVariant& v) { emit c->filtersSizeMinChanged(v.toLongLong()); }));
        m.insert("filters.sizeMax",
            emitFilter([](ConfigStore* c, const QVariant& v) { emit c->filtersSizeMaxChanged(v.toLongLong()); }));
        m.insert("filters.contentType",
            emitFilter([](ConfigStore* c, const QVariant& v) { emit c->filtersContentTypeChanged(v.toString()); }));

        // UI
        m.insert(
            "language", emitOnly([](ConfigStore* c, const QVariant& v) { emit c->languageChanged(v.toString()); }));
        m.insert("darkMode", emitOnly([](ConfigStore* c, const QVariant& v) { emit c->darkModeChanged(v.toBool()); }));
        m.insert(
            "trayOnClose", emitOnly([](ConfigStore* c, const QVariant& v) { emit c->trayOnCloseChanged(v.toBool()); }));
        m.insert("trayOnMinimize",
            emitOnly([](ConfigStore* c, const QVariant& v) { emit c->trayOnMinimizeChanged(v.toBool()); }));
        m.insert("startMinimized",
            emitOnly([](ConfigStore* c, const QVariant& v) { emit c->startMinimizedChanged(v.toBool()); }));
        m.insert(
            "autoStart", emitOnly([](ConfigStore* c, const QVariant& v) { emit c->autoStartChanged(v.toBool()); }));
        m.insert("checkUpdatesOnStartup",
            emitOnly([](ConfigStore* c, const QVariant& v) { emit c->checkUpdatesOnStartupChanged(v.toBool()); }));
        m.insert("dataDirectory",
            emitOnly([](ConfigStore* c, const QVariant& v) { emit c->dataDirectoryChanged(v.toString()); }));
        m.insert("agreementAccepted",
            emitOnly([](ConfigStore* c, const QVariant& v) { emit c->agreementAcceptedChanged(v.toBool()); }));

        return m;
    }();
    return table;
}

const KeyMeta* metaForKey(const QString& key)
{
    const auto& table = keyMetadata();
    auto it = table.find(key);
    return it != table.end() ? &it.value() : nullptr;
}

} // namespace

ConfigStore::ConfigStore(const QString& configPath, QObject* parent) : QObject(parent), configPath_(configPath)
{
    setDefaults();

    if (configPath_.isEmpty()) {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDir);
        configPath_ = dataDir + "/rats.json";
    }
}

ConfigStore::~ConfigStore()
{
    if (dirty_) {
        save();
    }
}

void ConfigStore::setDefaults()
{
    config_ = QJsonObject { // Network
        { "httpPort", 8095 }, { "p2pPort", 4444 }, { "dhtPort", 6881 },

        // P2P
        { "p2p", true }, { "p2pConnections", 10 }, { "p2pReplication", true }, { "p2pReplicationServer", true },

        // Indexer
        { "indexer", true }, { "trackers", true }, { "restApi", false }, { "upnp", true },

        // Spider
        { "spider", QJsonObject { { "walkInterval", 100 } } },

        // Filters
        { "filters",
            QJsonObject { { "maxFiles", 0 }, { "namingRegExp", "" }, { "namingRegExpNegative", false },
                { "adultFilter", false }, { "sizeMin", 0 }, { "sizeMax", 0 }, { "contentType", "" } } },

        // Downloads
        { "downloadPath", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) },

        // UI
        { "language", "en" }, { "darkMode", false }, { "trayOnClose", false }, { "trayOnMinimize", true },
        { "startMinimized", false }, { "autoStart", false }, { "checkUpdatesOnStartup", true },
        { "dataDirectory", QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) },

        // Legal
        { "agreementAccepted", false }
    };
}

bool ConfigStore::load()
{
    QFile file(configPath_);
    if (!file.exists()) {
        qInfo() << "Config file not found, using defaults:" << configPath_;
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config file:" << configPath_;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse config:" << error.errorString();
        return false;
    }

    // Merge loaded config with defaults
    QJsonObject loaded = doc.object();
    for (auto it = loaded.begin(); it != loaded.end(); ++it) {
        if (it.value().isObject() && config_.value(it.key()).isObject()) {
            // Merge nested objects
            QJsonObject base = config_.value(it.key()).toObject();
            QJsonObject overlay = it.value().toObject();
            for (auto oit = overlay.begin(); oit != overlay.end(); ++oit) {
                base[oit.key()] = oit.value();
            }
            config_[it.key()] = base;
        } else {
            config_[it.key()] = it.value();
        }
    }

    validateAndClamp();
    qInfo() << "Config loaded from" << configPath_;
    return true;
}

bool ConfigStore::save()
{
    QFile file(configPath_);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to save config to" << configPath_;
        return false;
    }

    QJsonDocument doc(config_);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    dirty_ = false;
    qInfo() << "Config saved to" << configPath_;
    return true;
}

void ConfigStore::reset()
{
    setDefaults();
    dirty_ = true;
    emit configChanged(config_.keys());
}

void ConfigStore::validateAndClamp()
{
    // Clamp p2pConnections
    int p2pConn = config_["p2pConnections"].toInt();
    if (p2pConn < 10)
        config_["p2pConnections"] = 10;
    if (p2pConn > 1000)
        config_["p2pConnections"] = 1000;

    // P2P replication dependency
    if (config_["p2pReplication"].toBool() && !config_["p2pReplicationServer"].toBool()) {
        config_["p2pReplicationServer"] = true;
    }
    if (!config_["p2pReplicationServer"].toBool() && config_["p2pReplication"].toBool()) {
        config_["p2pReplication"] = false;
    }
}

// ============================================================================
// Nested-aware JSON accessors
// ============================================================================

QJsonValue ConfigStore::readJsonValue(const QString& key) const
{
    int dot = key.indexOf('.');
    if (dot < 0) {
        return config_.value(key);
    }
    QString group = key.left(dot);
    QString sub = key.mid(dot + 1);
    return config_.value(group).toObject().value(sub);
}

void ConfigStore::writeJsonValue(const QString& key, const QJsonValue& jsonValue)
{
    int dot = key.indexOf('.');
    if (dot < 0) {
        config_[key] = jsonValue;
        return;
    }
    QString group = key.left(dot);
    QString sub = key.mid(dot + 1);
    QJsonObject obj = config_.value(group).toObject();
    obj[sub] = jsonValue;
    config_[group] = obj;
}

// ============================================================================
// Network Settings
// ============================================================================

int ConfigStore::httpPort() const
{
    return config_["httpPort"].toInt(8095);
}
void ConfigStore::setHttpPort(int port)
{
    setValue("httpPort", port);
}

int ConfigStore::p2pPort() const
{
    return config_["p2pPort"].toInt(4444);
}
void ConfigStore::setP2pPort(int port)
{
    setValue("p2pPort", port);
}

int ConfigStore::dhtPort() const
{
    return config_["dhtPort"].toInt(6881);
}
void ConfigStore::setDhtPort(int port)
{
    setValue("dhtPort", port);
}

// ============================================================================
// P2P Settings
// ============================================================================

bool ConfigStore::p2pEnabled() const
{
    return config_["p2p"].toBool(true);
}
void ConfigStore::setP2pEnabled(bool enabled)
{
    setValue("p2p", enabled);
}

int ConfigStore::p2pConnections() const
{
    return config_["p2pConnections"].toInt(10);
}
void ConfigStore::setP2pConnections(int connections)
{
    setValue("p2pConnections", connections);
}

bool ConfigStore::p2pReplication() const
{
    return config_["p2pReplication"].toBool(true);
}
void ConfigStore::setP2pReplication(bool enabled)
{
    if (enabled) {
        setP2pReplicationServer(true); // Dependency
    }
    setValue("p2pReplication", enabled);
}

bool ConfigStore::p2pReplicationServer() const
{
    return config_["p2pReplicationServer"].toBool(true);
}
void ConfigStore::setP2pReplicationServer(bool enabled)
{
    if (!enabled) {
        setValue("p2pReplication", false); // Dependency
    }
    setValue("p2pReplicationServer", enabled);
}

// ============================================================================
// Indexer Settings
// ============================================================================

bool ConfigStore::indexerEnabled() const
{
    return config_["indexer"].toBool(true);
}
void ConfigStore::setIndexerEnabled(bool enabled)
{
    setValue("indexer", enabled);
}

bool ConfigStore::trackersEnabled() const
{
    return config_["trackers"].toBool(true);
}
void ConfigStore::setTrackersEnabled(bool enabled)
{
    setValue("trackers", enabled);
}

bool ConfigStore::restApiEnabled() const
{
    return config_["restApi"].toBool(false);
}
void ConfigStore::setRestApiEnabled(bool enabled)
{
    setValue("restApi", enabled);
}

bool ConfigStore::upnpEnabled() const
{
    return config_["upnp"].toBool(true);
}
// ============================================================================
// Spider Settings
// ============================================================================

int ConfigStore::spiderWalkInterval() const
{
    return config_["spider"].toObject()["walkInterval"].toInt(100);
}
void ConfigStore::setSpiderWalkInterval(int interval)
{
    setValue("spider.walkInterval", interval);
}

// ============================================================================
// Filter Settings
// ============================================================================

int ConfigStore::filtersMaxFiles() const
{
    return config_["filters"].toObject()["maxFiles"].toInt(0);
}
void ConfigStore::setFiltersMaxFiles(int max)
{
    setValue("filters.maxFiles", max);
}

QString ConfigStore::filtersNamingRegExp() const
{
    return config_["filters"].toObject()["namingRegExp"].toString();
}
void ConfigStore::setFiltersNamingRegExp(const QString& regexp)
{
    setValue("filters.namingRegExp", regexp);
}

bool ConfigStore::filtersNamingRegExpNegative() const
{
    return config_["filters"].toObject()["namingRegExpNegative"].toBool(false);
}
void ConfigStore::setFiltersNamingRegExpNegative(bool negative)
{
    setValue("filters.namingRegExpNegative", negative);
}

bool ConfigStore::filtersAdultFilter() const
{
    return config_["filters"].toObject()["adultFilter"].toBool(false);
}
void ConfigStore::setFiltersAdultFilter(bool enabled)
{
    setValue("filters.adultFilter", enabled);
}

qint64 ConfigStore::filtersSizeMin() const
{
    return config_["filters"].toObject()["sizeMin"].toVariant().toLongLong();
}
void ConfigStore::setFiltersSizeMin(qint64 min)
{
    setValue("filters.sizeMin", QVariant(static_cast<qlonglong>(min)));
}

qint64 ConfigStore::filtersSizeMax() const
{
    return config_["filters"].toObject()["sizeMax"].toVariant().toLongLong();
}
void ConfigStore::setFiltersSizeMax(qint64 max)
{
    setValue("filters.sizeMax", QVariant(static_cast<qlonglong>(max)));
}

QString ConfigStore::filtersContentType() const
{
    return config_["filters"].toObject()["contentType"].toString();
}
void ConfigStore::setFiltersContentType(const QString& type)
{
    setValue("filters.contentType", type);
}

// ============================================================================
// Client Settings
// ============================================================================

QString ConfigStore::downloadPath() const
{
    return config_["downloadPath"].toString(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
}
void ConfigStore::setDownloadPath(const QString& path)
{
    setValue("downloadPath", path);
}

// ============================================================================
// UI Settings
// ============================================================================

QString ConfigStore::language() const
{
    return config_["language"].toString("en");
}
void ConfigStore::setLanguage(const QString& lang)
{
    setValue("language", lang);
}

bool ConfigStore::darkMode() const
{
    return config_["darkMode"].toBool(false);
}
void ConfigStore::setDarkMode(bool enabled)
{
    setValue("darkMode", enabled);
}

bool ConfigStore::trayOnClose() const
{
    return config_["trayOnClose"].toBool(false);
}
void ConfigStore::setTrayOnClose(bool enabled)
{
    setValue("trayOnClose", enabled);
}

bool ConfigStore::trayOnMinimize() const
{
    return config_["trayOnMinimize"].toBool(true);
}
void ConfigStore::setTrayOnMinimize(bool enabled)
{
    setValue("trayOnMinimize", enabled);
}

bool ConfigStore::startMinimized() const
{
    return config_["startMinimized"].toBool(false);
}
void ConfigStore::setStartMinimized(bool enabled)
{
    setValue("startMinimized", enabled);
}

bool ConfigStore::autoStart() const
{
    return config_["autoStart"].toBool(false);
}
void ConfigStore::setAutoStart(bool enabled)
{
    setValue("autoStart", enabled);
}

bool ConfigStore::checkUpdatesOnStartup() const
{
    return config_["checkUpdatesOnStartup"].toBool(true);
}
void ConfigStore::setCheckUpdatesOnStartup(bool enabled)
{
    setValue("checkUpdatesOnStartup", enabled);
}

QString ConfigStore::dataDirectory() const
{
    return config_["dataDirectory"].toString(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
}
void ConfigStore::setDataDirectory(const QString& path)
{
    setValue("dataDirectory", path);
}

bool ConfigStore::agreementAccepted() const
{
    return config_["agreementAccepted"].toBool(false);
}
void ConfigStore::setAgreementAccepted(bool accepted)
{
    if (setValue("agreementAccepted", accepted)) {
        save(); // Immediately persist agreement acceptance
    }
}

// ============================================================================
// Generic Access
// ============================================================================

QJsonObject ConfigStore::toJson() const
{
    return config_;
}

QStringList ConfigStore::fromJson(const QJsonObject& options)
{
    QStringList changed;

    for (auto it = options.begin(); it != options.end(); ++it) {
        if (setValue(it.key(), it.value().toVariant())) {
            changed.append(it.key());
        }
    }

    if (!changed.isEmpty()) {
        validateAndClamp();
        emit configChanged(changed);
    }

    return changed;
}

QVariant ConfigStore::value(const QString& key, const QVariant& defaultValue) const
{
    if (config_.contains(key)) {
        return config_[key].toVariant();
    }
    return defaultValue;
}

bool ConfigStore::setValue(const QString& key, const QVariant& value)
{
    const KeyMeta* meta = metaForKey(key);

    // Clamp per metadata before comparing, so out-of-range writes dedupe against
    // the already-clamped stored value on every path (typed setter, fromJson).
    QVariant effective = value;
    if (meta && meta->hasClamp) {
        effective = QVariant(static_cast<qlonglong>(qBound(meta->clampMin, value.toLongLong(), meta->clampMax)));
    }

    const QJsonValue newVal = QJsonValue::fromVariant(effective);
    if (readJsonValue(key) == newVal) {
        return false;
    }

    writeJsonValue(key, newVal);
    dirty_ = true;

    if (meta) {
        if (meta->emitSignal) {
            meta->emitSignal(this, effective);
        }
        if (meta->emitGroup) {
            meta->emitGroup(this);
        }
    }

    return true;
}

} // namespace app
} // namespace rats
