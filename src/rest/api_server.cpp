#include "rest/api_server.h"

#include "common/result.h"
#include "rest/api_router.h"

#include <QDateTime>
#include <QDebug>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <QWebSocket>
#include <QWebSocketServer>

namespace rats::rest {

// ============================================================================
// HTTP parsing helpers
// ============================================================================

namespace {

struct HttpRequest {
    QString method;
    QString path;
    QUrlQuery query;
    QMap<QString, QString> headers; // lower-cased keys
    QByteArray body;
};

// Parse the request line and headers from the header block (bytes before the
// terminating blank line). The body is filled in separately by the caller once
// Content-Length bytes are available.
HttpRequest parseHttpHeader(const QByteArray& header)
{
    HttpRequest req;

    const QString str = QString::fromUtf8(header);
    const QStringList lines = str.split("\r\n");
    if (lines.isEmpty()) {
        return req;
    }

    const QStringList requestLine = lines[0].split(' ');
    if (requestLine.size() >= 2) {
        req.method = requestLine[0].toUpper();
        const QUrl url(requestLine[1]);
        req.path = url.path();
        req.query = QUrlQuery(url.query());
    }

    for (int i = 1; i < lines.size(); ++i) {
        if (lines[i].isEmpty()) {
            continue;
        }
        const int colon = lines[i].indexOf(':');
        if (colon > 0) {
            const QString key = lines[i].left(colon).trimmed().toLower();
            const QString value = lines[i].mid(colon + 1).trimmed();
            req.headers[key] = value;
        }
    }

    return req;
}

QByteArray buildHttpResponse(int statusCode, const QString& statusText, const QByteArray& body,
    const QString& contentType = QStringLiteral("application/json"))
{
    QByteArray response;
    response.append(QStringLiteral("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
    response.append(QStringLiteral("Content-Type: %1\r\n").arg(contentType).toUtf8());
    response.append(QStringLiteral("Content-Length: %1\r\n").arg(body.size()).toUtf8());
    response.append("Access-Control-Allow-Origin: *\r\n");
    response.append("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
    response.append("Access-Control-Allow-Headers: Content-Type\r\n");
    response.append("Connection: close\r\n");
    response.append("\r\n");
    response.append(body);
    return response;
}

// Coerce a raw query-string value into the most specific JSON type it looks
// like (object/array, number, bool, otherwise string) — matches the legacy
// behavior.
QJsonValue coerceQueryValue(const QString& value)
{
    if ((value.startsWith('{') && value.endsWith('}')) || (value.startsWith('[') && value.endsWith(']'))) {
        const QJsonDocument doc = QJsonDocument::fromJson(value.toUtf8());
        if (!doc.isNull()) {
            return doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
        }
    }

    bool ok = false;
    const int intVal = value.toInt(&ok);
    if (ok) {
        return QJsonValue(intVal);
    }

    if (value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
        return QJsonValue(true);
    }
    if (value.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0) {
        return QJsonValue(false);
    }

    return QJsonValue(value);
}

// Serialize a Result into the legacy response envelope.
QJsonObject resultToEnvelope(const Result& result, const QString& requestId)
{
    QJsonObject obj;
    obj["success"] = result.ok();
    if (result.ok()) {
        const QJsonValue& data = result.data();
        if (!data.isNull() && !data.isUndefined()) {
            obj["data"] = data;
        }
    } else if (!result.error().isEmpty()) {
        obj["error"] = result.error();
    }
    if (!requestId.isEmpty()) {
        obj["requestId"] = requestId;
    }
    return obj;
}

} // namespace

// ============================================================================
// ApiServer
// ============================================================================

ApiServer::ApiServer(ApiRouter* router, QObject* parent) : QObject(parent), router_(router)
{
    // Single unified event channel: whatever the router broadcasts is pushed to
    // every WebSocket client.
    if (router_) {
        connect(router_, &ApiRouter::event, this,
            [this](const QString& name, const QJsonObject& data) { broadcastEvent(name, data); });
    }
}

ApiServer::~ApiServer()
{
    stop();
}

bool ApiServer::start(int httpPort, int wsPort)
{
    if (running_) {
        return true;
    }

    // ---- HTTP server ----
    if (httpPort > 0) {
        httpServer_ = std::make_unique<QTcpServer>(this);

        connect(httpServer_.get(), &QTcpServer::newConnection, this, [this]() {
            while (httpServer_->hasPendingConnections()) {
                QTcpSocket* socket = httpServer_->nextPendingConnection();

                connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onHttpReadyRead(socket); });
                connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
                    httpBuffers_.remove(socket);
                    socket->deleteLater();
                });
            }
        });

        if (!httpServer_->listen(QHostAddress::Any, static_cast<quint16>(httpPort))) {
            qWarning() << "Failed to start HTTP server on port" << httpPort;
            emit error("Failed to start HTTP server: " + httpServer_->errorString());
            httpServer_.reset();
            return false;
        }

        httpPort_ = httpServer_->serverPort();
        qInfo() << "HTTP API server listening on port" << httpPort_;
    }

    // ---- WebSocket server ----
    int wsPortActual = wsPort;
    if (wsPort == -1 && httpPort > 0) {
        wsPortActual = httpPort + 1;
    }

    if (wsPortActual > 0) {
        wsServer_ = std::make_unique<QWebSocketServer>("RatsAPI", QWebSocketServer::NonSecureMode, this);

        connect(wsServer_.get(), &QWebSocketServer::newConnection, this, [this]() {
            while (wsServer_->hasPendingConnections()) {
                QWebSocket* socket = wsServer_->nextPendingConnection();
                wsClients_.append(socket);

                const QString address = socket->peerAddress().toString();
                qInfo() << "WebSocket client connected:" << address;

                connect(socket, &QWebSocket::textMessageReceived, this,
                    [this, socket](const QString& message) { onWsMessage(socket, message); });

                connect(socket, &QWebSocket::disconnected, this, [this, socket, address]() {
                    wsClients_.removeOne(socket);
                    socket->deleteLater();
                    qInfo() << "WebSocket client disconnected:" << address;
                });
            }
        });

        if (!wsServer_->listen(QHostAddress::Any, static_cast<quint16>(wsPortActual))) {
            qWarning() << "Failed to start WebSocket server on port" << wsPortActual;
            emit error("Failed to start WebSocket server: " + wsServer_->errorString());
            wsServer_.reset();
            return false;
        }

        wsPort_ = wsServer_->serverPort();
        qInfo() << "WebSocket server listening on port" << wsPort_;
    }

    running_ = true;
    emit started();
    return true;
}

void ApiServer::stop()
{
    if (!running_) {
        return;
    }

    for (QWebSocket* client : wsClients_) {
        client->close();
    }
    wsClients_.clear();
    httpBuffers_.clear();

    if (wsServer_) {
        wsServer_->close();
        wsServer_.reset();
    }
    if (httpServer_) {
        httpServer_->close();
        httpServer_.reset();
    }

    running_ = false;
    emit stopped();
    qInfo() << "API server stopped";
}

bool ApiServer::isRunning() const
{
    return running_;
}

int ApiServer::httpPort() const
{
    return httpPort_;
}

int ApiServer::wsPort() const
{
    return wsPort_;
}

void ApiServer::broadcastEvent(const QString& event, const QJsonValue& data)
{
    QJsonObject msg;
    msg["event"] = event;
    msg["data"] = data;

    const QString json = QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact));

    for (QWebSocket* client : wsClients_) {
        if (client->isValid()) {
            client->sendTextMessage(json);
        }
    }
}

// ----------------------------------------------------------------------------
// HTTP handling
// ----------------------------------------------------------------------------

void ApiServer::onHttpReadyRead(QTcpSocket* socket)
{
    QByteArray& buffer = httpBuffers_[socket];
    buffer.append(socket->readAll());

    // Wait until the full header block has arrived.
    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return;
    }

    HttpRequest req = parseHttpHeader(buffer.left(headerEnd));

    // For requests with a body, wait until Content-Length bytes are buffered.
    const int bodyStart = headerEnd + 4;
    const int contentLength = req.headers.value(QStringLiteral("content-length")).toInt();
    if (contentLength > 0) {
        if (buffer.size() - bodyStart < contentLength) {
            return; // more body bytes still incoming
        }
        req.body = buffer.mid(bodyStart, contentLength);
    }

    // Request fully received; this connection is single-shot (Connection: close).
    httpBuffers_.remove(socket);

    // CORS preflight.
    if (req.method == "OPTIONS") {
        socket->write(buildHttpResponse(200, "OK", QByteArray()));
        socket->disconnectFromHost();
        return;
    }

    if (!req.path.startsWith("/api/")) {
        socket->write(buildHttpResponse(404, "Not Found", "{\"error\":\"Not Found\"}"));
        socket->disconnectFromHost();
        return;
    }

    const QString method = req.path.mid(5); // strip "/api/"

    // Build params from the query string...
    QJsonObject params;
    const auto items = req.query.queryItems();
    for (const auto& item : items) {
        params[item.first] = coerceQueryValue(item.second);
    }
    // ...then overlay a JSON object body (POST), which takes precedence.
    if (!req.body.isEmpty()) {
        const QJsonDocument bodyDoc = QJsonDocument::fromJson(req.body);
        if (bodyDoc.isObject()) {
            const QJsonObject bodyObj = bodyDoc.object();
            for (auto it = bodyObj.begin(); it != bodyObj.end(); ++it) {
                params[it.key()] = it.value();
            }
        }
    }

    dispatchHttp(socket, method, params);
}

void ApiServer::dispatchHttp(QTcpSocket* socket, const QString& method, const QJsonObject& params)
{
    const QString requestId = QString::number(QDateTime::currentMSecsSinceEpoch(), 16);

    if (!router_) {
        const QJsonObject env = resultToEnvelope(Result::failure("API router unavailable"), requestId);
        socket->write(buildHttpResponse(400, "Bad Request", QJsonDocument(env).toJson()));
        socket->disconnectFromHost();
        return;
    }

    router_->call(method, params, [socket, requestId](const Result& result) {
        if (!socket->isOpen()) {
            return;
        }
        const QJsonObject env = resultToEnvelope(result, requestId);
        socket->write(buildHttpResponse(
            result.ok() ? 200 : 400, result.ok() ? "OK" : "Bad Request", QJsonDocument(env).toJson()));
        socket->disconnectFromHost();
    });
}

// ----------------------------------------------------------------------------
// WebSocket handling
// ----------------------------------------------------------------------------

void ApiServer::onWsMessage(QWebSocket* socket, const QString& message)
{
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        socket->sendTextMessage("{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }

    const QJsonObject obj = doc.object();
    const QString method = obj["method"].toString();
    const QJsonObject params = obj["params"].toObject();
    const QString requestId = obj["id"].toString();

    if (method.isEmpty()) {
        socket->sendTextMessage("{\"success\":false,\"error\":\"Missing method\"}");
        return;
    }

    if (!router_) {
        const QJsonObject env = resultToEnvelope(Result::failure("API router unavailable"), requestId);
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(env).toJson(QJsonDocument::Compact)));
        return;
    }

    router_->call(method, params, [socket, requestId](const Result& result) {
        if (!socket->isValid()) {
            return;
        }
        const QJsonObject env = resultToEnvelope(result, requestId);
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(env).toJson(QJsonDocument::Compact)));
    });
}

} // namespace rats::rest
