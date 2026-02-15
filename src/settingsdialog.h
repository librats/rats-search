#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QLineEdit>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

class ConfigManager;
class RatsAPI;

/**
 * @brief SettingsDialog - Application settings dialog
 * 
 * Provides UI for configuring:
 * - General settings (language, tray behavior, theme)
 * - Network settings (ports, REST API)
 * - Indexer settings
 * - P2P network settings
 * - Performance settings
 * - Content filters
 * - Database cleanup
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(ConfigManager* config, RatsAPI* api, 
                           const QString& dataDirectory, QWidget *parent = nullptr);
    ~SettingsDialog();

    /**
     * @brief Check if settings requiring restart were changed
     * Returns true only for network ports and data directory changes
     */
    bool needsRestart() const { return needsRestart_; }

private slots:
    void onCheckTorrentsClicked();
    void onCleanTorrentsClicked();
    void onBrowseDataPath();
    void onAccepted();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    ConfigManager* config_;
    RatsAPI* api_;
    QString dataDirectory_;
    
    bool needsRestart_ = false;

    // General settings
    QComboBox* languageCombo_;
    QCheckBox* minimizeToTrayCheck_;
    QCheckBox* closeToTrayCheck_;
    QCheckBox* startMinimizedCheck_;
    QCheckBox* darkModeCheck_;
    QCheckBox* checkUpdatesCheck_;

    // Network settings
    QSpinBox* p2pPortSpin_;
    QSpinBox* dhtPortSpin_;
    QSpinBox* httpPortSpin_;
    QCheckBox* restApiCheck_;

    // Indexer settings
    QCheckBox* indexerCheck_;
    QCheckBox* trackersCheck_;

    // P2P settings
    QSpinBox* p2pConnectionsSpin_;
    QCheckBox* p2pReplicationCheck_;
    QCheckBox* p2pReplicationServerCheck_;

    // Performance settings
    QSpinBox* walkIntervalSpin_;
    QSpinBox* nodesUsageSpin_;
    QSpinBox* packagesLimitSpin_;

    // Filter settings
    QSpinBox* maxFilesSpin_;
    QSlider* maxFilesSlider_;
    QLineEdit* regexEdit_;
    QCheckBox* regexNegativeCheck_;
    QCheckBox* adultFilterCheck_;
    QSpinBox* sizeMinSpin_;
    QSpinBox* sizeMaxSpin_;
    
    // Content type filters
    QCheckBox* videoCheck_;
    QCheckBox* audioCheck_;
    QCheckBox* picturesCheck_;
    QCheckBox* booksCheck_;
    QCheckBox* appsCheck_;
    QCheckBox* archivesCheck_;
    QCheckBox* discsCheck_;

    // Cleanup UI
    QLabel* cleanupProgress_;
    QProgressBar* cleanupProgressBar_;
    QPushButton* checkTorrentsBtn_;
    QPushButton* cleanTorrentsBtn_;

    // Database
    QLineEdit* dataPathEdit_;
    
    // Downloads
    QLineEdit* downloadPathEdit_;
};

#endif // SETTINGSDIALOG_H
