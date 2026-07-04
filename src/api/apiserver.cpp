#include "apiserver.h"
#include "ratsapi.h"
#include "../p2pnetwork.h"
#include "../torrentspider.h"
#include "../torrentdatabase.h"
#include "../torrentclient.h"
#include "../manticoremanager.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <limits>

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
    qint64 startTimeMs = 0;
    qint64 httpRequestsTotal = 0;
    qint64 httpRequestsSuccess = 0;
    qint64 httpRequestsError = 0;
    qint64 wsMessagesTotal = 0;

    // Per-method request counters
    QMap<QString, qint64> requestsByMethod;
    // Per-status code counters
    QMap<int, qint64> requestsByStatus;
    // Request duration tracking (requestId -> start timestamp ms)
    QMap<QString, qint64> requestStartTimes;
    // Request duration histogram buckets (seconds)
    // Buckets: 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10, +Inf
    static constexpr int DURATION_BUCKETS_COUNT = 12;
    static constexpr double DURATION_BUCKETS[DURATION_BUCKETS_COUNT] = {
        0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0, std::numeric_limits<double>::infinity()
    };
    qint64 durationBucketCounts[DURATION_BUCKETS_COUNT] = {};
    double durationSumMs = 0;
    qint64 durationCount = 0;
    // Search-specific counters
    qint64 searchQueriesTotal = 0;
    qint64 searchDurationSumMs = 0;
    qint64 searchDurationBuckets[DURATION_BUCKETS_COUNT] = {};
    // DB index size cache (updated periodically)
    qint64 dbIndexSizeBytes = 0;
    qint64 dbIndexSizeLastUpdate = 0;
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
                    
                    // Health check endpoints (for Kubernetes)
                    if (req.path == "/healthz") {
                        socket->write(handleHealthz());
                        socket->disconnectFromHost();
                        return;
                    }
                    
                    if (req.path == "/readyz") {
                        socket->write(handleReadyz());
                        socket->disconnectFromHost();
                        return;
                    }
                    
                    // Prometheus metrics endpoint
                    if (req.path == "/metrics") {
                        socket->write(handleMetrics());
                        socket->disconnectFromHost();
                        return;
                    }
                    
                    // Serve static files from webui directory
                    if (req.path == "/" || 
                        req.path.startsWith("/css/") || 
                        req.path.startsWith("/js/") ||
                        req.path.endsWith(".html") ||
                        req.path.endsWith(".css") ||
                        req.path.endsWith(".js") ||
                        req.path.endsWith(".ico") ||
                        req.path.endsWith(".png") ||
                        req.path.endsWith(".jpg")) {
                        socket->write(handleStaticFile(req.path));
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
                        
                        d->httpRequestsTotal++;
                        d->requestsByMethod[req.method]++;
                        d->requestStartTimes[requestId] = QDateTime::currentMSecsSinceEpoch();
                        
                        d->api->call(method, params, [socket, requestId, method, this](const ApiResponse& resp) {
                            if (!socket->isOpen()) {
                                d->requestStartTimes.remove(requestId);
                                return;
                            }
                            
                            if (resp.success) {
                                d->httpRequestsSuccess++;
                            } else {
                                d->httpRequestsError++;
                            }
                            int statusCode = resp.success ? 200 : 400;
                            d->requestsByStatus[statusCode]++;

                            // Track request duration
                            qint64 now = QDateTime::currentMSecsSinceEpoch();
                            auto it = d->requestStartTimes.find(requestId);
                            if (it != d->requestStartTimes.end()) {
                                double durationMs = now - it.value();
                                double durationSec = durationMs / 1000.0;
                                d->durationSumMs += durationMs;
                                d->durationCount++;
                                for (int i = 0; i < Private::DURATION_BUCKETS_COUNT; i++) {
                                    if (durationSec <= Private::DURATION_BUCKETS[i]) {
                                        d->durationBucketCounts[i]++;
                                    }
                                }
                                // Track search-specific metrics
                                if (method.startsWith("search.")) {
                                    d->searchQueriesTotal++;
                                    d->searchDurationSumMs += durationMs;
                                    for (int i = 0; i < Private::DURATION_BUCKETS_COUNT; i++) {
                                        if (durationSec <= Private::DURATION_BUCKETS[i]) {
                                            d->searchDurationBuckets[i]++;
                                        }
                                    }
                                }
                                d->requestStartTimes.remove(requestId);
                            }
                            
                            QJsonDocument doc(resp.toJson());
                            socket->write(buildHttpResponse(statusCode, 
                                                           resp.success ? "OK" : "Bad Request",
                                                           doc.toJson()));
                            socket->disconnectFromHost();
                        }, requestId);
                        
                        return;
                    }
                    
                    // 404 for unknown paths
                    d->requestsByStatus[404]++;
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
                    d->wsMessagesTotal++;
                    
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
    d->startTimeMs = QDateTime::currentMSecsSinceEpoch();
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

// ============================================================================
// Health & Metrics Implementation
// ============================================================================

QByteArray ApiServer::handleHealthz() const
{
    QJsonObject status;
    status["status"] = "ok";
    status["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    
    QJsonDocument doc(status);
    return buildHttpResponse(200, "OK", doc.toJson(), "application/json");
}

QByteArray ApiServer::handleReadyz() const
{
    QJsonObject status;
    bool ready = true;
    
    // Check if API is initialized and ready
    if (d->api) {
        status["api"] = d->api->isReady() ? "ready" : "not_ready";
        if (!d->api->isReady()) ready = false;
    } else {
        status["api"] = "not_initialized";
        ready = false;
    }
    
    // Check HTTP server
    status["http"] = d->httpServer && d->httpServer->isListening() ? "listening" : "not_listening";
    if (!d->httpServer || !d->httpServer->isListening()) ready = false;
    
    // Check WebSocket server
    status["websocket"] = d->wsServer && d->wsServer->isListening() ? "listening" : "not_listening";
    
    status["status"] = ready ? "ready" : "not_ready";
    status["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    
    QJsonDocument doc(status);
    int statusCode = ready ? 200 : 503;
    QString statusText = ready ? "OK" : "Service Unavailable";
    return buildHttpResponse(statusCode, statusText, doc.toJson(), "application/json");
}

QByteArray ApiServer::handleMetrics() const
{
    QStringList metrics;
    
    // Timestamp for all metrics
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    
    // Server uptime
    qint64 uptimeMs = d->startTimeMs > 0 ? (timestamp - d->startTimeMs) : 0;
    metrics << "# HELP rats_server_uptime_seconds Server uptime in seconds";
    metrics << "# TYPE rats_server_uptime_seconds gauge";
    metrics << QString("rats_server_uptime_seconds %1 %2").arg(uptimeMs / 1000).arg(timestamp);

    // Startup timestamp (for Grafana annotations)
    if (d->startTimeMs > 0) {
        qint64 startTimestampMs = d->startTimeMs;
        metrics << "# HELP rats_start_time_seconds Timestamp when the server started (Unix epoch in milliseconds)";
        metrics << "# TYPE rats_start_time_seconds gauge";
        metrics << QString("rats_start_time_seconds %1 %2").arg(startTimestampMs).arg(startTimestampMs);
    }
    
    // WebSocket connections
    metrics << "# HELP rats_websocket_connections Current number of WebSocket connections";
    metrics << "# TYPE rats_websocket_connections gauge";
    metrics << QString("rats_websocket_connections %1 %2").arg(d->wsClients.size()).arg(timestamp);
    
    // HTTP server status
    metrics << "# HELP rats_http_server_running Whether HTTP server is running (1=yes, 0=no)";
    metrics << "# TYPE rats_http_server_running gauge";
    metrics << QString("rats_http_server_running %1 %2").arg(d->running_ ? 1 : 0).arg(timestamp);
    
    // WebSocket server status
    metrics << "# HELP rats_ws_server_running Whether WebSocket server is running (1=yes, 0=no)";
    metrics << "# TYPE rats_ws_server_running gauge";
    metrics << QString("rats_ws_server_running %1 %2").arg(d->wsServer && d->wsServer->isListening() ? 1 : 0).arg(timestamp);
    
    // Port info
    metrics << "# HELP rats_http_port HTTP server port";
    metrics << "# TYPE rats_http_port gauge";
    metrics << QString("rats_http_port %1 %2").arg(d->httpPort_).arg(timestamp);
    
    metrics << "# HELP rats_ws_port WebSocket server port";
    metrics << "# TYPE rats_ws_port gauge";
    metrics << QString("rats_ws_port %1 %2").arg(d->wsPort_).arg(timestamp);
    
    // Request counters
    metrics << "# HELP rats_http_requests_total Total HTTP requests";
    metrics << "# TYPE rats_http_requests_total counter";
    metrics << QString("rats_http_requests_total %1 %2").arg(d->httpRequestsTotal).arg(timestamp);
    
    metrics << "# HELP rats_http_requests_success_total Successful HTTP requests";
    metrics << "# TYPE rats_http_requests_success_total counter";
    metrics << QString("rats_http_requests_success_total %1 %2").arg(d->httpRequestsSuccess).arg(timestamp);
    
    metrics << "# HELP rats_http_requests_error_total Failed HTTP requests";
    metrics << "# TYPE rats_http_requests_error_total counter";
    metrics << QString("rats_http_requests_error_total %1 %2").arg(d->httpRequestsError).arg(timestamp);
    
    metrics << "# HELP rats_ws_messages_total Total WebSocket messages received";
    metrics << "# TYPE rats_ws_messages_total counter";
    metrics << QString("rats_ws_messages_total %1 %2").arg(d->wsMessagesTotal).arg(timestamp);
    
    // Application info
    if (d->api && d->api->isReady()) {
        metrics << "# HELP rats_info Application info";
        metrics << "# TYPE rats_info gauge";
        metrics << QString("rats_info{version=\"2.0.0\"} 1 %1").arg(timestamp);
    }
    
    // =========================================================================
    // P2P Network Metrics
    // =========================================================================
    if (d->api) {
        P2PNetwork* p2p = d->api->getP2PNetwork();
        if (p2p) {
            metrics << "# HELP rats_p2p_peer_count Current number of connected P2P peers";
            metrics << "# TYPE rats_p2p_peer_count gauge";
            metrics << QString("rats_p2p_peer_count %1 %2").arg(p2p->getPeerCount()).arg(timestamp);
            
            metrics << "# HELP rats_p2p_dht_node_count Number of DHT nodes in routing table";
            metrics << "# TYPE rats_p2p_dht_node_count gauge";
            metrics << QString("rats_p2p_dht_node_count %1 %2").arg(p2p->getDhtNodeCount()).arg(timestamp);
            
            metrics << "# HELP rats_p2p_dht_running Whether DHT is running (1=yes, 0=no)";
            metrics << "# TYPE rats_p2p_dht_running gauge";
            metrics << QString("rats_p2p_dht_running %1 %2").arg(p2p->isDhtRunning() ? 1 : 0).arg(timestamp);
            
            metrics << "# HELP rats_p2p_bittorrent_enabled Whether BitTorrent is enabled (1=yes, 0=no)";
            metrics << "# TYPE rats_p2p_bittorrent_enabled gauge";
            metrics << QString("rats_p2p_bittorrent_enabled %1 %2").arg(p2p->isBitTorrentEnabled() ? 1 : 0).arg(timestamp);
            
            metrics << "# HELP rats_p2p_running Whether P2P network is running (1=yes, 0=no)";
            metrics << "# TYPE rats_p2p_running gauge";
            metrics << QString("rats_p2p_running %1 %2").arg(p2p->isRunning() ? 1 : 0).arg(timestamp);
        }
    }
    
    // =========================================================================
    // Torrent Database Metrics
    // =========================================================================
    if (d->api) {
        TorrentDatabase* db = d->api->getDatabase();
        if (db && db->isReady()) {
            auto stats = db->getStatistics();
            
            metrics << "# HELP rats_db_torrents_total Total torrents in database";
            metrics << "# TYPE rats_db_torrents_total gauge";
            metrics << QString("rats_db_torrents_total %1 %2").arg(stats.totalTorrents).arg(timestamp);
            
            metrics << "# HELP rats_db_files_total Total files in database";
            metrics << "# TYPE rats_db_files_total gauge";
            metrics << QString("rats_db_files_total %1 %2").arg(stats.totalFiles).arg(timestamp);
            
            metrics << "# HELP rats_db_total_content_size_bytes Total content size of all torrents in bytes (sum of torrent sizes)";
            metrics << "# TYPE rats_db_total_content_size_bytes gauge";
            metrics << QString("rats_db_total_content_size_bytes %1 %2").arg(stats.totalSize).arg(timestamp);
            
            metrics << "# HELP rats_db_ready Whether database is ready (1=yes, 0=no)";
            metrics << "# TYPE rats_db_ready gauge";
            metrics << QString("rats_db_ready 1 %1").arg(timestamp);
        } else {
            metrics << "# HELP rats_db_ready Whether database is ready (1=yes, 0=no)";
            metrics << "# TYPE rats_db_ready gauge";
            metrics << QString("rats_db_ready 0 %1").arg(timestamp);
        }
    }
    
    // =========================================================================
    // Torrent Spider (DHT Crawler) Metrics
    // =========================================================================
    if (d->api) {
        TorrentSpider* spider = d->api->getSpider();
        if (spider) {
            metrics << "# HELP rats_spider_running Whether DHT spider is running (1=yes, 0=no)";
            metrics << "# TYPE rats_spider_running gauge";
            metrics << QString("rats_spider_running %1 %2").arg(spider->isRunning() ? 1 : 0).arg(timestamp);
            
            metrics << "# HELP rats_spider_indexed_total Total torrents indexed by spider";
            metrics << "# TYPE rats_spider_indexed_total gauge";
            metrics << QString("rats_spider_indexed_total %1 %2").arg(spider->getIndexedCount()).arg(timestamp);
            
            metrics << "# HELP rats_spider_pending_fetches Current pending metadata fetches";
            metrics << "# TYPE rats_spider_pending_fetches gauge";
            metrics << QString("rats_spider_pending_fetches %1 %2").arg(spider->getPendingCount()).arg(timestamp);
            
            metrics << "# HELP rats_spider_dht_nodes DHT routing table size";
            metrics << "# TYPE rats_spider_dht_nodes gauge";
            metrics << QString("rats_spider_dht_nodes %1 %2").arg(spider->getDhtNodeCount()).arg(timestamp);
            
            metrics << "# HELP rats_spider_pool_size Spider pool size";
            metrics << "# TYPE rats_spider_pool_size gauge";
            metrics << QString("rats_spider_pool_size %1 %2").arg(spider->getSpiderPoolSize()).arg(timestamp);
            
            metrics << "# HELP rats_spider_visited_nodes Total visited DHT nodes";
            metrics << "# TYPE rats_spider_visited_nodes gauge";
            metrics << QString("rats_spider_visited_nodes %1 %2").arg(spider->getSpiderVisitedCount()).arg(timestamp);
            
            metrics << "# HELP rats_spider_fetch_success_total Total successful metadata fetches";
            metrics << "# TYPE rats_spider_fetch_success_total counter";
            metrics << QString("rats_spider_fetch_success_total %1 %2").arg(spider->getFetchSuccessCount()).arg(timestamp);
            
            metrics << "# HELP rats_spider_fetch_errors_total Total failed metadata fetches";
            metrics << "# TYPE rats_spider_fetch_errors_total counter";
            metrics << QString("rats_spider_fetch_errors_total %1 %2").arg(spider->getFetchErrorCount()).arg(timestamp);
        }
    }
    
    // =========================================================================
    // Torrent Client (Downloads) Metrics
    // =========================================================================
    if (d->api) {
        TorrentClient* client = d->api->getTorrentClient();
        if (client && client->isReady()) {
            auto torrents = client->getAllTorrents();
            int activeCount = 0;
            int pausedCount = 0;
            qint64 totalDownloaded = 0;
            double totalSpeed = 0.0;
            
            for (const auto& t : torrents) {
                if (!t.completed) {
                    activeCount++;
                    if (t.paused) pausedCount++;
                    totalDownloaded += t.downloadedBytes;
                    totalSpeed += t.downloadSpeed;
                }
            }
            
            metrics << "# HELP rats_downloads_active Number of active downloads";
            metrics << "# TYPE rats_downloads_active gauge";
            metrics << QString("rats_downloads_active %1 %2").arg(activeCount).arg(timestamp);
            
            metrics << "# HELP rats_downloads_paused Number of paused downloads";
            metrics << "# TYPE rats_downloads_paused gauge";
            metrics << QString("rats_downloads_paused %1 %2").arg(pausedCount).arg(timestamp);
            
            metrics << "# HELP rats_downloads_total_bytes_downloaded Total bytes downloaded across all torrents";
            metrics << "# TYPE rats_downloads_total_bytes_downloaded counter";
            metrics << QString("rats_downloads_total_bytes_downloaded %1 %2").arg(totalDownloaded).arg(timestamp);
            
            metrics << "# HELP rats_downloads_speed_bytes_per_second Current total download speed in bytes/second";
            metrics << "# TYPE rats_downloads_speed_bytes_per_second gauge";
            metrics << QString("rats_downloads_speed_bytes_per_second %1 %2").arg(static_cast<qint64>(totalSpeed)).arg(timestamp);
            
            metrics << "# HELP rats_downloads_total Total number of downloads (active + completed)";
            metrics << "# TYPE rats_downloads_total gauge";
            metrics << QString("rats_downloads_total %1 %2").arg(torrents.size()).arg(timestamp);
        }
    }
    
    // =========================================================================
    // Request Method Breakdown
    // =========================================================================
    if (!d->requestsByMethod.isEmpty()) {
        metrics << "# HELP rats_http_requests_by_method_total Total HTTP requests by method";
        metrics << "# TYPE rats_http_requests_by_method_total counter";
        for (auto it = d->requestsByMethod.constBegin(); it != d->requestsByMethod.constEnd(); ++it) {
            metrics << QString("rats_http_requests_by_method_total{method=\"%1\"} %2 %3").arg(it.key()).arg(it.value()).arg(timestamp);
        }
    }
    
    // =========================================================================
    // Request Status Breakdown
    // =========================================================================
    if (!d->requestsByStatus.isEmpty()) {
        metrics << "# HELP rats_http_requests_by_status_total Total HTTP requests by status code";
        metrics << "# TYPE rats_http_requests_by_status_total counter";
        for (auto it = d->requestsByStatus.constBegin(); it != d->requestsByStatus.constEnd(); ++it) {
            metrics << QString("rats_http_requests_by_status_total{status=\"%1\"} %2 %3").arg(it.key()).arg(it.value()).arg(timestamp);
        }
    }
    
    // =========================================================================
    // Request Duration Histogram
    // =========================================================================
    if (d->durationCount > 0) {
        metrics << "# HELP rats_http_request_duration_seconds HTTP request duration in seconds";
        metrics << "# TYPE rats_http_request_duration_seconds histogram";
        qint64 cumulative = 0;
        for (int i = 0; i < Private::DURATION_BUCKETS_COUNT; i++) {
            cumulative += d->durationBucketCounts[i];
            QString le = (i == Private::DURATION_BUCKETS_COUNT - 1) ? "+Inf" 
                        : QString::number(Private::DURATION_BUCKETS[i], 'g', 4);
            metrics << QString("rats_http_request_duration_seconds_bucket{le=\"%1\"} %2 %3").arg(le).arg(cumulative).arg(timestamp);
        }
        metrics << QString("rats_http_request_duration_seconds_sum %1 %2").arg(d->durationSumMs / 1000.0, 0, 'f', 6).arg(timestamp);
        metrics << QString("rats_http_request_duration_seconds_count %1 %2").arg(d->durationCount).arg(timestamp);
    }
    
    // =========================================================================
    // Search Metrics
    // =========================================================================
    if (d->searchQueriesTotal > 0) {
        metrics << "# HELP rats_search_queries_total Total search queries executed";
        metrics << "# TYPE rats_search_queries_total counter";
        metrics << QString("rats_search_queries_total %1 %2").arg(d->searchQueriesTotal).arg(timestamp);
        
        metrics << "# HELP rats_search_duration_seconds Search query duration in seconds";
        metrics << "# TYPE rats_search_duration_seconds histogram";
        qint64 cumulative = 0;
        for (int i = 0; i < Private::DURATION_BUCKETS_COUNT; i++) {
            cumulative += d->searchDurationBuckets[i];
            QString le = (i == Private::DURATION_BUCKETS_COUNT - 1) ? "+Inf" 
                        : QString::number(Private::DURATION_BUCKETS[i], 'g', 4);
            metrics << QString("rats_search_duration_seconds_bucket{le=\"%1\"} %2 %3").arg(le).arg(cumulative).arg(timestamp);
        }
        metrics << QString("rats_search_duration_seconds_sum %1 %2").arg(d->searchDurationSumMs / 1000.0, 0, 'f', 6).arg(timestamp);
        metrics << QString("rats_search_duration_seconds_count %1 %2").arg(d->searchQueriesTotal).arg(timestamp);
    }
    
    // =========================================================================
    // Database Index Size
    // =========================================================================
    if (d->api && d->api->isReady()) {
        // Update index size every 60 seconds
        if (timestamp - d->dbIndexSizeLastUpdate > 60000) {
            TorrentDatabase* db = d->api->getDatabase();
            if (db && db->manager()) {
                QDir dbDir(db->manager()->databasePath());
                if (dbDir.exists()) {
                    qint64 totalSize = 0;
                    for (const auto& entry : dbDir.entryInfoList(QDir::Files)) {
                        totalSize += entry.size();
                    }
                    d->dbIndexSizeBytes = totalSize;
                }
                d->dbIndexSizeLastUpdate = timestamp;
            }
        }
        metrics << "# HELP rats_db_index_size_bytes Total size of database index files on disk";
        metrics << "# TYPE rats_db_index_size_bytes gauge";
        metrics << QString("rats_db_index_size_bytes %1 %2").arg(d->dbIndexSizeBytes).arg(timestamp);
    }
    
    QString body = metrics.join("\n") + "\n";
    return buildHttpResponse(200, "OK", body.toUtf8(), "text/plain; version=0.0.4; charset=utf-8");
}

QByteArray ApiServer::handleStaticFile(const QString& path) const
{
    // Get webui directory from config
    QString webuiDir;
    if (d->api) {
        // Try to get from config manager
        // For now, use default path
        webuiDir = QDir::currentPath() + "/webui";
    }
    
    if (webuiDir.isEmpty()) {
        return buildHttpResponse(404, "Not Found", "{\"error\":\"WebUI directory not configured\"}");
    }
    
    // Map path to file
    QString filePath;
    if (path == "/" || path.isEmpty()) {
        filePath = webuiDir + "/index.html";
    } else {
        filePath = webuiDir + path;
    }
    
    // Security: prevent directory traversal
    if (!filePath.startsWith(webuiDir)) {
        return buildHttpResponse(403, "Forbidden", "{\"error\":\"Access denied\"}");
    }
    
    QFile file(filePath);
    if (!file.exists()) {
        return buildHttpResponse(404, "Not Found", "{\"error\":\"File not found\"}");
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        return buildHttpResponse(500, "Internal Server Error", "{\"error\":\"Cannot read file\"}");
    }
    
    QByteArray content = file.readAll();
    file.close();
    
    // Determine content type
    QString contentType = "text/plain";
    if (filePath.endsWith(".html")) contentType = "text/html";
    else if (filePath.endsWith(".css")) contentType = "text/css";
    else if (filePath.endsWith(".js")) contentType = "application/javascript";
    else if (filePath.endsWith(".json")) contentType = "application/json";
    else if (filePath.endsWith(".png")) contentType = "image/png";
    else if (filePath.endsWith(".jpg") || filePath.endsWith(".jpeg")) contentType = "image/jpeg";
    else if (filePath.endsWith(".svg")) contentType = "image/svg+xml";
    else if (filePath.endsWith(".ico")) contentType = "image/x-icon";
    
    return buildHttpResponse(200, "OK", content, contentType);
}

