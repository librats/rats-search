#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QComboBox>
#include <QHash>
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
class QTableView;
class SearchResultModel;
class TorrentItemDelegate;
class TorrentDetailsPanel;
class QMenu;
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

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onSearchButtonClicked();
    void onSearchTextChanged(const QString& text);
    void onTorrentSelected(const QModelIndex& index);
    void onTorrentDoubleClicked(const QModelIndex& index);
    void onSortOrderChanged(int index);
    void onPeerCountChanged(int count);
    void onSpiderStatusChanged(const QString& status);
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
    void updateStatusBar();
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
    QPushButton* searchButton = nullptr;
    QComboBox* sortComboBox = nullptr;
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
};

#endif // MAINWINDOW_H
