#ifndef RATS_REST_API_SERVER_H
#define RATS_REST_API_SERVER_H

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QString>
#include <memory>

class QTcpServer;
class QTcpSocket;
class QWebSocket;
class QWebSocketServer;

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

    bool isRunning() const;
    int httpPort() const;
    int wsPort() const;

    // Push an event to every connected WebSocket client.
    void broadcastEvent(const QString& event, const QJsonValue& data);

signals:
    void started();
    void stopped();
    void error(const QString& message);

private:
    void onHttpReadyRead(QTcpSocket* socket);
    void dispatchHttp(QTcpSocket* socket, const QString& method, const QJsonObject& params);
    void onWsMessage(QWebSocket* socket, const QString& message);

    ApiRouter* router_ = nullptr;
    std::unique_ptr<QTcpServer> httpServer_;
    std::unique_ptr<QWebSocketServer> wsServer_;
    QList<QWebSocket*> wsClients_;
    // Per-connection receive buffer so a request split across several readyRead
    // signals (or a POST body arriving after its headers) is reassembled before
    // parsing.
    QHash<QTcpSocket*, QByteArray> httpBuffers_;
    int httpPort_ = 0;
    int wsPort_ = 0;
    bool running_ = false;
};

} // namespace rats::rest

#endif // RATS_REST_API_SERVER_H
