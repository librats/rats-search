#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QStatusBar>
#include <QTabWidget>
#include <memory>

class SearchEngine;
class TorrentDatabase;
class TorrentSpider;
class P2PNetwork;
class QLineEdit;
class QPushButton;
class QTableView;
class SearchResultModel;

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

private slots:
    void onSearchButtonClicked();
    void onSearchTextChanged(const QString &text);
    void onTorrentDoubleClicked(const QModelIndex &index);
    void onP2PStatusChanged(const QString &status);
    void onPeerCountChanged(int count);
    void onSpiderStatusChanged(const QString &status);
    void onTorrentIndexed(const QString &infoHash, const QString &name);
    void showSettings();
    void showAbout();
    void showStatistics();

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

    // UI Components
    Ui::MainWindow *ui;
    QLineEdit *searchLineEdit;
    QPushButton *searchButton;
    QTableView *resultsTableView;
    QTabWidget *tabWidget;
    
    // Status bar
    QLabel *p2pStatusLabel;
    QLabel *peerCountLabel;
    QLabel *torrentCountLabel;
    QLabel *spiderStatusLabel;
    
    // Core components
    std::unique_ptr<SearchEngine> searchEngine;
    std::unique_ptr<TorrentDatabase> torrentDatabase;
    std::unique_ptr<TorrentSpider> torrentSpider;
    std::unique_ptr<P2PNetwork> p2pNetwork;
    
    // Models
    SearchResultModel *searchResultModel;
    
    // Configuration
    int p2pPort_;
    int dhtPort_;
    QString dataDirectory_;
    
    // State
    bool servicesStarted_;
};

#endif // MAINWINDOW_H

