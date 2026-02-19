#include "configmanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

ConfigManager::ConfigManager(const QString& configPath, QObject *parent)
    : QObject(parent)
    , configPath_(configPath)
{
    setDefaults();
    
    if (configPath_.isEmpty()) {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDir);
        configPath_ = dataDir + "/rats.json";
    }
}

ConfigManager::~ConfigManager()
{
    if (dirty_) {
        save();
    }
}

void ConfigManager::setDefaults()
{
    config_ = QJsonObject{
        // Network
        {"httpPort", 8095},
        {"p2pPort", 4444},
        {"dhtPort", 6881},
        {"udpTrackersTimeout", 180000},
        
        // P2P
        {"p2p", true},
        {"p2pConnections", 10},
        {"p2pReplication", true},
        {"p2pReplicationServer", true},
        
        // Indexer
        {"indexer", true},
        {"trackers", true},
        {"restApi", false},
        {"upnp", true},
        
        // Spider
        {"spider", QJsonObject{
            {"walkInterval", 100},
            {"nodesUsage", 100},
            {"packagesLimit", 500}
        }},
        
        // Filters
        {"filters", QJsonObject{
            {"maxFiles", 0},
            {"namingRegExp", ""},
            {"namingRegExpNegative", false},
            {"adultFilter", false},
            {"sizeMin", 0},
            {"sizeMax", 0},
            {"contentType", ""}
        }},
        
        // Cleanup
        {"cleanup", true},
        {"cleanupDiscLimit", 7LL * 1024 * 1024 * 1024},
        {"spaceQuota", false},
        {"spaceDiskLimit", 7LL * 1024 * 1024 * 1024},
        {"recheckFilesOnAdding", true},
        
        // Downloads
        {"downloadPath", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)},
        
        // UI
        {"language", "en"},
        {"darkMode", false},
        {"trayOnClose", false},
        {"trayOnMinimize", true},
        {"startMinimized", false},
        {"autoStart", false},
        {"checkUpdatesOnStartup", true},
        {"dataDirectory", QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)},
        
        // Legal
        {"agreementAccepted", false}
    };
}

bool ConfigManager::load()
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

bool ConfigManager::save()
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

void ConfigManager::reset()
{
    setDefaults();
    dirty_ = true;
    emit configChanged(config_.keys());
}

void ConfigManager::validateAndClamp()
{
    // Clamp p2pConnections
    int p2pConn = config_["p2pConnections"].toInt();
    if (p2pConn < 10) config_["p2pConnections"] = 10;
    if (p2pConn > 1000) config_["p2pConnections"] = 1000;
    
    // P2P replication dependency
    if (config_["p2pReplication"].toBool() && !config_["p2pReplicationServer"].toBool()) {
        config_["p2pReplicationServer"] = true;
    }
    if (!config_["p2pReplicationServer"].toBool() && config_["p2pReplication"].toBool()) {
        config_["p2pReplication"] = false;
    }
}

// ============================================================================
// Network Settings
// ============================================================================

int ConfigManager::httpPort() const { return config_["httpPort"].toInt(8095); }
void ConfigManager::setHttpPort(int port) { 
    if (setValue("httpPort", port)) emit httpPortChanged(port);
}

int ConfigManager::p2pPort() const { return config_["p2pPort"].toInt(4444); }
void ConfigManager::setP2pPort(int port) { 
    if (setValue("p2pPort", port)) emit p2pPortChanged(port);
}

int ConfigManager::dhtPort() const { return config_["dhtPort"].toInt(6881); }
void ConfigManager::setDhtPort(int port) { 
    if (setValue("dhtPort", port)) emit dhtPortChanged(port);
}

int ConfigManager::udpTrackersTimeout() const { return config_["udpTrackersTimeout"].toInt(180000); }
void ConfigManager::setUdpTrackersTimeout(int timeout) { setValue("udpTrackersTimeout", timeout); }

// ============================================================================
// P2P Settings
// ============================================================================

bool ConfigManager::p2pEnabled() const { return config_["p2p"].toBool(true); }
void ConfigManager::setP2pEnabled(bool enabled) { 
    if (setValue("p2p", enabled)) emit p2pEnabledChanged(enabled);
}

int ConfigManager::p2pConnections() const { return config_["p2pConnections"].toInt(10); }
void ConfigManager::setP2pConnections(int connections) {
    connections = qBound(10, connections, 1000);
    if (setValue("p2pConnections", connections)) emit p2pConnectionsChanged(connections);
}

bool ConfigManager::p2pReplication() const { return config_["p2pReplication"].toBool(true); }
void ConfigManager::setP2pReplication(bool enabled) {
    if (enabled) setP2pReplicationServer(true);  // Dependency
    if (setValue("p2pReplication", enabled)) emit p2pReplicationChanged(enabled);
}

bool ConfigManager::p2pReplicationServer() const { return config_["p2pReplicationServer"].toBool(true); }
void ConfigManager::setP2pReplicationServer(bool enabled) {
    if (!enabled) config_["p2pReplication"] = false;  // Dependency
    setValue("p2pReplicationServer", enabled);
}

// ============================================================================
// Indexer Settings
// ============================================================================

bool ConfigManager::indexerEnabled() const { return config_["indexer"].toBool(true); }
void ConfigManager::setIndexerEnabled(bool enabled) { 
    if (setValue("indexer", enabled)) emit indexerEnabledChanged(enabled);
}

bool ConfigManager::trackersEnabled() const { return config_["trackers"].toBool(true); }
void ConfigManager::setTrackersEnabled(bool enabled) { 
    if (setValue("trackers", enabled)) emit trackersEnabledChanged(enabled);
}

bool ConfigManager::restApiEnabled() const { return config_["restApi"].toBool(false); }
void ConfigManager::setRestApiEnabled(bool enabled) { setValue("restApi", enabled); }

bool ConfigManager::upnpEnabled() const { return config_["upnp"].toBool(true); }
void ConfigManager::setUpnpEnabled(bool enabled) { setValue("upnp", enabled); }

// ============================================================================
// Spider Settings
// ============================================================================

int ConfigManager::spiderWalkInterval() const { 
    return config_["spider"].toObject()["walkInterval"].toInt(5); 
}
void ConfigManager::setSpiderWalkInterval(int interval) {
    QJsonObject spider = config_["spider"].toObject();
    spider["walkInterval"] = interval;
    config_["spider"] = spider;
    dirty_ = true;
}

int ConfigManager::spiderNodesUsage() const { 
    return config_["spider"].toObject()["nodesUsage"].toInt(100); 
}
void ConfigManager::setSpiderNodesUsage(int usage) {
    QJsonObject spider = config_["spider"].toObject();
    spider["nodesUsage"] = usage;
    config_["spider"] = spider;
    dirty_ = true;
}

int ConfigManager::spiderPackagesLimit() const { 
    return config_["spider"].toObject()["packagesLimit"].toInt(500); 
}
void ConfigManager::setSpiderPackagesLimit(int limit) {
    QJsonObject spider = config_["spider"].toObject();
    spider["packagesLimit"] = limit;
    config_["spider"] = spider;
    dirty_ = true;
}

// ============================================================================
// Filter Settings
// ============================================================================

int ConfigManager::filtersMaxFiles() const { 
    return config_["filters"].toObject()["maxFiles"].toInt(0); 
}
void ConfigManager::setFiltersMaxFiles(int max) {
    int oldValue = filtersMaxFiles();
    if (oldValue == max) return;
    
    QJsonObject filters = config_["filters"].toObject();
    filters["maxFiles"] = max;
    config_["filters"] = filters;
    dirty_ = true;
    emit filtersMaxFilesChanged(max);
    emit filtersChanged();
}

QString ConfigManager::filtersNamingRegExp() const { 
    return config_["filters"].toObject()["namingRegExp"].toString(); 
}
void ConfigManager::setFiltersNamingRegExp(const QString& regexp) {
    QString oldValue = filtersNamingRegExp();
    if (oldValue == regexp) return;
    
    QJsonObject filters = config_["filters"].toObject();
    filters["namingRegExp"] = regexp;
    config_["filters"] = filters;
    dirty_ = true;
    emit filtersNamingRegExpChanged(regexp);
    emit filtersChanged();
}

bool ConfigManager::filtersNamingRegExpNegative() const { 
    return config_["filters"].toObject()["namingRegExpNegative"].toBool(false); 
}
void ConfigManager::setFiltersNamingRegExpNegative(bool negative) {
    bool oldValue = filtersNamingRegExpNegative();
    if (oldValue == negative) return;
    
    QJsonObject filters = config_["filters"].toObject();
    filters["namingRegExpNegative"] = negative;
    config_["filters"] = filters;
    dirty_ = true;
    emit filtersNamingRegExpNegativeChanged(negative);
    emit filtersChanged();
}

bool ConfigManager::filtersAdultFilter() const { 
    return config_["filters"].toObject()["adultFilter"].toBool(false); 
}
void ConfigManager::setFiltersAdultFilter(bool enabled) {
    bool oldValue = filtersAdultFilter();
    if (oldValue == enabled) return;
    
    QJsonObject filters = config_["filters"].toObject();
    filters["adultFilter"] = enabled;
    config_["filters"] = filters;
    dirty_ = true;
    emit filtersAdultFilterChanged(enabled);
    emit filtersChanged();
}

qint64 ConfigManager::filtersSizeMin() const { 
    return config_["filters"].toObject()["sizeMin"].toVariant().toLongLong(); 
}
void ConfigManager::setFiltersSizeMin(qint64 min) {
    qint64 oldValue = filtersSizeMin();
    if (oldValue == min) return;
    
    QJsonObject filters = config_["filters"].toObject();
    filters["sizeMin"] = min;
    config_["filters"] = filters;
    dirty_ = true;
    emit filtersSizeMinChanged(min);
    emit filtersChanged();
}

qint64 ConfigManager::filtersSizeMax() const { 
    return config_["filters"].toObject()["sizeMax"].toVariant().toLongLong(); 
}
void ConfigManager::setFiltersSizeMax(qint64 max) {
    qint64 oldValue = filtersSizeMax();
    if (oldValue == max) return;
    
    QJsonObject filters = config_["filters"].toObject();
    filters["sizeMax"] = max;
    config_["filters"] = filters;
    dirty_ = true;
    emit filtersSizeMaxChanged(max);
    emit filtersChanged();
}

QString ConfigManager::filtersContentType() const { 
    return config_["filters"].toObject()["contentType"].toString(); 
}
void ConfigManager::setFiltersContentType(const QString& type) {
    QString oldValue = filtersContentType();
    if (oldValue == type) return;
    
    QJsonObject filters = config_["filters"].toObject();
    filters["contentType"] = type;
    config_["filters"] = filters;
    dirty_ = true;
    emit filtersContentTypeChanged(type);
    emit filtersChanged();
}

// ============================================================================
// Cleanup Settings
// ============================================================================

bool ConfigManager::cleanupEnabled() const { return config_["cleanup"].toBool(true); }
void ConfigManager::setCleanupEnabled(bool enabled) { setValue("cleanup", enabled); }

qint64 ConfigManager::cleanupDiscLimit() const { 
    return config_["cleanupDiscLimit"].toVariant().toLongLong(); 
}
void ConfigManager::setCleanupDiscLimit(qint64 limit) { setValue("cleanupDiscLimit", limit); }

bool ConfigManager::spaceQuotaEnabled() const { return config_["spaceQuota"].toBool(false); }
void ConfigManager::setSpaceQuotaEnabled(bool enabled) { setValue("spaceQuota", enabled); }

qint64 ConfigManager::spaceDiskLimit() const { 
    return config_["spaceDiskLimit"].toVariant().toLongLong(); 
}
void ConfigManager::setSpaceDiskLimit(qint64 limit) { setValue("spaceDiskLimit", limit); }

bool ConfigManager::recheckFilesOnAdding() const { return config_["recheckFilesOnAdding"].toBool(true); }
void ConfigManager::setRecheckFilesOnAdding(bool enabled) { setValue("recheckFilesOnAdding", enabled); }

// ============================================================================
// Client Settings
// ============================================================================

QString ConfigManager::downloadPath() const { 
    return config_["downloadPath"].toString(
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
    ); 
}
void ConfigManager::setDownloadPath(const QString& path) { setValue("downloadPath", path); }

// ============================================================================
// UI Settings
// ============================================================================

QString ConfigManager::language() const { return config_["language"].toString("en"); }
void ConfigManager::setLanguage(const QString& lang) { 
    if (setValue("language", lang)) emit languageChanged(lang);
}

bool ConfigManager::darkMode() const { return config_["darkMode"].toBool(false); }
void ConfigManager::setDarkMode(bool enabled) { 
    if (setValue("darkMode", enabled)) emit darkModeChanged(enabled);
}

bool ConfigManager::trayOnClose() const { return config_["trayOnClose"].toBool(false); }
void ConfigManager::setTrayOnClose(bool enabled) { 
    if (setValue("trayOnClose", enabled)) emit trayOnCloseChanged(enabled);
}

bool ConfigManager::trayOnMinimize() const { return config_["trayOnMinimize"].toBool(true); }
void ConfigManager::setTrayOnMinimize(bool enabled) { 
    if (setValue("trayOnMinimize", enabled)) emit trayOnMinimizeChanged(enabled);
}

bool ConfigManager::startMinimized() const { return config_["startMinimized"].toBool(false); }
void ConfigManager::setStartMinimized(bool enabled) { 
    if (setValue("startMinimized", enabled)) emit startMinimizedChanged(enabled);
}

bool ConfigManager::autoStart() const { return config_["autoStart"].toBool(false); }
void ConfigManager::setAutoStart(bool enabled) { 
    if (setValue("autoStart", enabled)) emit autoStartChanged(enabled);
}

bool ConfigManager::checkUpdatesOnStartup() const { return config_["checkUpdatesOnStartup"].toBool(true); }
void ConfigManager::setCheckUpdatesOnStartup(bool enabled) { 
    if (setValue("checkUpdatesOnStartup", enabled)) emit checkUpdatesOnStartupChanged(enabled);
}

QString ConfigManager::dataDirectory() const { 
    return config_["dataDirectory"].toString(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
    ); 
}
void ConfigManager::setDataDirectory(const QString& path) { 
    if (setValue("dataDirectory", path)) emit dataDirectoryChanged(path);
}

bool ConfigManager::agreementAccepted() const { return config_["agreementAccepted"].toBool(false); }
void ConfigManager::setAgreementAccepted(bool accepted) { 
    if (setValue("agreementAccepted", accepted)) {
        save();  // Immediately persist agreement acceptance
        emit agreementAcceptedChanged(accepted);
    }
}

// ============================================================================
// Generic Access
// ============================================================================

QJsonObject ConfigManager::toJson() const
{
    return config_;
}

QStringList ConfigManager::fromJson(const QJsonObject& options)
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

QVariant ConfigManager::value(const QString& key, const QVariant& defaultValue) const
{
    if (config_.contains(key)) {
        return config_[key].toVariant();
    }
    return defaultValue;
}

bool ConfigManager::setValue(const QString& key, const QVariant& value)
{
    QJsonValue newVal = QJsonValue::fromVariant(value);
    if (config_.value(key) == newVal) {
        return false;
    }
    
    config_[key] = newVal;
    dirty_ = true;
    return true;
}

