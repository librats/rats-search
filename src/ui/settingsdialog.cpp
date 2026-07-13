#include "settingsdialog.h"
#include "app/application.h"
#include "app/config_store.h"
#include "app/translation_manager.h"
#include "autostartmanager.h"
#include "rest/api_router.h"
#include <QApplication>

#include <QApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScrollArea>
#include <QSettings>
#include <QStyle>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(rats::app::Application* app, QWidget* parent)
    : QDialog(parent), app_(app), dataDirectory_(app ? app->options().dataDirectory : QString())
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(600, 550);
    resize(650, 620);

    setupUi();
    loadSettings();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setupUi()
{
    QVBoxLayout* dialogLayout = new QVBoxLayout(this);
    dialogLayout->setSpacing(12);
    dialogLayout->setContentsMargins(16, 16, 16, 16);

    // Title
    QLabel* titleLabel = new QLabel(tr("Rats Search Settings"));
    titleLabel->setObjectName("headerLabel");
    dialogLayout->addWidget(titleLabel);

    // Tab widget
    tabWidget_ = new QTabWidget(this);
    tabWidget_->addTab(createGeneralTab(), tr("⚙️ General"));
    tabWidget_->addTab(createNetworkTab(), tr("🌐 Network"));
    tabWidget_->addTab(createIndexerTab(), tr("🕷️ Indexer"));
    tabWidget_->addTab(createFiltersTab(), tr("🔍 Filters"));
    tabWidget_->addTab(createStorageTab(), tr("💾 Storage"));
    dialogLayout->addWidget(tabWidget_, 1);

    // Buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    dialogLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// =============================================================================
// Helper: wrap any content widget in a QScrollArea
// =============================================================================
QWidget* SettingsDialog::wrapInScrollArea(QWidget* content)
{
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(content);

    QWidget* wrapper = new QWidget();
    QVBoxLayout* wrapperLayout = new QVBoxLayout(wrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->addWidget(scrollArea);

    // Prevent scroll-wheel from changing spinboxes/combos/sliders
    // when user just scrolls the page
    installScrollGuard(content);

    return wrapper;
}

// =============================================================================
// Helper: install event filter on all scrollable input widgets
// =============================================================================
void SettingsDialog::installScrollGuard(QWidget* container)
{
    // Find all QSpinBox, QComboBox, QSlider children
    const auto spinBoxes = container->findChildren<QSpinBox*>();
    for (auto* w : spinBoxes) {
        w->setFocusPolicy(Qt::StrongFocus);
        w->installEventFilter(this);
    }
    const auto comboBoxes = container->findChildren<QComboBox*>();
    for (auto* w : comboBoxes) {
        w->setFocusPolicy(Qt::StrongFocus);
        w->installEventFilter(this);
    }
    const auto sliders = container->findChildren<QSlider*>();
    for (auto* w : sliders) {
        w->setFocusPolicy(Qt::StrongFocus);
        w->installEventFilter(this);
    }
}

// =============================================================================
// Event filter: block wheel events on unfocused input widgets
// =============================================================================
bool SettingsDialog::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        auto* widget = qobject_cast<QWidget*>(obj);
        if (widget && !widget->hasFocus()) {
            // Forward the wheel event to the parent scroll area instead
            event->ignore();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

// =============================================================================
// Tab: General
// =============================================================================
QWidget* SettingsDialog::createGeneralTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* tabLayout = new QVBoxLayout(tab);
    tabLayout->setSpacing(16);
    tabLayout->setContentsMargins(12, 12, 12, 12);

    // --- Appearance ---
    QGroupBox* appearanceGroup = new QGroupBox(tr("Appearance"));
    QFormLayout* appearanceLayout = new QFormLayout(appearanceGroup);
    appearanceLayout->setSpacing(10);

    languageCombo_ = new QComboBox();
    for (const auto& lang : rats::app::TranslationManager::instance().availableLanguages())
        languageCombo_->addItem(lang.flagEmoji + QStringLiteral(" ") + lang.nativeName, lang.code);
    appearanceLayout->addRow(tr("Language:"), languageCombo_);

    darkModeCheck_ = new QCheckBox(tr("Dark mode"));
    appearanceLayout->addRow(darkModeCheck_);

    tabLayout->addWidget(appearanceGroup);

    // --- Startup & Tray ---
    QGroupBox* startupGroup = new QGroupBox(tr("Startup && System Tray"));
    QFormLayout* startupLayout = new QFormLayout(startupGroup);
    startupLayout->setSpacing(10);

    autoStartCheck_ = new QCheckBox(tr("Start with system (autostart)"));
    autoStartCheck_->setToolTip(tr("Automatically start Rats Search when you log in to your computer"));
    startupLayout->addRow(autoStartCheck_);

    startMinimizedCheck_ = new QCheckBox(tr("Start minimized"));
    startupLayout->addRow(startMinimizedCheck_);

    minimizeToTrayCheck_ = new QCheckBox(tr("Hide to tray on minimize"));
    startupLayout->addRow(minimizeToTrayCheck_);

    closeToTrayCheck_ = new QCheckBox(tr("Hide to tray on close"));
    startupLayout->addRow(closeToTrayCheck_);

    // When autostart is enabled, suggest starting minimized
    connect(autoStartCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && !startMinimizedCheck_->isChecked()) {
            startMinimizedCheck_->setChecked(true);
        }
    });

    tabLayout->addWidget(startupGroup);

    // --- Updates ---
    QGroupBox* updatesGroup = new QGroupBox(tr("Updates"));
    QFormLayout* updatesLayout = new QFormLayout(updatesGroup);

    checkUpdatesCheck_ = new QCheckBox(tr("Check for updates on startup"));
    updatesLayout->addRow(checkUpdatesCheck_);

    tabLayout->addWidget(updatesGroup);

    tabLayout->addStretch();
    return wrapInScrollArea(tab);
}

// =============================================================================
// Tab: Network
// =============================================================================
QWidget* SettingsDialog::createNetworkTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* tabLayout = new QVBoxLayout(tab);
    tabLayout->setSpacing(16);
    tabLayout->setContentsMargins(12, 12, 12, 12);

    // --- Ports ---
    QGroupBox* portsGroup = new QGroupBox(tr("Ports"));
    QFormLayout* portsLayout = new QFormLayout(portsGroup);
    portsLayout->setSpacing(10);

    p2pPortSpin_ = new QSpinBox();
    p2pPortSpin_->setRange(1024, 65535);
    portsLayout->addRow(tr("P2P Port:"), p2pPortSpin_);

    dhtPortSpin_ = new QSpinBox();
    dhtPortSpin_->setRange(1024, 65535);
    portsLayout->addRow(tr("DHT Port:"), dhtPortSpin_);

    httpPortSpin_ = new QSpinBox();
    httpPortSpin_->setRange(1024, 65535);
    portsLayout->addRow(tr("HTTP API Port:"), httpPortSpin_);

    QLabel* portsHint = new QLabel(tr("* Changing ports requires restart"));
    portsHint->setObjectName("hintLabel");
    portsLayout->addRow(portsHint);

    tabLayout->addWidget(portsGroup);

    // --- P2P Network ---
    QGroupBox* p2pGroup = new QGroupBox(tr("P2P Network"));
    QFormLayout* p2pLayout = new QFormLayout(p2pGroup);
    p2pLayout->setSpacing(10);

    p2pConnectionsSpin_ = new QSpinBox();
    p2pConnectionsSpin_->setRange(10, 1000);
    p2pConnectionsSpin_->setToolTip(tr("Maximum number of P2P connections"));
    p2pLayout->addRow(tr("Max connections:"), p2pConnectionsSpin_);

    p2pReplicationCheck_ = new QCheckBox(tr("Enable P2P replication (client)"));
    p2pReplicationCheck_->setToolTip(tr("Replicate database from other peers"));
    p2pLayout->addRow(p2pReplicationCheck_);

    p2pReplicationServerCheck_ = new QCheckBox(tr("Enable P2P replication server"));
    p2pReplicationServerCheck_->setToolTip(tr("Serve database to other peers"));
    p2pLayout->addRow(p2pReplicationServerCheck_);

    tabLayout->addWidget(p2pGroup);

    // --- REST API ---
    QGroupBox* apiGroup = new QGroupBox(tr("REST API"));
    QFormLayout* apiLayout = new QFormLayout(apiGroup);

    restApiCheck_ = new QCheckBox(tr("Enable REST API server"));
    apiLayout->addRow(restApiCheck_);

    tabLayout->addWidget(apiGroup);

    tabLayout->addStretch();
    return wrapInScrollArea(tab);
}

// =============================================================================
// Tab: Indexer
// =============================================================================
QWidget* SettingsDialog::createIndexerTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* tabLayout = new QVBoxLayout(tab);
    tabLayout->setSpacing(16);
    tabLayout->setContentsMargins(12, 12, 12, 12);

    // --- DHT Indexer ---
    QGroupBox* indexerGroup = new QGroupBox(tr("DHT Indexer"));
    QFormLayout* indexerLayout = new QFormLayout(indexerGroup);
    indexerLayout->setSpacing(10);

    indexerCheck_ = new QCheckBox(tr("Enable DHT indexer"));
    indexerCheck_->setToolTip(tr("Crawl the DHT network for new torrents"));
    indexerLayout->addRow(indexerCheck_);

    trackersCheck_ = new QCheckBox(tr("Enable tracker checking"));
    trackersCheck_->setToolTip(tr("Check trackers for seeders/leechers info"));
    indexerLayout->addRow(trackersCheck_);

    tabLayout->addWidget(indexerGroup);

    // --- Spider Performance ---
    QGroupBox* perfGroup = new QGroupBox(tr("Spider Performance"));
    QFormLayout* perfLayout = new QFormLayout(perfGroup);
    perfLayout->setSpacing(10);

    walkIntervalSpin_ = new QSpinBox();
    walkIntervalSpin_->setRange(1, 500);
    walkIntervalSpin_->setToolTip(tr("Interval between DHT walks (lower = faster, more CPU)"));
    perfLayout->addRow(tr("Walk interval:"), walkIntervalSpin_);

    QLabel* perfHint = new QLabel(tr("* Lower walk interval = faster indexing but higher CPU usage"));
    perfHint->setObjectName("hintLabel");
    perfHint->setWordWrap(true);
    perfLayout->addRow(perfHint);

    tabLayout->addWidget(perfGroup);

    tabLayout->addStretch();
    return wrapInScrollArea(tab);
}

// =============================================================================
// Tab: Filters
// =============================================================================
QWidget* SettingsDialog::createFiltersTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* tabLayout = new QVBoxLayout(tab);
    tabLayout->setSpacing(16);
    tabLayout->setContentsMargins(12, 12, 12, 12);

    // --- Name Filters ---
    QGroupBox* nameGroup = new QGroupBox(tr("Name Filters"));
    QVBoxLayout* nameLayout = new QVBoxLayout(nameGroup);
    nameLayout->setSpacing(10);

    QHBoxLayout* regexRow = new QHBoxLayout();
    QLabel* regexLabel = new QLabel(tr("Name filter (regex):"));
    regexEdit_ = new QLineEdit();
    regexEdit_->setPlaceholderText(tr("Regular expression pattern..."));

    QComboBox* regexExamples = new QComboBox();
    regexExamples->addItem(tr("Examples..."), "");
    regexExamples->addItem(tr("Russian + English only"),
        QString::fromUtf8(R"(^[А-Яа-я0-9A-Za-z.!@?#"$%&:;() *\+,\/;\-=[\\\]\^_{|}<>\u0400-\u04FF]+$)"));
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
    nameLayout->addLayout(regexRow);

    regexNegativeCheck_ = new QCheckBox(tr("Negative regex filter (reject matches)"));
    regexNegativeCheck_->setToolTip(tr("When enabled, torrents matching the regex will be rejected"));
    nameLayout->addWidget(regexNegativeCheck_);

    QLabel* regexHint = new QLabel(tr("* Empty string = Disabled"));
    regexHint->setObjectName("hintLabel");
    nameLayout->addWidget(regexHint);

    adultFilterCheck_ = new QCheckBox(tr("Adult content filter (ignore XXX content)"));
    adultFilterCheck_->setToolTip(tr("When enabled, adult content will be filtered out"));
    nameLayout->addWidget(adultFilterCheck_);

    tabLayout->addWidget(nameGroup);

    // --- Size & File Limits ---
    QGroupBox* sizeGroup = new QGroupBox(tr("Size && File Limits"));
    QVBoxLayout* sizeGroupLayout = new QVBoxLayout(sizeGroup);
    sizeGroupLayout->setSpacing(10);

    // Max files per torrent
    QHBoxLayout* maxFilesRow = new QHBoxLayout();
    QLabel* maxFilesLabel = new QLabel(tr("Max files per torrent:"));
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
    sizeGroupLayout->addLayout(maxFilesRow);

    QLabel* maxFilesHint = new QLabel(tr("* 0 = Disabled (no limit)"));
    maxFilesHint->setObjectName("hintLabel");
    sizeGroupLayout->addWidget(maxFilesHint);

    // Size filter
    QFormLayout* sizeLayout = new QFormLayout();
    sizeLayout->setSpacing(8);

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

    sizeGroupLayout->addLayout(sizeLayout);
    tabLayout->addWidget(sizeGroup);

    // --- Content Type Filter ---
    QGroupBox* contentTypeBox = new QGroupBox(tr("Content Type Filter"));
    QVBoxLayout* contentTypeLayout = new QVBoxLayout(contentTypeBox);

    QLabel* contentTypeHint = new QLabel(tr("Uncheck to disable specific content types:"));
    contentTypeHint->setObjectName("hintLabel");
    contentTypeLayout->addWidget(contentTypeHint);

    QGridLayout* typeGrid = new QGridLayout();
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
    tabLayout->addWidget(contentTypeBox);

    tabLayout->addStretch();
    return wrapInScrollArea(tab);
}

// =============================================================================
// Tab: Storage
// =============================================================================
QWidget* SettingsDialog::createStorageTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* tabLayout = new QVBoxLayout(tab);
    tabLayout->setSpacing(16);
    tabLayout->setContentsMargins(12, 12, 12, 12);

    // --- Downloads ---
    QGroupBox* downloadGroup = new QGroupBox(tr("Downloads"));
    QFormLayout* downloadLayout = new QFormLayout(downloadGroup);
    downloadLayout->setSpacing(10);

    QHBoxLayout* downloadPathLayout = new QHBoxLayout();
    downloadPathEdit_ = new QLineEdit();
    QPushButton* browseDownloadBtn = new QPushButton(tr("Browse..."));
    browseDownloadBtn->setObjectName("secondaryButton");
    connect(browseDownloadBtn, &QPushButton::clicked, this, [this]() {
        QString dir
            = QFileDialog::getExistingDirectory(this, tr("Select Download Directory"), downloadPathEdit_->text());
        if (!dir.isEmpty()) {
            downloadPathEdit_->setText(dir);
        }
    });
    downloadPathLayout->addWidget(downloadPathEdit_);
    downloadPathLayout->addWidget(browseDownloadBtn);
    downloadLayout->addRow(tr("Default directory:"), downloadPathLayout);

    QLabel* downloadPathHint = new QLabel(tr("* Default location for downloaded torrents"));
    downloadPathHint->setObjectName("hintLabel");
    downloadLayout->addRow(downloadPathHint);

    tabLayout->addWidget(downloadGroup);

    // --- Data Directory ---
    QGroupBox* dbGroup = new QGroupBox(tr("Data Directory"));
    QFormLayout* dbLayout = new QFormLayout(dbGroup);
    dbLayout->setSpacing(10);

    QHBoxLayout* pathLayout = new QHBoxLayout();
    dataPathEdit_ = new QLineEdit();
    QPushButton* browseBtn = new QPushButton(tr("Browse..."));
    browseBtn->setObjectName("secondaryButton");
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseDataPath);
    pathLayout->addWidget(dataPathEdit_);
    pathLayout->addWidget(browseBtn);
    dbLayout->addRow(tr("Path:"), pathLayout);

    QLabel* dataPathHint = new QLabel(tr("* Database and configuration storage. Changing requires restart."));
    dataPathHint->setObjectName("hintLabel");
    dataPathHint->setWordWrap(true);
    dbLayout->addRow(dataPathHint);

    tabLayout->addWidget(dbGroup);

    // --- Database Cleanup ---
    QGroupBox* cleanupBox = new QGroupBox(tr("Database Cleanup"));
    QVBoxLayout* cleanupLayout = new QVBoxLayout(cleanupBox);
    cleanupLayout->setSpacing(10);

    QLabel* cleanupDesc = new QLabel(tr("Check and remove torrents that don't match the current filters:"));
    cleanupDesc->setWordWrap(true);
    cleanupLayout->addWidget(cleanupDesc);

    cleanupProgress_ = new QLabel("");
    cleanupProgress_->setObjectName("cleanupProgressLabel");
    cleanupLayout->addWidget(cleanupProgress_);

    cleanupProgressBar_ = new QProgressBar();
    cleanupProgressBar_->setVisible(false);
    cleanupProgressBar_->setRange(0, 100);
    cleanupLayout->addWidget(cleanupProgressBar_);

    QHBoxLayout* cleanupBtnRow = new QHBoxLayout();
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

    tabLayout->addWidget(cleanupBox);

    tabLayout->addStretch();
    return wrapInScrollArea(tab);
}

// =============================================================================
// Load / Save
// =============================================================================

void SettingsDialog::loadSettings()
{
    if (!app_ || !app_->config())
        return;
    auto* config_ = app_->config();

    // General
    QString currentLang = config_->language();
    for (int i = 0; i < languageCombo_->count(); ++i) {
        if (languageCombo_->itemData(i).toString() == currentLang) {
            languageCombo_->setCurrentIndex(i);
            break;
        }
    }
    darkModeCheck_->setChecked(config_->darkMode());
    autoStartCheck_->setChecked(AutoStartManager::isEnabled());
    startMinimizedCheck_->setChecked(config_->startMinimized());
    minimizeToTrayCheck_->setChecked(config_->trayOnMinimize());
    closeToTrayCheck_->setChecked(config_->trayOnClose());
    checkUpdatesCheck_->setChecked(config_->checkUpdatesOnStartup());

    // Network
    p2pPortSpin_->setValue(config_->p2pPort());
    dhtPortSpin_->setValue(config_->dhtPort());
    httpPortSpin_->setValue(config_->httpPort());
    restApiCheck_->setChecked(config_->restApiEnabled());
    p2pConnectionsSpin_->setValue(config_->p2pConnections());
    p2pReplicationCheck_->setChecked(config_->p2pReplication());
    p2pReplicationServerCheck_->setChecked(config_->p2pReplicationServer());

    // Indexer
    indexerCheck_->setChecked(config_->indexerEnabled());
    trackersCheck_->setChecked(config_->trackersEnabled());
    walkIntervalSpin_->setValue(config_->spiderWalkInterval());

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
    QStringList enabledTypes = currentContentTypes.isEmpty()
        ? QStringList { "video", "audio", "pictures", "books", "application", "archive", "disc" }
        : currentContentTypes.split(",", Qt::SkipEmptyParts);

    videoCheck_->setChecked(enabledTypes.contains("video"));
    audioCheck_->setChecked(enabledTypes.contains("audio"));
    picturesCheck_->setChecked(enabledTypes.contains("pictures"));
    booksCheck_->setChecked(enabledTypes.contains("books"));
    appsCheck_->setChecked(enabledTypes.contains("application"));
    archivesCheck_->setChecked(enabledTypes.contains("archive"));
    discsCheck_->setChecked(enabledTypes.contains("disc"));

    // Storage
    downloadPathEdit_->setText(config_->downloadPath());

    // Database - QSettings is the source of truth for the data directory (it has
    // to be readable before rats.json, which lives inside it, can be loaded).
    QSettings settings("RatsSearch", "RatsSearch");
    QString savedDataDir = settings.value("dataDirectory").toString();
    if (savedDataDir.isEmpty()) {
        savedDataDir = dataDirectory_; // Use runtime directory if not saved
    }
    dataPathEdit_->setText(savedDataDir);
}

void SettingsDialog::saveSettings()
{
    if (!app_ || !app_->config())
        return;
    auto* config_ = app_->config();

    QSettings settings("RatsSearch", "RatsSearch");

    // Track what changed for restart notification
    int oldP2pPort = config_->p2pPort();
    int oldDhtPort = config_->dhtPort();
    int oldHttpPort = config_->httpPort();
    bool oldRestApi = config_->restApiEnabled();
    QString oldDataDir = settings.value("dataDirectory").toString();

    // Save General
    config_->setLanguage(languageCombo_->currentData().toString());
    config_->setDarkMode(darkModeCheck_->isChecked());
    config_->setStartMinimized(startMinimizedCheck_->isChecked());
    config_->setTrayOnMinimize(minimizeToTrayCheck_->isChecked());
    config_->setTrayOnClose(closeToTrayCheck_->isChecked());
    config_->setCheckUpdatesOnStartup(checkUpdatesCheck_->isChecked());

    // Autostart lives in the OS (registry / .desktop / launch agent), which is its
    // only source of truth — loadSettings() reads it back from AutoStartManager.
    bool autoStartEnabled = autoStartCheck_->isChecked();
    if (autoStartEnabled != AutoStartManager::isEnabled()) {
        if (!AutoStartManager::setEnabled(autoStartEnabled)) {
            qWarning() << "Failed to" << (autoStartEnabled ? "enable" : "disable") << "autostart";
        }
    }

    // Save Network
    config_->setP2pPort(p2pPortSpin_->value());
    config_->setDhtPort(dhtPortSpin_->value());
    config_->setHttpPort(httpPortSpin_->value());
    config_->setRestApiEnabled(restApiCheck_->isChecked());
    config_->setP2pConnections(p2pConnectionsSpin_->value());
    config_->setP2pReplication(p2pReplicationCheck_->isChecked());
    config_->setP2pReplicationServer(p2pReplicationServerCheck_->isChecked());

    // Save Indexer
    config_->setIndexerEnabled(indexerCheck_->isChecked());
    config_->setTrackersEnabled(trackersCheck_->isChecked());
    config_->setSpiderWalkInterval(walkIntervalSpin_->value());

    // Save Filters
    config_->setFiltersMaxFiles(maxFilesSpin_->value());
    config_->setFiltersNamingRegExp(regexEdit_->text());
    config_->setFiltersNamingRegExpNegative(regexNegativeCheck_->isChecked());
    config_->setFiltersAdultFilter(adultFilterCheck_->isChecked());
    config_->setFiltersSizeMin(static_cast<qint64>(sizeMinSpin_->value()) * 1024 * 1024);
    config_->setFiltersSizeMax(static_cast<qint64>(sizeMaxSpin_->value()) * 1024 * 1024);

    // Build content type filter string
    QStringList contentTypes;
    if (videoCheck_->isChecked())
        contentTypes << "video";
    if (audioCheck_->isChecked())
        contentTypes << "audio";
    if (picturesCheck_->isChecked())
        contentTypes << "pictures";
    if (booksCheck_->isChecked())
        contentTypes << "books";
    if (appsCheck_->isChecked())
        contentTypes << "application";
    if (archivesCheck_->isChecked())
        contentTypes << "archive";
    if (discsCheck_->isChecked())
        contentTypes << "disc";

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

    // Save Data Directory to QSettings, where main.cpp reads it at startup.
    QString newDataDir = dataPathEdit_->text();
    if (!newDataDir.isEmpty()) {
        settings.setValue("dataDirectory", newDataDir);
    }

    // Check if restart needed (only for settings that can't be applied at
    // runtime)
    needsRestart_ = (p2pPortSpin_->value() != oldP2pPort) || (dhtPortSpin_->value() != oldDhtPort)
        || (httpPortSpin_->value() != oldHttpPort) || (restApiCheck_->isChecked() != oldRestApi)
        || (newDataDir != oldDataDir && !newDataDir.isEmpty());

    // Save config to file
    config_->save();
}

// =============================================================================
// Slots
// =============================================================================

void SettingsDialog::runCleanup(bool dryRun)
{
    cleanupProgressBar_->setVisible(false);
    cleanupProgress_->setObjectName("cleanupProgressLabel");
    cleanupProgress_->setText(dryRun ? tr("Checking torrents against filters...") : tr("Cleaning torrents..."));
    QApplication::processEvents(); // paint the status before the synchronous
                                   // sweep

    app_->api()->call(
        QStringLiteral("torrent.cleanup"), { { "dryRun", dryRun } }, [this, dryRun](const rats::Result& r) {
            if (!r.ok()) {
                cleanupProgress_->setText(tr("Cleanup failed: %1").arg(r.error()));
                return;
            }
            const QJsonObject data = r.data().toObject();
            const int matched = data["matched"].toInt();
            const int scanned = data["scanned"].toInt();
            cleanupProgress_->setText(dryRun
                    ? tr("%1 of %2 torrents don't match the current filters.").arg(matched).arg(scanned)
                    : tr("Removed %1 torrents that didn't match the filters.").arg(matched));
        });
}

void SettingsDialog::onCheckTorrentsClicked()
{
    runCleanup(/*dryRun*/ true);
}

void SettingsDialog::onCleanTorrentsClicked()
{
    runCleanup(/*dryRun*/ false);
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
