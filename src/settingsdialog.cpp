#include "settingsdialog.h"
#include "api/configmanager.h"
#include "api/ratsapi.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QGridLayout>
#include <QStyle>
#include <QSettings>

SettingsDialog::SettingsDialog(ConfigManager* config, RatsAPI* api,
                               const QString& dataDirectory, QWidget *parent)
    : QDialog(parent)
    , config_(config)
    , api_(api)
    , dataDirectory_(dataDirectory)
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(550, 600);
    resize(550, 700);
    
    setupUi();
    loadSettings();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setupUi()
{
    QVBoxLayout *dialogLayout = new QVBoxLayout(this);
    dialogLayout->setSpacing(12);
    dialogLayout->setContentsMargins(16, 16, 16, 16);

    // Title
    QLabel *titleLabel = new QLabel(tr("Rats Search Settings"));
    titleLabel->setObjectName("headerLabel");
    dialogLayout->addWidget(titleLabel);

    // Scroll area
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setObjectName("settingsScrollArea");

    QWidget *scrollContent = new QWidget();
    scrollContent->setObjectName("settingsScrollContent");
    QVBoxLayout *mainLayout = new QVBoxLayout(scrollContent);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // =========================================================================
    // General Settings
    // =========================================================================
    QGroupBox *generalGroup = new QGroupBox(tr("General"));
    QFormLayout *generalLayout = new QFormLayout(generalGroup);
    generalLayout->setSpacing(12);

    languageCombo_ = new QComboBox();
    languageCombo_->addItem("English", "en");
    languageCombo_->addItem("Русский", "ru");
    languageCombo_->addItem("Deutsch", "de");
    languageCombo_->addItem("Español", "es");
    languageCombo_->addItem("Français", "fr");
    generalLayout->addRow(tr("Language:"), languageCombo_);

    minimizeToTrayCheck_ = new QCheckBox(tr("Hide to tray on minimize"));
    generalLayout->addRow(minimizeToTrayCheck_);

    closeToTrayCheck_ = new QCheckBox(tr("Hide to tray on close"));
    generalLayout->addRow(closeToTrayCheck_);

    startMinimizedCheck_ = new QCheckBox(tr("Start minimized"));
    generalLayout->addRow(startMinimizedCheck_);

    darkModeCheck_ = new QCheckBox(tr("Dark mode"));
    generalLayout->addRow(darkModeCheck_);

    checkUpdatesCheck_ = new QCheckBox(tr("Check for updates on startup"));
    generalLayout->addRow(checkUpdatesCheck_);

    mainLayout->addWidget(generalGroup);

    // =========================================================================
    // Network Settings
    // =========================================================================
    QGroupBox *networkGroup = new QGroupBox(tr("Network"));
    QFormLayout *networkLayout = new QFormLayout(networkGroup);
    networkLayout->setSpacing(12);

    p2pPortSpin_ = new QSpinBox();
    p2pPortSpin_->setRange(1024, 65535);
    networkLayout->addRow(tr("P2P Port:"), p2pPortSpin_);

    dhtPortSpin_ = new QSpinBox();
    dhtPortSpin_->setRange(1024, 65535);
    networkLayout->addRow(tr("DHT Port:"), dhtPortSpin_);

    httpPortSpin_ = new QSpinBox();
    httpPortSpin_->setRange(1024, 65535);
    networkLayout->addRow(tr("HTTP API Port:"), httpPortSpin_);

    restApiCheck_ = new QCheckBox(tr("Enable REST API server"));
    networkLayout->addRow(restApiCheck_);

    mainLayout->addWidget(networkGroup);

    // =========================================================================
    // Indexer Settings
    // =========================================================================
    QGroupBox *indexerGroup = new QGroupBox(tr("Indexer"));
    QFormLayout *indexerLayout = new QFormLayout(indexerGroup);

    indexerCheck_ = new QCheckBox(tr("Enable DHT indexer"));
    indexerLayout->addRow(indexerCheck_);

    trackersCheck_ = new QCheckBox(tr("Enable tracker checking"));
    indexerLayout->addRow(trackersCheck_);

    mainLayout->addWidget(indexerGroup);

    // =========================================================================
    // P2P Network Settings
    // =========================================================================
    QGroupBox *p2pGroup = new QGroupBox(tr("P2P Network"));
    QFormLayout *p2pLayout = new QFormLayout(p2pGroup);
    p2pLayout->setSpacing(12);

    p2pBootstrapCheck_ = new QCheckBox(tr("Enable bootstrap nodes"));
    p2pLayout->addRow(p2pBootstrapCheck_);

    p2pConnectionsSpin_ = new QSpinBox();
    p2pConnectionsSpin_->setRange(5, 100);
    p2pConnectionsSpin_->setToolTip(tr("Maximum number of P2P connections"));
    p2pLayout->addRow(tr("Max connections:"), p2pConnectionsSpin_);

    p2pReplicationCheck_ = new QCheckBox(tr("Enable P2P replication (client)"));
    p2pReplicationCheck_->setToolTip(tr("Replicate database from other peers"));
    p2pLayout->addRow(p2pReplicationCheck_);

    p2pReplicationServerCheck_ = new QCheckBox(tr("Enable P2P replication server"));
    p2pReplicationServerCheck_->setToolTip(tr("Serve database to other peers"));
    p2pLayout->addRow(p2pReplicationServerCheck_);

    mainLayout->addWidget(p2pGroup);

    // =========================================================================
    // Performance Settings
    // =========================================================================
    QGroupBox *perfGroup = new QGroupBox(tr("Performance"));
    QFormLayout *perfLayout = new QFormLayout(perfGroup);
    perfLayout->setSpacing(12);

    walkIntervalSpin_ = new QSpinBox();
    walkIntervalSpin_->setRange(1, 150);
    walkIntervalSpin_->setToolTip(tr("Interval between DHT walks (lower = faster, more CPU)"));
    perfLayout->addRow(tr("Spider walk interval:"), walkIntervalSpin_);

    nodesUsageSpin_ = new QSpinBox();
    nodesUsageSpin_->setRange(0, 1000);
    nodesUsageSpin_->setToolTip(tr("Number of DHT nodes to use (0 = auto)"));
    perfLayout->addRow(tr("DHT nodes usage:"), nodesUsageSpin_);

    packagesLimitSpin_ = new QSpinBox();
    packagesLimitSpin_->setRange(0, 5000);
    packagesLimitSpin_->setToolTip(tr("Maximum network packages per second (0 = unlimited)"));
    perfLayout->addRow(tr("Package limit:"), packagesLimitSpin_);

    mainLayout->addWidget(perfGroup);

    // =========================================================================
    // Content Filters
    // =========================================================================
    QGroupBox *filtersGroup = new QGroupBox(tr("Content Filters"));
    QVBoxLayout *filtersLayout = new QVBoxLayout(filtersGroup);
    filtersLayout->setSpacing(12);

    // Max files per torrent
    QHBoxLayout *maxFilesRow = new QHBoxLayout();
    QLabel *maxFilesLabel = new QLabel(tr("Max files per torrent:"));
    maxFilesLabel->setToolTip(tr("Maximum number of files in a torrent (0 = disabled)"));
    
    maxFilesSlider_ = new QSlider(Qt::Horizontal);
    maxFilesSlider_->setRange(0, 50000);
    
    maxFilesSpin_ = new QSpinBox();
    maxFilesSpin_->setRange(0, 50000);
    maxFilesSpin_->setMinimumWidth(80);

    connect(maxFilesSlider_, &QSlider::valueChanged, maxFilesSpin_, &QSpinBox::setValue);
    connect(maxFilesSpin_, QOverload<int>::of(&QSpinBox::valueChanged), maxFilesSlider_, &QSlider::setValue);

    maxFilesRow->addWidget(maxFilesLabel);
    maxFilesRow->addWidget(maxFilesSlider_, 1);
    maxFilesRow->addWidget(maxFilesSpin_);
    filtersLayout->addLayout(maxFilesRow);

    QLabel *maxFilesHint = new QLabel(tr("* 0 = Disabled (no limit)"));
    maxFilesHint->setObjectName("hintLabel");
    filtersLayout->addWidget(maxFilesHint);

    // Regex filter
    QHBoxLayout *regexRow = new QHBoxLayout();
    QLabel *regexLabel = new QLabel(tr("Name filter (regex):"));
    regexEdit_ = new QLineEdit();
    regexEdit_->setPlaceholderText(tr("Regular expression pattern..."));

    QComboBox *regexExamples = new QComboBox();
    regexExamples->addItem(tr("Examples..."), "");
    regexExamples->addItem(tr("Russian + English only"), QString::fromUtf8(R"(^[А-Яа-я0-9A-Za-z.!@?#"$%&:;() *\+,\/;\-=[\\\]\^_{|}<>\u0400-\u04FF]+$)"));
    regexExamples->addItem(tr("English only"), R"(^[0-9A-Za-z.!@?#"$%&:;() *\+,\/;\-=[\\\]\^_{|}<>]+$)");
    regexExamples->addItem(tr("Ignore 'badword'"), R"(^((?!badword).)*$)");

    connect(regexExamples, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, regexExamples](int index) {
        QString example = regexExamples->itemData(index).toString();
        if (!example.isEmpty()) {
            regexEdit_->setText(example);
        }
    });

    regexRow->addWidget(regexLabel);
    regexRow->addWidget(regexEdit_, 1);
    regexRow->addWidget(regexExamples);
    filtersLayout->addLayout(regexRow);

    regexNegativeCheck_ = new QCheckBox(tr("Negative regex filter (reject matches)"));
    regexNegativeCheck_->setToolTip(tr("When enabled, torrents matching the regex will be rejected"));
    filtersLayout->addWidget(regexNegativeCheck_);

    QLabel *regexHint = new QLabel(tr("* Empty string = Disabled"));
    regexHint->setObjectName("hintLabel");
    filtersLayout->addWidget(regexHint);

    adultFilterCheck_ = new QCheckBox(tr("Adult content filter (ignore XXX content)"));
    adultFilterCheck_->setToolTip(tr("When enabled, adult content will be filtered out"));
    filtersLayout->addWidget(adultFilterCheck_);

    // Size filter
    QGroupBox *sizeFilterBox = new QGroupBox(tr("Size Filter"));
    QFormLayout *sizeLayout = new QFormLayout(sizeFilterBox);

    sizeMinSpin_ = new QSpinBox();
    sizeMinSpin_->setRange(0, 999999);
    sizeMinSpin_->setSuffix(" MB");
    sizeMinSpin_->setToolTip(tr("Minimum torrent size (0 = no minimum)"));
    sizeLayout->addRow(tr("Minimum size:"), sizeMinSpin_);

    sizeMaxSpin_ = new QSpinBox();
    sizeMaxSpin_->setRange(0, 999999);
    sizeMaxSpin_->setSuffix(" MB");
    sizeMaxSpin_->setToolTip(tr("Maximum torrent size (0 = no maximum)"));
    sizeLayout->addRow(tr("Maximum size:"), sizeMaxSpin_);

    filtersLayout->addWidget(sizeFilterBox);

    // Content type filter
    QGroupBox *contentTypeBox = new QGroupBox(tr("Content Type Filter"));
    QVBoxLayout *contentTypeLayout = new QVBoxLayout(contentTypeBox);

    QLabel *contentTypeHint = new QLabel(tr("Uncheck to disable specific content types:"));
    contentTypeHint->setObjectName("hintLabel");
    contentTypeLayout->addWidget(contentTypeHint);

    QGridLayout *typeGrid = new QGridLayout();
    videoCheck_ = new QCheckBox(tr("Video"));
    audioCheck_ = new QCheckBox(tr("Audio/Music"));
    picturesCheck_ = new QCheckBox(tr("Pictures/Images"));
    booksCheck_ = new QCheckBox(tr("Books"));
    appsCheck_ = new QCheckBox(tr("Apps/Games"));
    archivesCheck_ = new QCheckBox(tr("Archives"));
    discsCheck_ = new QCheckBox(tr("Discs/ISO"));

    typeGrid->addWidget(videoCheck_, 0, 0);
    typeGrid->addWidget(audioCheck_, 0, 1);
    typeGrid->addWidget(picturesCheck_, 1, 0);
    typeGrid->addWidget(booksCheck_, 1, 1);
    typeGrid->addWidget(appsCheck_, 2, 0);
    typeGrid->addWidget(archivesCheck_, 2, 1);
    typeGrid->addWidget(discsCheck_, 3, 0);

    contentTypeLayout->addLayout(typeGrid);
    filtersLayout->addWidget(contentTypeBox);

    // Database cleanup
    QGroupBox *cleanupBox = new QGroupBox(tr("Database Cleanup"));
    QVBoxLayout *cleanupLayout = new QVBoxLayout(cleanupBox);

    QLabel *cleanupDesc = new QLabel(tr("Check and remove torrents that don't match the current filters:"));
    cleanupDesc->setWordWrap(true);
    cleanupLayout->addWidget(cleanupDesc);

    cleanupProgress_ = new QLabel("");
    cleanupProgress_->setObjectName("cleanupProgressLabel");
    cleanupLayout->addWidget(cleanupProgress_);

    cleanupProgressBar_ = new QProgressBar();
    cleanupProgressBar_->setVisible(false);
    cleanupProgressBar_->setRange(0, 100);
    cleanupLayout->addWidget(cleanupProgressBar_);

    QHBoxLayout *cleanupBtnRow = new QHBoxLayout();
    checkTorrentsBtn_ = new QPushButton(tr("Check Torrents"));
    checkTorrentsBtn_->setToolTip(tr("Count how many torrents would be removed (dry run)"));
    checkTorrentsBtn_->setObjectName("secondaryButton");
    connect(checkTorrentsBtn_, &QPushButton::clicked, this, &SettingsDialog::onCheckTorrentsClicked);

    cleanTorrentsBtn_ = new QPushButton(tr("Clean Torrents"));
    cleanTorrentsBtn_->setToolTip(tr("Remove torrents that don't match the current filters"));
    cleanTorrentsBtn_->setObjectName("dangerButton");
    connect(cleanTorrentsBtn_, &QPushButton::clicked, this, &SettingsDialog::onCleanTorrentsClicked);

    cleanupBtnRow->addWidget(checkTorrentsBtn_);
    cleanupBtnRow->addWidget(cleanTorrentsBtn_);
    cleanupBtnRow->addStretch();
    cleanupLayout->addLayout(cleanupBtnRow);

    // Connect cleanup progress signal from API
    if (api_) {
        connect(api_, &RatsAPI::cleanupProgress, this, [this](int current, int total, const QString& status) {
            if (total > 0) {
                cleanupProgressBar_->setMaximum(total);
                cleanupProgressBar_->setValue(current);
            }
            if (status == "check") {
                cleanupProgress_->setText(tr("Checking: %1 found...").arg(current));
            } else {
                cleanupProgress_->setText(tr("Cleaning: %1/%2...").arg(current).arg(total));
            }
        });
    }

    filtersLayout->addWidget(cleanupBox);
    mainLayout->addWidget(filtersGroup);

    // =========================================================================
    // Downloads Settings
    // =========================================================================
    QGroupBox *downloadGroup = new QGroupBox(tr("Downloads"));
    QFormLayout *downloadLayout = new QFormLayout(downloadGroup);

    QHBoxLayout *downloadPathLayout = new QHBoxLayout();
    downloadPathEdit_ = new QLineEdit();
    QPushButton *browseDownloadBtn = new QPushButton(tr("Browse..."));
    browseDownloadBtn->setObjectName("secondaryButton");
    connect(browseDownloadBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Download Directory"), 
            downloadPathEdit_->text());
        if (!dir.isEmpty()) {
            downloadPathEdit_->setText(dir);
        }
    });
    downloadPathLayout->addWidget(downloadPathEdit_);
    downloadPathLayout->addWidget(browseDownloadBtn);
    downloadLayout->addRow(tr("Default Download Directory:"), downloadPathLayout);
    
    QLabel *downloadPathHint = new QLabel(tr("* Default location for downloaded torrents"));
    downloadPathHint->setObjectName("hintLabel");
    downloadLayout->addRow(downloadPathHint);

    mainLayout->addWidget(downloadGroup);

    // =========================================================================
    // Database Settings
    // =========================================================================
    QGroupBox *dbGroup = new QGroupBox(tr("Database"));
    QFormLayout *dbLayout = new QFormLayout(dbGroup);

    QHBoxLayout *pathLayout = new QHBoxLayout();
    dataPathEdit_ = new QLineEdit();
    QPushButton *browseBtn = new QPushButton(tr("Browse..."));
    browseBtn->setObjectName("secondaryButton");
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseDataPath);
    pathLayout->addWidget(dataPathEdit_);
    pathLayout->addWidget(browseBtn);
    dbLayout->addRow(tr("Data Directory:"), pathLayout);
    
    QLabel *dataPathHint = new QLabel(tr("* Changing data directory requires restart"));
    dataPathHint->setObjectName("hintLabel");
    dbLayout->addRow(dataPathHint);

    mainLayout->addWidget(dbGroup);
    mainLayout->addStretch();

    scrollContent->setLayout(mainLayout);
    scrollArea->setWidget(scrollContent);
    dialogLayout->addWidget(scrollArea, 1);

    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    dialogLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::loadSettings()
{
    if (!config_) return;

    // General
    QString currentLang = config_->language();
    for (int i = 0; i < languageCombo_->count(); ++i) {
        if (languageCombo_->itemData(i).toString() == currentLang) {
            languageCombo_->setCurrentIndex(i);
            break;
        }
    }
    minimizeToTrayCheck_->setChecked(config_->trayOnMinimize());
    closeToTrayCheck_->setChecked(config_->trayOnClose());
    startMinimizedCheck_->setChecked(config_->startMinimized());
    darkModeCheck_->setChecked(config_->darkMode());
    checkUpdatesCheck_->setChecked(config_->checkUpdatesOnStartup());

    // Network
    p2pPortSpin_->setValue(config_->p2pPort());
    dhtPortSpin_->setValue(config_->dhtPort());
    httpPortSpin_->setValue(config_->httpPort());
    restApiCheck_->setChecked(config_->restApiEnabled());

    // Indexer
    indexerCheck_->setChecked(config_->indexerEnabled());
    trackersCheck_->setChecked(config_->trackersEnabled());

    // P2P
    p2pBootstrapCheck_->setChecked(config_->p2pBootstrap());
    p2pConnectionsSpin_->setValue(config_->p2pConnections());
    p2pReplicationCheck_->setChecked(config_->p2pReplication());
    p2pReplicationServerCheck_->setChecked(config_->p2pReplicationServer());

    // Performance
    walkIntervalSpin_->setValue(config_->spiderWalkInterval());
    nodesUsageSpin_->setValue(config_->spiderNodesUsage());
    packagesLimitSpin_->setValue(config_->spiderPackagesLimit());

    // Filters
    maxFilesSpin_->setValue(config_->filtersMaxFiles());
    maxFilesSlider_->setValue(config_->filtersMaxFiles());
    regexEdit_->setText(config_->filtersNamingRegExp());
    regexNegativeCheck_->setChecked(config_->filtersNamingRegExpNegative());
    adultFilterCheck_->setChecked(config_->filtersAdultFilter());
    sizeMinSpin_->setValue(static_cast<int>(config_->filtersSizeMin() / (1024 * 1024)));
    sizeMaxSpin_->setValue(static_cast<int>(config_->filtersSizeMax() / (1024 * 1024)));

    // Content types
    QString currentContentTypes = config_->filtersContentType();
    QStringList enabledTypes = currentContentTypes.isEmpty() ?
        QStringList{"video", "audio", "pictures", "books", "application", "archive", "disc"} :
        currentContentTypes.split(",", Qt::SkipEmptyParts);

    videoCheck_->setChecked(enabledTypes.contains("video"));
    audioCheck_->setChecked(enabledTypes.contains("audio"));
    picturesCheck_->setChecked(enabledTypes.contains("pictures"));
    booksCheck_->setChecked(enabledTypes.contains("books"));
    appsCheck_->setChecked(enabledTypes.contains("application"));
    archivesCheck_->setChecked(enabledTypes.contains("archive"));
    discsCheck_->setChecked(enabledTypes.contains("disc"));
    
    // Downloads
    downloadPathEdit_->setText(config_->downloadPath());
    
    // Database - load from QSettings first (source of truth for data directory),
    // then fallback to config, then to current runtime directory
    QSettings settings("RatsSearch", "RatsSearch");
    QString savedDataDir = settings.value("dataDirectory").toString();
    if (savedDataDir.isEmpty()) {
        savedDataDir = config_->dataDirectory();
    }
    if (savedDataDir.isEmpty()) {
        savedDataDir = dataDirectory_;  // Use runtime directory if not saved
    }
    dataPathEdit_->setText(savedDataDir);
}

void SettingsDialog::saveSettings()
{
    if (!config_) return;

    // Track what changed for restart notification
    int oldP2pPort = config_->p2pPort();
    int oldDhtPort = config_->dhtPort();
    int oldHttpPort = config_->httpPort();
    bool oldRestApi = config_->restApiEnabled();
    QString oldDataDir = config_->dataDirectory();

    // Save General (language and dark mode are applied immediately via signals)
    config_->setLanguage(languageCombo_->currentData().toString());
    config_->setTrayOnMinimize(minimizeToTrayCheck_->isChecked());
    config_->setTrayOnClose(closeToTrayCheck_->isChecked());
    config_->setStartMinimized(startMinimizedCheck_->isChecked());
    config_->setDarkMode(darkModeCheck_->isChecked());
    config_->setCheckUpdatesOnStartup(checkUpdatesCheck_->isChecked());

    // Save Network
    config_->setP2pPort(p2pPortSpin_->value());
    config_->setDhtPort(dhtPortSpin_->value());
    config_->setHttpPort(httpPortSpin_->value());
    config_->setRestApiEnabled(restApiCheck_->isChecked());

    // Save Indexer
    config_->setIndexerEnabled(indexerCheck_->isChecked());
    config_->setTrackersEnabled(trackersCheck_->isChecked());

    // Save P2P
    config_->setP2pBootstrap(p2pBootstrapCheck_->isChecked());
    config_->setP2pConnections(p2pConnectionsSpin_->value());
    config_->setP2pReplication(p2pReplicationCheck_->isChecked());
    config_->setP2pReplicationServer(p2pReplicationServerCheck_->isChecked());

    // Save Performance
    config_->setSpiderWalkInterval(walkIntervalSpin_->value());
    config_->setSpiderNodesUsage(nodesUsageSpin_->value());
    config_->setSpiderPackagesLimit(packagesLimitSpin_->value());

    // Save Filters
    config_->setFiltersMaxFiles(maxFilesSpin_->value());
    config_->setFiltersNamingRegExp(regexEdit_->text());
    config_->setFiltersNamingRegExpNegative(regexNegativeCheck_->isChecked());
    config_->setFiltersAdultFilter(adultFilterCheck_->isChecked());
    config_->setFiltersSizeMin(static_cast<qint64>(sizeMinSpin_->value()) * 1024 * 1024);
    config_->setFiltersSizeMax(static_cast<qint64>(sizeMaxSpin_->value()) * 1024 * 1024);

    // Build content type filter string
    QStringList contentTypes;
    if (videoCheck_->isChecked()) contentTypes << "video";
    if (audioCheck_->isChecked()) contentTypes << "audio";
    if (picturesCheck_->isChecked()) contentTypes << "pictures";
    if (booksCheck_->isChecked()) contentTypes << "books";
    if (appsCheck_->isChecked()) contentTypes << "application";
    if (archivesCheck_->isChecked()) contentTypes << "archive";
    if (discsCheck_->isChecked()) contentTypes << "disc";

    if (contentTypes.size() == 7) {
        config_->setFiltersContentType("");
    } else {
        config_->setFiltersContentType(contentTypes.join(","));
    }

    // Save Download Path
    QString newDownloadPath = downloadPathEdit_->text();
    if (!newDownloadPath.isEmpty()) {
        config_->setDownloadPath(newDownloadPath);
    }

    // Save Data Directory
    QString newDataDir = dataPathEdit_->text();
    if (!newDataDir.isEmpty()) {
        config_->setDataDirectory(newDataDir);
        
        // Also save to QSettings so main.cpp can read it at startup
        // This solves the chicken-and-egg problem: config is in dataDirectory,
        // but we need to know dataDirectory before loading config
        QSettings settings("RatsSearch", "RatsSearch");
        settings.setValue("dataDirectory", newDataDir);
    }

    // Check if restart needed (only for settings that can't be applied at runtime)
    // Network ports and data directory require restart
    needsRestart_ = (p2pPortSpin_->value() != oldP2pPort) ||
                    (dhtPortSpin_->value() != oldDhtPort) ||
                    (httpPortSpin_->value() != oldHttpPort) ||
                    (restApiCheck_->isChecked() != oldRestApi) ||
                    (newDataDir != oldDataDir && !newDataDir.isEmpty());

    // Save config to file
    config_->save();
}

void SettingsDialog::onCheckTorrentsClicked()
{
    if (!api_) return;

    cleanupProgress_->setText(tr("Checking torrents..."));
    cleanupProgress_->setObjectName("warningLabel");
    cleanupProgress_->style()->unpolish(cleanupProgress_);
    cleanupProgress_->style()->polish(cleanupProgress_);
    cleanupProgressBar_->setVisible(true);
    cleanupProgressBar_->setValue(0);
    checkTorrentsBtn_->setEnabled(false);
    cleanTorrentsBtn_->setEnabled(false);

    api_->removeTorrents(true, [this](const ApiResponse& response) {
        QMetaObject::invokeMethod(this, [this, response]() {
            cleanupProgressBar_->setVisible(false);
            checkTorrentsBtn_->setEnabled(true);
            cleanTorrentsBtn_->setEnabled(true);

            if (response.success) {
                QJsonObject data = response.data.toObject();
                int found = data["found"].toInt();
                int checked = data["checked"].toInt();

                if (found > 0) {
                    cleanupProgress_->setText(tr("Found %1 torrents to remove (checked %2)").arg(found).arg(checked));
                    cleanupProgress_->setObjectName("warningLabel");
                } else {
                    cleanupProgress_->setText(tr("All %1 torrents match filters").arg(checked));
                    cleanupProgress_->setObjectName("successLabel");
                }
            } else {
                cleanupProgress_->setText(tr("Error: %1").arg(response.error));
                cleanupProgress_->setObjectName("errorLabel");
            }
            cleanupProgress_->style()->unpolish(cleanupProgress_);
            cleanupProgress_->style()->polish(cleanupProgress_);
        });
    });
}

void SettingsDialog::onCleanTorrentsClicked()
{
    if (!api_) return;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Confirm Cleanup"),
        tr("This will permanently remove torrents that don't match the current filters.\n\nAre you sure?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    cleanupProgress_->setText(tr("Cleaning torrents..."));
    cleanupProgress_->setObjectName("errorLabel");
    cleanupProgress_->style()->unpolish(cleanupProgress_);
    cleanupProgress_->style()->polish(cleanupProgress_);
    cleanupProgressBar_->setVisible(true);
    cleanupProgressBar_->setValue(0);
    checkTorrentsBtn_->setEnabled(false);
    cleanTorrentsBtn_->setEnabled(false);

    api_->removeTorrents(false, [this](const ApiResponse& response) {
        QMetaObject::invokeMethod(this, [this, response]() {
            cleanupProgressBar_->setVisible(false);
            checkTorrentsBtn_->setEnabled(true);
            cleanTorrentsBtn_->setEnabled(true);

            if (response.success) {
                QJsonObject data = response.data.toObject();
                int removed = data["removed"].toInt();
                int checked = data["checked"].toInt();

                cleanupProgress_->setText(tr("Removed %1 torrents (checked %2)").arg(removed).arg(checked));
                cleanupProgress_->setObjectName("successLabel");
            } else {
                cleanupProgress_->setText(tr("Error: %1").arg(response.error));
                cleanupProgress_->setObjectName("errorLabel");
            }
            cleanupProgress_->style()->unpolish(cleanupProgress_);
            cleanupProgress_->style()->polish(cleanupProgress_);
        });
    });
}

void SettingsDialog::onBrowseDataPath()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Data Directory"), dataDirectory_);
    if (!dir.isEmpty()) {
        dataPathEdit_->setText(dir);
    }
}

void SettingsDialog::onAccepted()
{
    saveSettings();
    accept();
}
