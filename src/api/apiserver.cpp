#include "apiserver.h"
#include "ratsapi.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

// ============================================================================
// Private Implementation
// ============================================================================

class ApiServer::Private {
public:
    RatsAPI* api = nullptr;
    std::unique_ptr<QTcpServer> httpServer;
    std::unique_ptr<QWebSocketServer> wsServer;
    QList<QWebSocket*> wsClients;
    int httpPort_ = 0;
    int wsPort_ = 0;
    bool running_ = false;
};

// ============================================================================
// Simple HTTP Parser (minimal implementation)
// ============================================================================

struct HttpRequest {
    QString method;
    QString path;
    QUrlQuery query;
    QMap<QString, QString> headers;
    QByteArray body;
};

static HttpRequest parseHttpRequest(const QByteArray& data)
{
    HttpRequest req;
    
    QString str = QString::fromUtf8(data);
    QStringList lines = str.split("\r\n");
    
    if (lines.isEmpty()) return req;
    
    // Parse request line
    QStringList requestLine = lines[0].split(' ');
    if (requestLine.size() >= 2) {
        req.method = requestLine[0];
        QUrl url(requestLine[1]);
        req.path = url.path();
        req.query = QUrlQuery(url.query());
    }
    
    // Parse headers
    int i = 1;
    while (i < lines.size() && !lines[i].isEmpty()) {
        int colonPos = lines[i].indexOf(':');
        if (colonPos > 0) {
            QString key = lines[i].left(colonPos).trimmed().toLower();
            QString value = lines[i].mid(colonPos + 1).trimmed();
            req.headers[key] = value;
        }
        ++i;
    }
    
    // Body would be after blank line
    if (i + 1 < lines.size()) {
        req.body = lines.mid(i + 1).join("\r\n").toUtf8();
    }
    
    return req;
}

static QByteArray buildHttpResponse(int statusCode, 
                                     const QString& statusText,
                                     const QByteArray& body,
                                     const QString& contentType = "application/json")
{
    QByteArray response;
    response.append(QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
    response.append(QString("Content-Type: %1\r\n").arg(contentType).toUtf8());
    response.append(QString("Content-Length: %1\r\n").arg(body.size()).toUtf8());
    response.append("Access-Control-Allow-Origin: *\r\n");
    response.append("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
    response.append("Access-Control-Allow-Headers: Content-Type\r\n");
    response.append("Connection: close\r\n");
    response.append("\r\n");
    response.append(body);
    return response;
}

// ============================================================================
// ApiServer Implementation
// ============================================================================

ApiServer::ApiServer(RatsAPI* api, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->api = api;
    
    // Connect API events for broadcasting
    if (api) {
        connect(api, &RatsAPI::remoteSearchResults, this, [this](const QString& searchId, const QJsonArray& torrents) {
            QJsonObject data;
            data["searchId"] = searchId;
            data["torrents"] = torrents;
            broadcastEvent("remoteSearchResults", data);
        });
        
        connect(api, &RatsAPI::downloadProgress, this, [this](const QString& hash, const QJsonObject& progress) {
            QJsonObject data = progress;
            data["hash"] = hash;
            broadcastEvent("downloadProgress", data);
        });
        
        connect(api, &RatsAPI::downloadCompleted, this, [this](const QString& hash, bool cancelled) {
            QJsonObject data;
            data["hash"] = hash;
            data["cancelled"] = cancelled;
            broadcastEvent("downloadCompleted", data);
        });
        
        connect(api, &RatsAPI::filesReady, this, [this](const QString& hash, const QJsonArray& files) {
            QJsonObject data;
            data["hash"] = hash;
            data["files"] = files;
            broadcastEvent("filesReady", data);
        });
        
        connect(api, &RatsAPI::configChanged, this, [this](const QJsonObject& config) {
            broadcastEvent("configChanged", config);
        });
        
        connect(api, &RatsAPI::votesUpdated, this, [this](const QString& hash, int good, int bad) {
            QJsonObject data;
            data["hash"] = hash;
            data["good"] = good;
            data["bad"] = bad;
            broadcastEvent("votesUpdated", data);
        });
        
        connect(api, &RatsAPI::feedUpdated, this, [this](const QJsonArray& feed) {
            QJsonObject data;
            data["feed"] = feed;
            broadcastEvent("feedUpdated", data);
        });
        
        connect(api, &RatsAPI::torrentIndexed, this, [this](const QString& hash, const QString& name) {
            QJsonObject data;
            data["hash"] = hash;
            data["name"] = name;
            broadcastEvent("torrentIndexed", data);
        });
    }
}

ApiServer::~ApiServer()
{
    stop();
}

bool ApiServer::start(int httpPort, int wsPort)
{
    if (d->running_) {
        return true;
    }
    
    // Start HTTP server
    if (httpPort > 0) {
        d->httpServer = std::make_unique<QTcpServer>(this);
        
        connect(d->httpServer.get(), &QTcpServer::newConnection, this, [this]() {
            while (d->httpServer->hasPendingConnections()) {
                QTcpSocket* socket = d->httpServer->nextPendingConnection();
                
                connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                    QByteArray data = socket->readAll();
                    HttpRequest req = parseHttpRequest(data);
                    
                    // Handle CORS preflight
                    if (req.method == "OPTIONS") {
                        socket->write(buildHttpResponse(200, "OK", ""));
                        socket->disconnectFromHost();
                        return;
                    }
                    
                    // Route to API
                    if (req.path.startsWith("/api/")) {
                        QString method = req.path.mid(5);  // Remove /api/
                        
                        // Convert query params to JSON
                        QJsonObject params;
                        for (const auto& item : req.query.queryItems()) {
                            QString value = item.second;
                            // Try to parse as JSON if it looks like an object/array
                            if ((value.startsWith('{') && value.endsWith('}')) ||
                                (value.startsWith('[') && value.endsWith(']'))) {
                                QJsonDocument doc = QJsonDocument::fromJson(value.toUtf8());
                                if (!doc.isNull()) {
                                    params[item.first] = doc.isArray() ? QJsonValue(doc.array()) 
                                                                       : QJsonValue(doc.object());
                                    continue;
                                }
                            }
                            // Try to parse as number
                            bool ok;
                            int intVal = value.toInt(&ok);
                            if (ok) {
                                params[item.first] = intVal;
                                continue;
                            }
                            // Try as bool
                            if (value.toLower() == "true") {
                                params[item.first] = true;
                                continue;
                            }
                            if (value.toLower() == "false") {
                                params[item.first] = false;
                                continue;
                            }
                            // Default to string
                            params[item.first] = value;
                        }
                        
                        QString requestId = QString::number(QDateTime::currentMSecsSinceEpoch(), 16);
                        
                        d->api->call(method, params, [socket, requestId](const ApiResponse& resp) {
                            if (!socket->isOpen()) return;
                            
                            QJsonDocument doc(resp.toJson());
                            socket->write(buildHttpResponse(resp.success ? 200 : 400, 
                                                           resp.success ? "OK" : "Bad Request",
                                                           doc.toJson()));
                            socket->disconnectFromHost();
                        }, requestId);
                        
                        return;
                    }
                    
                    // 404 for unknown paths
                    socket->write(buildHttpResponse(404, "Not Found", "{\"error\":\"Not Found\"}"));
                    socket->disconnectFromHost();
                });
                
                connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
            }
        });
        
        if (!d->httpServer->listen(QHostAddress::Any, httpPort)) {
            qWarning() << "Failed to start HTTP server on port" << httpPort;
            emit error("Failed to start HTTP server: " + d->httpServer->errorString());
            return false;
        }
        
        d->httpPort_ = d->httpServer->serverPort();
        qInfo() << "HTTP API server listening on port" << d->httpPort_;
    }
    
    // Start WebSocket server
    int wsPortActual = wsPort;
    if (wsPort == -1 && httpPort > 0) {
        wsPortActual = httpPort + 1;  // Use next port
    }
    
    if (wsPortActual > 0) {
        d->wsServer = std::make_unique<QWebSocketServer>(
            "RatsAPI", QWebSocketServer::NonSecureMode, this);
        
        connect(d->wsServer.get(), &QWebSocketServer::newConnection, this, [this]() {
            while (d->wsServer->hasPendingConnections()) {
                QWebSocket* socket = d->wsServer->nextPendingConnection();
                d->wsClients.append(socket);
                
                QString address = socket->peerAddress().toString();
                emit clientConnected(address);
                qInfo() << "WebSocket client connected:" << address;
                
                connect(socket, &QWebSocket::textMessageReceived, this, [this, socket](const QString& message) {
                    // Parse JSON-RPC style message
                    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
                    if (!doc.isObject()) {
                        socket->sendTextMessage("{\"error\":\"Invalid JSON\"}");
                        return;
                    }
                    
                    QJsonObject obj = doc.object();
                    QString method = obj["method"].toString();
                    QJsonObject params = obj["params"].toObject();
                    QString requestId = obj["id"].toString();
                    
                    if (method.isEmpty()) {
                        socket->sendTextMessage("{\"error\":\"Missing method\"}");
                        return;
                    }
                    
                    d->api->call(method, params, [socket, requestId](const ApiResponse& resp) {
                        if (!socket->isValid()) return;
                        
                        QJsonDocument doc(resp.toJson());
                        socket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
                    }, requestId);
                });
                
                connect(socket, &QWebSocket::disconnected, this, [this, socket, address]() {
                    d->wsClients.removeOne(socket);
                    socket->deleteLater();
                    emit clientDisconnected(address);
                    qInfo() << "WebSocket client disconnected:" << address;
                });
            }
        });
        
        if (!d->wsServer->listen(QHostAddress::Any, wsPortActual)) {
            qWarning() << "Failed to start WebSocket server on port" << wsPortActual;
            emit error("Failed to start WebSocket server: " + d->wsServer->errorString());
            return false;
        }
        
        d->wsPort_ = d->wsServer->serverPort();
        qInfo() << "WebSocket server listening on port" << d->wsPort_;
    }
    
    d->running_ = true;
    emit started();
    return true;
}

void ApiServer::stop()
{
    if (!d->running_) {
        return;
    }
    
    // Close WebSocket clients
    for (QWebSocket* client : d->wsClients) {
        client->close();
    }
    d->wsClients.clear();
    
    // Stop servers
    if (d->wsServer) {
        d->wsServer->close();
        d->wsServer.reset();
    }
    
    if (d->httpServer) {
        d->httpServer->close();
        d->httpServer.reset();
    }
    
    d->running_ = false;
    emit stopped();
    qInfo() << "API server stopped";
}

bool ApiServer::isRunning() const
{
    return d->running_;
}

int ApiServer::httpPort() const
{
    return d->httpPort_;
}

int ApiServer::wsPort() const
{
    return d->wsPort_;
}

int ApiServer::clientCount() const
{
    return d->wsClients.size();
}

void ApiServer::broadcastEvent(const QString& event, const QJsonValue& data)
{
    QJsonObject msg;
    msg["event"] = event;
    msg["data"] = data;
    
    QJsonDocument doc(msg);
    QString json = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    
    for (QWebSocket* client : d->wsClients) {
        if (client->isValid()) {
            client->sendTextMessage(json);
        }
    }
}

