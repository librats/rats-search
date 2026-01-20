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
class QTextEdit;
class QMenu;
class TopTorrentsWidget;
class FeedWidget;
class DownloadsWidget;

// Managers
class DownloadManager;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

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
    void setupToolBar();
    void setupStatusBar();
    void connectSignals();
    void startServices();
    void stopServices();
    void performSearch(const QString &query);
    void updateStatusBar();
    void applyDarkTheme();
    void logActivity(const QString &message);
    void setupSystemTray();
    void loadSettings();
    void saveSettings();

    // UI Components
    Ui::MainWindow *ui;
    QLineEdit *searchLineEdit;
    QPushButton *searchButton;
    QComboBox *sortComboBox;
    QTableView *resultsTableView;
    QTabWidget *tabWidget;
    QSplitter *mainSplitter;
    TorrentDetailsPanel *detailsPanel;
    QTextEdit *activityLog;
    
    // New tab widgets (migrated from legacy)
    TopTorrentsWidget *topTorrentsWidget;
    FeedWidget *feedWidget;
    DownloadsWidget *downloadsWidget;
    
    // Status bar
    QLabel *p2pStatusLabel;
    QLabel *peerCountLabel;
    QLabel *torrentCountLabel;
    QLabel *spiderStatusLabel;
    
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
    QString currentSortField_;
    bool currentSortDesc_;
    
    // System Tray
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
};

#endif // MAINWINDOW_H

