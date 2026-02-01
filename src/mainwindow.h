#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QStatusBar>
#include <QTabWidget>
#include <QSplitter>
#include <QComboBox>
#include <QSystemTrayIcon>
#include <memory>

// Core components
class TorrentDatabase;
class TorrentSpider;
class P2PNetwork;
class TorrentClient;

// New API layer
class RatsAPI;
class ConfigManager;
class ApiServer;
class UpdateManager;

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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(int p2pPort, int dhtPort, const QString& dataDirectory, QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void changeEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onSearchButtonClicked();
    void onSearchTextChanged(const QString &text);
    void onTorrentSelected(const QModelIndex &index);
    void onTorrentDoubleClicked(const QModelIndex &index);
    void onSortOrderChanged(int index);
    void onP2PStatusChanged(const QString &status);
    void onPeerCountChanged(int count);
    void onSpiderStatusChanged(const QString &status);
    void onTorrentIndexed(const QString &infoHash, const QString &name);
    void onDatabaseStatisticsChanged(qint64 torrents, qint64 files, qint64 totalSize);
    void onDetailsPanelCloseRequested();
    void onMagnetLinkRequested(const QString &hash, const QString &name);
    void onDownloadRequested(const QString &hash);
    void showSettings();
    void showAbout();
    void showStatistics();
    void showTorrentContextMenu(const QPoint &pos);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void toggleWindowVisibility();
    void initializeServicesDeferred();  // Deferred heavy initialization
    
    // Torrent management slots
    void addTorrentFile();      // Add .torrent file to search index
    void createTorrent();       // Create torrent from file/directory and seed
    
    // Settings slots - applied immediately
    void onDarkModeChanged(bool enabled);
    void onLanguageChanged(const QString& languageCode);
    
    // Update slots
    void onUpdateAvailable(const QString& version, const QString& releaseNotes);
    void onUpdateDownloadProgress(int percent);
    void onUpdateReady();
    void onUpdateError(const QString& error);
    void checkForUpdates();
    void showUpdateDialog();

private:
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();
    void connectSignals();
    void startServices();
    void stopServices();
    void performSearch(const QString &query);
    void updateStatusBar();
    void updateStatisticsTab();
    void applyTheme(bool darkMode);
    void setupSystemTray();
    void loadSettings();
    void saveSettings();

    // UI Components
    QLineEdit *searchLineEdit;
    QPushButton *searchButton;
    QComboBox *sortComboBox;
    QTableView *resultsTableView;
    QTabWidget *tabWidget;
    QSplitter *mainSplitter;      // Horizontal: tabs + details
    QSplitter *verticalSplitter;  // Vertical: main content + files panel
    TorrentDetailsPanel *detailsPanel;
    TorrentFilesWidget *filesWidget;  // Bottom panel for file list
    
    // New tab widgets (migrated from legacy)
    TopTorrentsWidget *topTorrentsWidget;
    FeedWidget *feedWidget;
    DownloadsWidget *downloadsWidget;
    
    // Status bar
    QLabel *p2pStatusLabel;
    QLabel *peerCountLabel;
    QLabel *torrentCountLabel;
    QLabel *spiderStatusLabel;
    
    // Statistics tab labels
    QLabel *statsP2pLabel;
    QLabel *statsDbLabel;
    
    // Core components
    std::unique_ptr<TorrentDatabase> torrentDatabase;
    std::unique_ptr<TorrentSpider> torrentSpider;
    std::unique_ptr<P2PNetwork> p2pNetwork;
    std::unique_ptr<TorrentClient> torrentClient;
    
    // API layer
    std::unique_ptr<RatsAPI> api;
    std::unique_ptr<ConfigManager> config;
    std::unique_ptr<ApiServer> apiServer;
    std::unique_ptr<UpdateManager> updateManager;
    
    // Models and Delegates
    SearchResultModel *searchResultModel;
    TorrentItemDelegate *torrentDelegate;
    
    // Configuration
    QString dataDirectory_;
    
    // State
    bool servicesStarted_;
    QString currentSearchQuery_;
    
    // System Tray
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
    bool trayNotificationShown_ = false;
    
    // Cached statistics (updated via signals, no polling)
    qint64 cachedTorrents_ = 0;
    qint64 cachedFiles_ = 0;
    qint64 cachedTotalSize_ = 0;
    int cachedPeerCount_ = 0;
    int cachedDhtNodes_ = 0;
    bool cachedP2pConnected_ = false;
};

#endif // MAINWINDOW_H

