# Add API Endpoint Skill

## Description
Add a new REST API endpoint to the Rats Search API server.

## Prerequisites
- Understanding of the existing API structure
- Qt6 WebSocket knowledge

## Architecture

The API layer consists of:
- `src/api/ratsapi.h/cpp` - Core API facade (business logic)
- `src/api/apiserver.h/cpp` - REST/WebSocket server (HTTP handling)

## Steps

### 1. Add Method to RatsAPI
Add the new endpoint method to `src/api/ratsapi.h`:

```cpp
class RatsAPI : public QObject
{
    Q_OBJECT

public:
    // ... existing methods ...
    
    // New endpoint
    void newEndpoint(const QString& param, std::function<void(const ApiResponse&)> callback);
    
signals:
    // ... existing signals ...
};
```

Implement in `src/api/ratsapi.cpp`:

```cpp
void RatsAPI::newEndpoint(const QString& param, std::function<void(const ApiResponse&)> callback)
{
    // Business logic here
    ApiResponse response;
    response.success = true;
    response.data = QJsonObject{{"result", "value"}};
    callback(response);
}
```

### 2. Wire in ApiServer
Add REST endpoint in `src/api/apiserver.cpp`:

```cpp
void ApiServer::setupRoutes()
{
    // ... existing routes ...
    
    // New REST endpoint
    m_server->route("/api/new-endpoint", QHttpServerRequest::Method::GET,
        [this](const QHttpServerRequest &request) {
            QString param = request.query().queryItemValue("param");
            
            QPromise<QHttpServerResponse> promise;
            m_api->newEndpoint(param, [promise](const ApiResponse& response) mutable {
                QJsonObject result;
                result["success"] = response.success;
                result["data"] = response.data;
                promise.start(QJsonDocument(result).toJson());
            });
            
            return promise;
        });
}
```

### 3. Add WebSocket Handler (if needed)
For real-time updates, add WebSocket handling:

```cpp
void ApiServer::handleWebSocketMessage(const QJsonObject& message)
{
    QString action = message["action"].toString();
    
    if (action == "new-action") {
        // Handle WebSocket message
        m_api->newEndpoint(message["param"].toString(), 
            [this](const ApiResponse& response) {
                QJsonObject wsMessage;
                wsMessage["action"] = "new-action-result";
                wsMessage["data"] = response.data;
                broadcastWebSocket(QJsonDocument(wsMessage).toJson());
            });
    }
}
```

### 4. Update API Documentation
Update `docs/API.md` with the new endpoint documentation.

## Existing Endpoints Reference

### Search Endpoints
- `GET /api/search` - Search torrents
- `GET /api/torrent/:hash` - Get torrent details
- `GET /api/files/:hash` - Get torrent files

### Statistics Endpoints
- `GET /api/stats` - Get database statistics
- `GET /api/top` - Get top torrents
- `GET /api/recent` - Get recent torrents

### P2P Endpoints
- `GET /api/peers` - Get connected peers
- `GET /api/dht` - Get DHT node count

## Response Format

```json
{
  "success": true,
  "data": {
    // Endpoint-specific data
  },
  "error": "Error message (if success=false)"
}
```

## Testing

Test the new endpoint:
```bash
# REST API test
curl http://localhost:8095/api/new-endpoint?param=value

# WebSocket test (using wscat)
wscat -c ws://localhost:8095
> {"action": "new-action", "param": "value"}
```

## References
- Existing API: `src/api/ratsapi.cpp`
- Server implementation: `src/api/apiserver.cpp`
- API documentation: `docs/API.md`
