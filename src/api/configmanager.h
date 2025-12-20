#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QSettings>
#include <QVariant>

/**
 * @brief ConfigManager - Application configuration management
 * 
 * Manages application configuration with:
 * - Type-safe getters/setters
 * - Automatic persistence
 * - Change notifications
 * - Validation
 * 
 * Configuration is organized into sections:
 * - Network: ports, P2P settings
 * - Spider: indexing settings
 * - Filters: content filtering
 * - Client: download settings
 * - UI: interface preferences
 */
class ConfigManager : public QObject
{
    Q_OBJECT
    
    // Network settings
    Q_PROPERTY(int httpPort READ httpPort WRITE setHttpPort NOTIFY httpPortChanged)
    Q_PROPERTY(int spiderPort READ spiderPort WRITE setSpiderPort NOTIFY spiderPortChanged)
    Q_PROPERTY(int p2pPort READ p2pPort WRITE setP2pPort NOTIFY p2pPortChanged)
    Q_PROPERTY(int dhtPort READ dhtPort WRITE setDhtPort NOTIFY dhtPortChanged)
    
    // P2P settings
    Q_PROPERTY(bool p2pEnabled READ p2pEnabled WRITE setP2pEnabled NOTIFY p2pEnabledChanged)
    Q_PROPERTY(int p2pConnections READ p2pConnections WRITE setP2pConnections NOTIFY p2pConnectionsChanged)
    Q_PROPERTY(bool p2pReplication READ p2pReplication WRITE setP2pReplication NOTIFY p2pReplicationChanged)
    
    // Indexer settings
    Q_PROPERTY(bool indexerEnabled READ indexerEnabled WRITE setIndexerEnabled NOTIFY indexerEnabledChanged)
    Q_PROPERTY(bool trackersEnabled READ trackersEnabled WRITE setTrackersEnabled NOTIFY trackersEnabledChanged)
    
    // UI settings
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(bool trayOnClose READ trayOnClose WRITE setTrayOnClose NOTIFY trayOnCloseChanged)
    Q_PROPERTY(bool trayOnMinimize READ trayOnMinimize WRITE setTrayOnMinimize NOTIFY trayOnMinimizeChanged)
    Q_PROPERTY(bool startMinimized READ startMinimized WRITE setStartMinimized NOTIFY startMinimizedChanged)
    Q_PROPERTY(bool checkUpdatesOnStartup READ checkUpdatesOnStartup WRITE setCheckUpdatesOnStartup NOTIFY checkUpdatesOnStartupChanged)

public:
    explicit ConfigManager(const QString& configPath = QString(), QObject *parent = nullptr);
    ~ConfigManager();
    
    /**
     * @brief Load configuration from file
     */
    bool load();
    
    /**
     * @brief Save configuration to file
     */
    bool save();
    
    /**
     * @brief Reset to default values
     */
    void reset();
    
    // =========================================================================
    // Network Settings
    // =========================================================================
    
    int httpPort() const;
    void setHttpPort(int port);
    
    int spiderPort() const;
    void setSpiderPort(int port);
    
    int p2pPort() const;
    void setP2pPort(int port);
    
    int dhtPort() const;
    void setDhtPort(int port);
    
    int udpTrackersPort() const;
    void setUdpTrackersPort(int port);
    
    int udpTrackersTimeout() const;
    void setUdpTrackersTimeout(int timeout);
    
    // =========================================================================
    // P2P Settings
    // =========================================================================
    
    bool p2pEnabled() const;
    void setP2pEnabled(bool enabled);
    
    int p2pConnections() const;
    void setP2pConnections(int connections);
    
    bool p2pBootstrap() const;
    void setP2pBootstrap(bool enabled);
    
    bool p2pReplication() const;
    void setP2pReplication(bool enabled);
    
    bool p2pReplicationServer() const;
    void setP2pReplicationServer(bool enabled);
    
    // =========================================================================
    // Indexer Settings
    // =========================================================================
    
    bool indexerEnabled() const;
    void setIndexerEnabled(bool enabled);
    
    bool trackersEnabled() const;
    void setTrackersEnabled(bool enabled);
    
    bool restApiEnabled() const;
    void setRestApiEnabled(bool enabled);
    
    bool upnpEnabled() const;
    void setUpnpEnabled(bool enabled);
    
    // =========================================================================
    // Spider Settings
    // =========================================================================
    
    int spiderWalkInterval() const;
    void setSpiderWalkInterval(int interval);
    
    int spiderNodesUsage() const;
    void setSpiderNodesUsage(int usage);
    
    int spiderPackagesLimit() const;
    void setSpiderPackagesLimit(int limit);
    
    // =========================================================================
    // Filter Settings
    // =========================================================================
    
    int filtersMaxFiles() const;
    void setFiltersMaxFiles(int max);
    
    QString filtersNamingRegExp() const;
    void setFiltersNamingRegExp(const QString& regexp);
    
    bool filtersNamingRegExpNegative() const;
    void setFiltersNamingRegExpNegative(bool negative);
    
    bool filtersAdultFilter() const;
    void setFiltersAdultFilter(bool enabled);
    
    qint64 filtersSizeMin() const;
    void setFiltersSizeMin(qint64 min);
    
    qint64 filtersSizeMax() const;
    void setFiltersSizeMax(qint64 max);
    
    QString filtersContentType() const;
    void setFiltersContentType(const QString& type);
    
    // =========================================================================
    // Cleanup Settings
    // =========================================================================
    
    bool cleanupEnabled() const;
    void setCleanupEnabled(bool enabled);
    
    qint64 cleanupDiscLimit() const;
    void setCleanupDiscLimit(qint64 limit);
    
    bool spaceQuotaEnabled() const;
    void setSpaceQuotaEnabled(bool enabled);
    
    qint64 spaceDiskLimit() const;
    void setSpaceDiskLimit(qint64 limit);
    
    bool recheckFilesOnAdding() const;
    void setRecheckFilesOnAdding(bool enabled);
    
    // =========================================================================
    // Client Settings
    // =========================================================================
    
    QString downloadPath() const;
    void setDownloadPath(const QString& path);
    
    int downloaderMaxConnections() const;
    void setDownloaderMaxConnections(int max);
    
    int downloaderTimeout() const;
    void setDownloaderTimeout(int timeout);
    
    // =========================================================================
    // UI Settings
    // =========================================================================
    
    QString language() const;
    void setLanguage(const QString& lang);
    
    bool darkMode() const;
    void setDarkMode(bool enabled);
    
    bool trayOnClose() const;
    void setTrayOnClose(bool enabled);
    
    bool trayOnMinimize() const;
    void setTrayOnMinimize(bool enabled);
    
    bool startMinimized() const;
    void setStartMinimized(bool enabled);
    
    bool checkUpdatesOnStartup() const;
    void setCheckUpdatesOnStartup(bool enabled);
    
    // =========================================================================
    // Generic Access (for API)
    // =========================================================================
    
    /**
     * @brief Get all config as JSON
     */
    QJsonObject toJson() const;
    
    /**
     * @brief Update from JSON (bulk update)
     * @param options Key-value pairs to update
     * @return List of changed keys
     */
    QStringList fromJson(const QJsonObject& options);
    
    /**
     * @brief Get a specific value
     */
    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;
    
    /**
     * @brief Set a specific value
     * @return true if value was changed
     */
    bool setValue(const QString& key, const QVariant& value);
    
signals:
    void configChanged(const QStringList& changedKeys);
    
    // Individual property signals
    void httpPortChanged(int port);
    void spiderPortChanged(int port);
    void p2pPortChanged(int port);
    void dhtPortChanged(int port);
    void p2pEnabledChanged(bool enabled);
    void p2pConnectionsChanged(int connections);
    void p2pReplicationChanged(bool enabled);
    void indexerEnabledChanged(bool enabled);
    void trackersEnabledChanged(bool enabled);
    void languageChanged(const QString& lang);
    void darkModeChanged(bool enabled);
    void trayOnCloseChanged(bool enabled);
    void trayOnMinimizeChanged(bool enabled);
    void startMinimizedChanged(bool enabled);
    void checkUpdatesOnStartupChanged(bool enabled);

private:
    void setDefaults();
    void validateAndClamp();
    
    QString configPath_;
    QJsonObject config_;
    bool dirty_ = false;
};

#endif // CONFIGMANAGER_H

