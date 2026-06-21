#include <QtTest>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "api/apiserver.h"
#include "api/ratsapi.h"
#include "torrentdatabase.h"
#include "p2pnetwork.h"

class TestMetricsEndpoint : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testHealthzEndpoint();
    void testReadyzEndpoint();
    void testMetricsEndpoint();
    void testMetricsContainsRequiredMetrics();

private:
    QByteArray sendRequest(const QString& path);
    bool waitForConnection(QTcpSocket& socket, int timeoutMs = 5000);
    
    std::unique_ptr<RatsAPI> api;
    std::unique_ptr<ApiServer> server;
    int port = 0;
};

void TestMetricsEndpoint::initTestCase()
{
    // Create a temporary data directory
    QString tempDir = QDir::tempPath() + "/rats-test-metrics-" + 
                      QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(tempDir);
    
    // Create API and server
    api = std::make_unique<RatsAPI>();
    server = std::make_unique<ApiServer>(api.get());
    
    // Start on a random available port
    QVERIFY(server->start(0));  // 0 = auto-select port
    port = server->httpPort();
    QVERIFY(port > 0);
    
    qDebug() << "Test server started on port" << port;
}

void TestMetricsEndpoint::cleanupTestCase()
{
    if (server) {
        server->stop();
    }
}

QByteArray TestMetricsEndpoint::sendRequest(const QString& path)
{
    QTcpSocket socket;
    socket.connectToHost("127.0.0.1", port);
    
    if (!socket.waitForConnected(5000)) {
        qDebug() << "Connection failed:" << socket.errorString();
        return QByteArray();
    }
    
    QByteArray request = "GET " + path.toUtf8() + " HTTP/1.1\r\n"
                        "Host: localhost:" + QByteArray::number(port) + "\r\n"
                        "Connection: close\r\n"
                        "\r\n";
    
    socket.write(request);
    
    if (!socket.waitForReadyRead(5000)) {
        qDebug() << "No response received";
        return QByteArray();
    }
    
    QByteArray response;
    while (socket.waitForReadyRead(1000)) {
        response.append(socket.readAll());
    }
    response.append(socket.readAll());
    
    // Extract body (after the headers)
    int bodyStart = response.indexOf("\r\n\r\n");
    if (bodyStart >= 0) {
        return response.mid(bodyStart + 4);
    }
    return response;
}

bool TestMetricsEndpoint::waitForConnection(QTcpSocket& socket, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (socket.state() == QAbstractSocket::ConnectingState && timer.elapsed() < timeoutMs) {
        QApplication::processEvents();
        QThread::msleep(10);
    }
    return socket.state() == QAbstractSocket::ConnectedState;
}

void TestMetricsEndpoint::testHealthzEndpoint()
{
    QByteArray body = sendRequest("/healthz");
    QVERIFY(!body.isEmpty());
    
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QVERIFY(!doc.isNull());
    QVERIFY(doc.isObject());
    
    QJsonObject obj = doc.object();
    QCOMPARE(obj["status"].toString(), QString("ok"));
    QVERIFY(obj["timestamp"].toString().isEmpty() == false);
    
    qDebug() << "Healthz response:" << body;
}

void TestMetricsEndpoint::testReadyzEndpoint()
{
    QByteArray body = sendRequest("/readyz");
    QVERIFY(!body.isEmpty());
    
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QVERIFY(!doc.isNull());
    QVERIFY(doc.isObject());
    
    QJsonObject obj = doc.object();
    QVERIFY(obj.contains("status"));
    QVERIFY(obj.contains("api"));
    QVERIFY(obj.contains("http"));
    QVERIFY(obj.contains("websocket"));
    QVERIFY(obj.contains("timestamp"));
    
    qDebug() << "Readyz response:" << body;
}

void TestMetricsEndpoint::testMetricsEndpoint()
{
    QByteArray body = sendRequest("/metrics");
    QVERIFY(!body.isEmpty());
    
    // Metrics should be in Prometheus text format
    QString bodyStr = QString::fromUtf8(body);
    QVERIFY(bodyStr.contains("# HELP"));
    QVERIFY(bodyStr.contains("# TYPE"));
    
    qDebug() << "Metrics response (first 500 chars):" << bodyStr.left(500);
}

void TestMetricsEndpoint::testMetricsContainsRequiredMetrics()
{
    QByteArray body = sendRequest("/metrics");
    QVERIFY(!body.isEmpty());
    
    QString bodyStr = QString::fromUtf8(body);
    
    // Check for required server metrics
    QVERIFY(bodyStr.contains("rats_server_uptime_seconds"));
    QVERIFY(bodyStr.contains("rats_websocket_connections"));
    QVERIFY(bodyStr.contains("rats_http_server_running"));
    QVERIFY(bodyStr.contains("rats_ws_server_running"));
    QVERIFY(bodyStr.contains("rats_http_port"));
    QVERIFY(bodyStr.contains("rats_ws_port"));
    QVERIFY(bodyStr.contains("rats_http_requests_total"));
    QVERIFY(bodyStr.contains("rats_http_requests_success_total"));
    QVERIFY(bodyStr.contains("rats_http_requests_error_total"));
    QVERIFY(bodyStr.contains("rats_ws_messages_total"));
    
    // Check for P2P metrics
    QVERIFY(bodyStr.contains("rats_p2p_peer_count"));
    QVERIFY(bodyStr.contains("rats_p2p_dht_node_count"));
    QVERIFY(bodyStr.contains("rats_p2p_dht_running"));
    QVERIFY(bodyStr.contains("rats_p2p_bittorrent_enabled"));
    QVERIFY(bodyStr.contains("rats_p2p_running"));
    
    // Check for database metrics
    QVERIFY(bodyStr.contains("rats_db_torrents_total"));
    QVERIFY(bodyStr.contains("rats_db_files_total"));
    QVERIFY(bodyStr.contains("rats_db_size_bytes"));
    QVERIFY(bodyStr.contains("rats_db_ready"));
    
    // Check for spider metrics (may be null if not initialized)
    // QVERIFY(bodyStr.contains("rats_spider_running"));
    
    // Check for download metrics
    QVERIFY(bodyStr.contains("rats_downloads_active"));
    QVERIFY(bodyStr.contains("rats_downloads_paused"));
    QVERIFY(bodyStr.contains("rats_downloads_total_bytes_downloaded"));
    QVERIFY(bodyStr.contains("rats_downloads_speed_bytes_per_second"));
    QVERIFY(bodyStr.contains("rats_downloads_total"));
    
    qDebug() << "All required metrics found!";
}

QTEST_MAIN(TestMetricsEndpoint)
#include "test_metrics_endpoint.moc"
