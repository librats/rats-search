#ifndef RATS_REST_API_SERVER_H
#define RATS_REST_API_SERVER_H

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <limits>
#include <memory>

class QTcpServer;
class QTcpSocket;
class QWebSocket;
class QWebSocketServer;

namespace rats::app {
class Application;
}

namespace rats::rest {

class ApiRouter;

// HTTP REST + WebSocket front-end for the ApiRouter method surface.
//
// - REST:      GET /api/{method}?{params}  ->  { success, data|error, requestId
// }
// - WebSocket: { method, params, id }      ->  { success, data|error, requestId
// }
//              plus pushed events           ->  { event, data }
//
// Every request is translated into a single router->call(method, params,
// respond) dispatch; the Result handed back is serialized into the legacy
// response envelope. Push events emitted by ApiRouter::event() are broadcast to
// all connected WebSocket clients (download progress, feed updates, config
// changes) through a single event channel.
class ApiServer : public QObject {
    Q_OBJECT

public:
    explicit ApiServer(ApiRouter* router, QObject* parent = nullptr);
    ~ApiServer() override;

    // Start listening.
    //   httpPort: port for the HTTP REST API (0 disables it).
    //   wsPort:   port for WebSocket (0 disables, -1 shares by using httpPort +
    //   1).
    bool start(int httpPort = 8095, int wsPort = -1);
    void stop();

private:
    // Push an event to every connected WebSocket client.
    void broadcastEvent(const QString& event, const QJsonValue& data);

    // Set webui directory for static file serving
    void setWebuiDir(const QString& dir);

    // Set application pointer for metrics (P2P, DB, crawler, downloads)
    void setApplication(rats::app::Application* app);

    // Health & Metrics endpoints
    QByteArray handleHealthz() const;
    QByteArray handleReadyz() const;
    QByteArray handleMetrics() const;
    QByteArray handleStaticFile(const QString& path) const;

signals:
    void started();
    void stopped();
    void error(const QString& message);

private:
    void onHttpReadyRead(QTcpSocket* socket);
    void dispatchHttp(QTcpSocket* socket, const QString& method, const QJsonObject& params);
    void onWsMessage(QWebSocket* socket, const QString& message);

    ApiRouter* router_ = nullptr;
    rats::app::Application* app_ = nullptr;
    std::unique_ptr<QTcpServer> httpServer_;
    std::unique_ptr<QWebSocketServer> wsServer_;
    QList<QWebSocket*> wsClients_;
    QHash<QTcpSocket*, QByteArray> httpBuffers_;
    bool running_ = false;
    QString webuiDir_;

    // Metrics tracking
    qint64 startTimeMs_ = 0;
    qint64 httpRequestsTotal_ = 0;
    qint64 httpRequestsSuccess_ = 0;
    qint64 httpRequestsError_ = 0;
    qint64 wsMessagesTotal_ = 0;
    QMap<QString, qint64> requestsByMethod_;
    QMap<int, qint64> requestsByStatus_;
    QMap<QString, qint64> requestStartTimes_;
    static constexpr int DURATION_BUCKETS_COUNT = 12;
    static constexpr double DURATION_BUCKETS[DURATION_BUCKETS_COUNT] = {
        0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0, std::numeric_limits<double>::infinity()
    };
    qint64 durationBucketCounts[DURATION_BUCKETS_COUNT] = {};
    double durationSumMs_ = 0;
    qint64 durationCount_ = 0;
    qint64 searchQueriesTotal_ = 0;
    qint64 searchDurationSumMs_ = 0;
    qint64 searchDurationBuckets_[DURATION_BUCKETS_COUNT] = {};
    mutable qint64 dbIndexSizeBytes_ = 0;
    mutable qint64 dbIndexSizeLastUpdate_ = 0;
};

} // namespace rats::rest

#endif // RATS_REST_API_SERVER_H
