#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCheckBox>
#include <QComboBox>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QSplitter>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTimer>

#include "domain/torrent.h"

// Composition root — owns every service. MainWindow is now pure UI and only
// borrows it (non-owning).
namespace rats::app {
class Application;
}

// UI components
class QLineEdit;
class QPushButton;
class QCompleter;
class QStringListModel;
class QTableView;
class SearchResultModel;
class TorrentItemDelegate;
class TorrentDetailsPanel;
class QMenu;
class QToolButton;
class QSpinBox;
class QDoubleSpinBox;
class TopTorrentsWidget;
class FeedWidget;
class DownloadsWidget;
class TorrentFilesWidget;
class ActivityWidget;
class FavoritesWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    // The Application is already started by main() before MainWindow exists;
    // MainWindow reaches every service through it and never owns/starts one.
    explicit MainWindow(rats::app::Application* app, QWidget* parent = nullptr);
    ~MainWindow();

public slots:
    /// Un-minimize, un-hide and focus the window (tray click, second launch).
    void bringToFront();

protected:
    void closeEvent(QCloseEvent* event) override;
    // Watches the search field so clicking/focusing it while empty drops down
    // the search history.
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onSearchButtonClicked();
    void onSearchTextChanged(const QString& text);
    // Standard line-edit menu plus the search-history entries.
    void showSearchContextMenu(const QPoint& pos);
    void clearSearchHistory();
    void onTorrentSelected(const QModelIndex& index);
    void onTorrentDoubleClicked(const QModelIndex& index);
    void onSortOrderChanged(int index);
    void onPeerCountChanged(int count);
    void refreshSpiderStatus();
    void updateNetworkStatus(); // Timer-based status update
    void onTorrentIndexed(const rats::domain::Torrent& torrent);
    void onDetailsPanelCloseRequested();
    void onDownloadRequested(const QString& hash);
    void showSettings();
    void showAbout();
    void showChangelog();
    void showTorrentContextMenu(const QPoint& pos);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void toggleWindowVisibility();

    // Torrent management slots
    void addTorrentFile(); // Add .torrent file to search index
    void createTorrent(); // Create torrent from file/directory and seed
    void exportDatabase(); // Write the whole index to a portable .ratsdb dump
    void importDatabase(); // Merge somebody else's .ratsdb dump into the index
    void pullDatabaseFromPeer(); // Ask a connected peer for its whole index

    // Settings slots - applied immediately
    void onDarkModeChanged(bool enabled);

    // Update slots
    void onUpdateAvailable(const QString& version, const QString& releaseNotes);
    void onUpdateDownloadProgress(int percent);
    void onUpdateReady();
    void onUpdateError(const QString& error);
    void checkForUpdates();

    // GitHub slots
    void reportBug();
    void requestFeature();

    // Tab change handler
    void onTabChanged(int index);

private:
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();
    void wireWidgets(); // hand app_ to every tab/panel
    void connectSignals();
    void connectSearchSignals();
    void connectTabSignals();
    void connectDetailsSignals();
    void connectServiceSignals(); // transport / repository / indexing / peers
    void connectPeerSignals(); // remote P2P results streamed into the UI
    void performSearch(const QString& query);

    // Size / file-count ranges from the "Filters" popup. 0 is "no bound" on
    // every field — the shape both TorrentRepository and the P2P wire expect.
    struct SearchFilters {
        qint64 sizeMin = 0;
        qint64 sizeMax = 0;
        int filesMin = 0;
        int filesMax = 0;

        bool isEmpty() const { return sizeMin == 0 && sizeMax == 0 && filesMin == 0 && filesMax == 0; }
        int activeCount() const
        {
            return (sizeMin > 0 ? 1 : 0) + (sizeMax > 0 ? 1 : 0) + (filesMin > 0 ? 1 : 0) + (filesMax > 0 ? 1 : 0);
        }
        bool operator==(const SearchFilters& other) const
        {
            return sizeMin == other.sizeMin && sizeMax == other.sizeMax && filesMin == other.filesMin
                && filesMax == other.filesMax;
        }
    };
    // Build the "Filters" drop-down (size from/to, files from/to) for the
    // search bar. Called from setupUi() before the button is laid out.
    void setupSearchFilters();
    // Read the popup's widgets into bytes / plain counts.
    SearchFilters currentSearchFilters() const;
    // Refresh the button label ("Filters (2)") and its tooltip, and keep the
    // max bounds from sitting below the min ones.
    void updateSearchFiltersButton();
    // Clear every range back to "any".
    void resetSearchFilters();
    // Set up the history dropdown on the search field (completer + event filter).
    void setupSearchHistory();
    // Re-fill the completer from app_->searchHistory(). Wired to its
    // historyChanged signal, so every recorded query shows up immediately.
    void refreshSearchHistory();
    // Drop down the full history (no prefix filtering). No-op when empty.
    void showSearchHistoryPopup();
    void updateStatusBar();
    // Transient status text (indexed torrents, downloads, migrations, …). It
    // goes into its own status-bar slot instead of QStatusBar::showMessage(),
    // which would hide the peer/DHT/torrent counters for the whole timeout.
    // timeoutMs <= 0 keeps the text until the next message.
    void showStatusMessage(const QString& message, int timeoutMs = 0);
    // One status-bar line for a database export/import/transfer progress payload.
    QString databaseSyncStatusText(const QJsonObject& info) const;
    QString databaseServeStatusText(const QJsonObject& info) const;
    void clearStatusMessage();
    // Re-elide statusMessageText_ to the label's current width.
    void updateStatusMessageElide();
    void applyTheme(bool darkMode);
    void setupSystemTray();
    void loadSettings();
    void saveSettings();
    // Recompute p2pState_ from the transport, then repaint. Call this one.
    void refreshP2PStatus();
    // Render the current p2pState_ into the status-bar indicator.
    void paintP2PIndicator();
    bool showAgreementDialog(); // Show EULA on first launch, returns true if accepted

    // Torrent detail / action helpers (used by multiple tabs)
    void showTorrentDetails(const rats::domain::Torrent& torrent);
    void openMagnetLink(const rats::domain::Torrent& torrent);
    void exportTorrentToFile(const rats::domain::Torrent& torrent);
    // A .torrent requested via exportTorrentToFile is ready: prompt for a save
    // location and copy it there. Wired to TorrentExporter::exportReady.
    void onExportReady(const QString& hash, const QString& name, const QString& cachePath);
    // Background data-migration progress → status bar. Wired to
    // MigrationService::migrationProgress.
    void onMigrationProgress(const QString& migrationId, qint64 current, qint64 total);
    void addToFavorites(const rats::domain::Torrent& torrent);

    // P2P Connection state for status indicator
    enum class P2PState {
        NotStarted, // Red - P2P not started
        NoConnection, // Yellow/Orange - No peers connected
        Connected // Green - Peers connected
    };
    P2PState p2pState_ = P2PState::NotStarted;

    // The running application (non-owning). All services are reached through it.
    rats::app::Application* app_ = nullptr;

    // UI Components
    QLineEdit* searchLineEdit = nullptr;
    QCompleter* searchCompleter = nullptr; // history dropdown on searchLineEdit
    QStringListModel* searchHistoryModel = nullptr; // completer's backing model
    QPushButton* searchButton = nullptr;
    QComboBox* sortComboBox = nullptr;
    QComboBox* typeComboBox = nullptr;
    QCheckBox* safeSearchCheckBox = nullptr;
    // Search-bar "Filters" drop-down: size and file-count ranges.
    QToolButton* filtersButton = nullptr;
    QMenu* filtersMenu = nullptr;
    QDoubleSpinBox* sizeMinSpin = nullptr;
    QDoubleSpinBox* sizeMaxSpin = nullptr;
    QComboBox* sizeMinUnit = nullptr;
    QComboBox* sizeMaxUnit = nullptr;
    QSpinBox* filesMinSpin = nullptr;
    QSpinBox* filesMaxSpin = nullptr;
    // Snapshot taken when the popup opens, so closing it only re-runs the
    // search when something actually changed.
    SearchFilters filtersOnOpen_;
    QTableView* resultsTableView = nullptr;
    QTabWidget* tabWidget = nullptr;
    QSplitter* mainSplitter = nullptr; // Horizontal: tabs + details
    QSplitter* verticalSplitter = nullptr; // Vertical: main content + files panel
    TorrentDetailsPanel* detailsPanel = nullptr;
    TorrentFilesWidget* filesWidget = nullptr; // Bottom panel for file list

    // Tab widgets
    TopTorrentsWidget* topTorrentsWidget = nullptr;
    FeedWidget* feedWidget = nullptr;
    DownloadsWidget* downloadsWidget = nullptr;
    ActivityWidget* activityWidget = nullptr;
    FavoritesWidget* favoritesWidget = nullptr;

    // Status bar
    QLabel* p2pStatusLabel = nullptr;
    QLabel* peerCountLabel = nullptr;
    QLabel* dhtNodeCountLabel = nullptr;
    QLabel* torrentCountLabel = nullptr;
    QLabel* spiderStatusLabel = nullptr;
    // Transient messages live here, next to (not on top of) the counters above.
    QLabel* statusMessageLabel = nullptr;
    QTimer* statusMessageTimer_ = nullptr;
    QString statusMessageText_;
    QTimer* statusUpdateTimer_ = nullptr;

    // Models and Delegates
    SearchResultModel* searchResultModel = nullptr;
    TorrentItemDelegate* torrentDelegate = nullptr;

    // State
    QString currentSearchQuery_;
    qint64 cachedTorrentCount_ = 0; // Local torrent count (from statistics)
    qint64 cachedRemoteTorrentCount_ = 0; // Sum of torrents advertised by peers
    // Last torrent selected in each non-search tab, so switching tabs can
    // restore the details panel without querying widget-specific accessors.
    QHash<QWidget*, rats::domain::Torrent> tabSelection_;

    // System Tray
    QSystemTrayIcon* trayIcon = nullptr;
    QMenu* trayMenu = nullptr;
    bool trayNotificationShown_ = false;

    // Set once the user has committed to installing an update. While true,
    // closeEvent() shuts the app down unconditionally — no tray-hide, no
    // confirmation prompt — so the external updater is never left waiting.
    bool updateInstalling_ = false;
};

#endif // MAINWINDOW_H
