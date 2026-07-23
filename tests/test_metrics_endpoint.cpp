#include <QtTest>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "rest/api_server.h"
#include "rest/api_router.h"

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

    std::unique_ptr<rats::rest::ApiServer> server;
    int port = 0;
};

void TestMetricsEndpoint::initTestCase()
{
    server = std::make_unique<rats::rest::ApiServer>(nullptr);
    QVERIFY(server->start(0));
    port = server->httpPort();
    QVERIFY(port > 0);
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
        return QByteArray();
    }

    QByteArray request = "GET " + path.toUtf8() + " HTTP/1.1\r\n"
                        "Host: localhost:" + QByteArray::number(port) + "\r\n"
                        "Connection: close\r\n"
                        "\r\n";

    socket.write(request);

    if (!socket.waitForReadyRead(5000)) {
        return QByteArray();
    }

    QByteArray response;
    while (socket.waitForReadyRead(1000)) {
        response.append(socket.readAll());
    }
    response.append(socket.readAll());

    int bodyStart = response.indexOf("\r\n\r\n");
    if (bodyStart >= 0) {
        return response.mid(bodyStart + 4);
    }
    return response;
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
}

void TestMetricsEndpoint::testMetricsEndpoint()
{
    QByteArray body = sendRequest("/metrics");
    QVERIFY(!body.isEmpty());

    QString bodyStr = QString::fromUtf8(body);
    QVERIFY(bodyStr.contains("# HELP"));
    QVERIFY(bodyStr.contains("# TYPE"));
}

void TestMetricsEndpoint::testMetricsContainsRequiredMetrics()
{
    QByteArray body = sendRequest("/metrics");
    QVERIFY(!body.isEmpty());

    QString bodyStr = QString::fromUtf8(body);

    // Server metrics
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
}

QTEST_MAIN(TestMetricsEndpoint)
#include "test_metrics_endpoint.moc"
