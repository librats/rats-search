#ifndef APISERVER_H
#define APISERVER_H

#include <QObject>
#include <QTcpServer>
#include <QWebSocketServer>
#include <memory>

class RatsAPI;

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

