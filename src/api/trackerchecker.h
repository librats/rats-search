#ifndef TRACKERCHECKER_H
#define TRACKERCHECKER_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QHash>
#include <QString>
#include <QHostAddress>
#include <QDateTime>
#include <functional>

/**
 * @brief TrackerResult - Result from tracker scrape
 */
struct TrackerResult {
    QString tracker;
    int seeders = 0;
    int completed = 0;
    int leechers = 0;
    bool success = false;
    QString error;
    
    bool isValid() const { return success && seeders >= 0; }
};

/**
 * @brief TrackerRequest - Pending tracker request
 */
struct TrackerRequest {
    QString hash;
    QString host;
    quint16 port;
    QDateTime created;
    std::function<void(const TrackerResult&)> callback;
    
    // Connection state
    quint32 connectionIdHigh = 0;
    quint32 connectionIdLow = 0;
    bool connected = false;
};

/**
 * @brief TrackerChecker - UDP tracker scraping for seeders/leechers info
 * 
 * Implements the BitTorrent UDP Tracker Protocol (BEP 15) scrape request
 * to retrieve seeders, leechers, and completed counts for torrents.
 * 
 * Usage:
 *   tracker.scrape("tracker.example.com", 6969, hash, [](const TrackerResult& r) {
 *       if (r.success) {
 *           qInfo() << "Seeders:" << r.seeders;
 *       }
 *   });
 */
class TrackerChecker : public QObject
{
    Q_OBJECT

public:
    explicit TrackerChecker(QObject *parent = nullptr);
    ~TrackerChecker();
    
    /**
     * @brief Initialize the tracker checker
     * @param port Local UDP port to bind
     * @return true if initialized successfully
     */
    bool initialize(quint16 port = 0);
    
    /**
     * @brief Close and release resources
     */
    void close();
    
    /**
     * @brief Check if initialized and ready
     */
    bool isReady() const;
    
    /**
     * @brief Get the bound local port
     */
    quint16 localPort() const;
    
    /**
     * @brief Scrape a torrent from a tracker
     * @param host Tracker hostname or IP
     * @param port Tracker port
     * @param hash Torrent info hash (40 char hex)
     * @param callback Called with result
     */
    void scrape(const QString& host, quint16 port, const QString& hash,
                std::function<void(const TrackerResult&)> callback);
    
    /**
     * @brief Scrape from multiple common trackers
     * @param hash Torrent info hash (40 char hex)
     * @param callback Called with aggregated result (best values)
     */
    void scrapeMultiple(const QString& hash,
                        std::function<void(const TrackerResult&)> callback);
    
    /**
     * @brief Get request timeout in milliseconds
     */
    int timeout() const { return timeoutMs_; }
    
    /**
     * @brief Set request timeout
     */
    void setTimeout(int ms) { timeoutMs_ = ms; }
    
    /**
     * @brief Get list of default trackers
     */
    static QStringList defaultTrackers();

signals:
    /**
     * @brief Emitted when scrape result is received
     */
    void scrapeResult(const QString& hash, const TrackerResult& result);

private slots:
    void onReadyRead();
    void onCleanupTimer();

private:
    // UDP protocol constants
    static constexpr quint32 CONNECT_ID_HIGH = 0x417;
    static constexpr quint32 CONNECT_ID_LOW = 0x27101980;
    static constexpr quint32 ACTION_CONNECT = 0;
    static constexpr quint32 ACTION_ANNOUNCE = 1;
    static constexpr quint32 ACTION_SCRAPE = 2;
    static constexpr quint32 ACTION_ERROR = 3;
    
    void sendConnect(quint32 transactionId, const QString& host, quint16 port);
    void sendScrape(quint32 transactionId, const TrackerRequest& request);
    void handleConnectResponse(const QByteArray& data);
    void handleScrapeResponse(const QByteArray& data);
    void handleError(quint32 transactionId, const QString& error);
    
    QByteArray hexToBytes(const QString& hex) const;
    
    QUdpSocket* socket_;
    QTimer* cleanupTimer_;
    int timeoutMs_;
    
    // Pending requests by transaction ID
    QHash<quint32, TrackerRequest> requests_;
    
    // For scrapeMultiple: collect results for a hash
    struct MultiScrapeState {
        int pending = 0;
        TrackerResult best;
        std::function<void(const TrackerResult&)> callback;
    };
    QHash<QString, MultiScrapeState> multiScrapes_;
};

#endif // TRACKERCHECKER_H
