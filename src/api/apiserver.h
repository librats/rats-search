#ifndef APISERVER_H
#define APISERVER_H

#include <QObject>
#include <QTcpServer>
#include <QWebSocketServer>
#include <memory>

class RatsAPI;
class P2PNetwork;
class TorrentSpider;
class TorrentDatabase;
class TorrentClient;

/**
 * @brief ApiServer - HTTP REST + WebSocket server for RatsAPI
 * 
 * Provides external access to RatsAPI through:
 * - REST API: GET /api/{method}?{params}
 * - WebSocket: JSON-RPC style messages
 * 
 * REST API Format:
 *   GET /api/search.torrents?text=ubuntu&limit=10
 *   Response: { "success": true, "data": [...], "requestId": "..." }
 * 
 * WebSocket Format:
 *   Request:  { "method": "search.torrents", "params": {...}, "id": "..." }
 *   Response: { "success": true, "data": [...], "requestId": "..." }
 *   Event:    { "event": "downloadProgress", "data": {...} }
 * 
 * Features:
 * - Automatic routing to RatsAPI methods
 * - JSON serialization/deserialization
 * - Request ID tracking
 * - Event broadcasting via WebSocket
 * - CORS support for browser clients
 */
class ApiServer : public QObject
{
    Q_OBJECT

public:
    explicit ApiServer(RatsAPI* api, QObject *parent = nullptr);
    ~ApiServer();
    
    /**
     * @brief Start the server
     * @param httpPort Port for HTTP REST API (0 to disable)
     * @param wsPort Port for WebSocket (0 to disable, -1 to share with HTTP)
     * @return true if started successfully
     */
    bool start(int httpPort = 8095, int wsPort = -1);
    
    /**
     * @brief Stop the server
     */
    void stop();
    
    /**
     * @brief Check if server is running
     */
    bool isRunning() const;
    
    /**
     * @brief Get HTTP port
     */
    int httpPort() const;
    
    /**
     * @brief Get WebSocket port
     */
    int wsPort() const;
    
    /**
     * @brief Get number of connected WebSocket clients
     */
    int clientCount() const;
    
    /**
     * @brief Broadcast event to all WebSocket clients
     */
    void broadcastEvent(const QString& event, const QJsonValue& data);
    
    // =========================================================================
    // Health & Metrics (for Kubernetes probes and Prometheus)
    // =========================================================================
    
    /**
     * @brief Liveness probe - returns 200 if server is running
     * Use for kubernetes livenessProbe
     */
    QByteArray handleHealthz() const;
    
    /**
     * @brief Readiness probe - returns 200 if API is ready to serve requests
     * Checks: API initialized, database ready, P2P running
     * Use for kubernetes readinessProbe
     */
    QByteArray handleReadyz() const;
    
    /**
     * @brief Prometheus metrics endpoint
     * Returns metrics in Prometheus text exposition format
     */
    QByteArray handleMetrics() const;

    /**
     * @brief Serve static file from webui directory
     * @param path Request path (e.g., "/css/style.css")
     * @return HTTP response with file content
     */
    QByteArray handleStaticFile(const QString& path) const;

signals:
    void started();
    void stopped();
    void clientConnected(const QString& address);
    void clientDisconnected(const QString& address);
    void error(const QString& message);

private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif // APISERVER_H

