#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTcpSocket>
#include "rest/api_server.h"
#include "rest/api_router.h"

class TestWebUI : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testStaticFileServing();
    void testRootPathServesIndex();
    void testDirectoryTraversalViaHttp();
    void testContentTypeHtml();
    void testContentTypeCss();
    void testContentTypeJs();
    void testContentTypeJson();
    void testContentTypePng();
    void testContentTypeSvg();
    void testFileNotFound();
    void testUnknownExtension();
    void testMissingWebuiDir();

private:
    std::unique_ptr<rats::rest::ApiServer> server;
    QString tempDirPath;
    int port = 0;

    QByteArray sendRequest(const QString& path);
    QByteArray getHeader(const QByteArray& response, const QByteArray& header);
    QByteArray getBody(const QByteArray& response);
    int getStatusCode(const QByteArray& response);
};

void TestWebUI::initTestCase()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    tempDirPath = tempDir.path();

    QDir().mkpath(tempDirPath + "/webui/css");
    QDir().mkpath(tempDirPath + "/webui/js");
    QDir().mkpath(tempDirPath + "/webui/images");

    {
        QFile f(tempDirPath + "/webui/index.html");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("<html><body>Test</body></html>");
        f.close();
    }
    {
        QFile f(tempDirPath + "/webui/css/style.css");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("body { color: red; }");
        f.close();
    }
    {
        QFile f(tempDirPath + "/webui/js/app.js");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("console.log('hello');");
        f.close();
    }
    {
        QFile f(tempDirPath + "/webui/config.json");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"key\": \"value\"}");
        f.close();
    }
    {
        QFile f(tempDirPath + "/webui/images/icon.png");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("\x89PNG");
        f.close();
    }
    {
        QFile f(tempDirPath + "/webui/logo.svg");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("<svg></svg>");
        f.close();
    }
    {
        QFile f(tempDirPath + "/webui/readme.txt");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("hello world");
        f.close();
    }

    server = std::make_unique<rats::rest::ApiServer>(nullptr);
    server->setWebuiDir(tempDirPath + "/webui");
    QVERIFY(server->start(0));
    port = server->httpPort();
    QVERIFY(port > 0);
}

void TestWebUI::cleanupTestCase()
{
    if (server) server->stop();
    server.reset();
}

QByteArray TestWebUI::sendRequest(const QString& path)
{
    QTcpSocket socket;
    socket.connectToHost("127.0.0.1", port);
    if (!socket.waitForConnected(5000)) return QByteArray();

    QByteArray request = "GET " + path.toUtf8() + " HTTP/1.1\r\n"
                        "Host: localhost:" + QByteArray::number(port) + "\r\n"
                        "Connection: close\r\n"
                        "\r\n";
    socket.write(request);

    if (!socket.waitForReadyRead(5000)) return QByteArray();
    QByteArray response;
    while (socket.waitForReadyRead(1000)) {
        response.append(socket.readAll());
    }
    response.append(socket.readAll());
    return response;
}

QByteArray TestWebUI::getHeader(const QByteArray& response, const QByteArray& header)
{
    int idx = response.indexOf(header + ": ");
    if (idx < 0) return QByteArray();
    int start = idx + header.length() + 2;
    int end = response.indexOf("\r\n", start);
    if (end < 0) return response.mid(start);
    return response.mid(start, end - start);
}

QByteArray TestWebUI::getBody(const QByteArray& response)
{
    int bodyStart = response.indexOf("\r\n\r\n");
    if (bodyStart < 0) return QByteArray();
    return response.mid(bodyStart + 4);
}

int TestWebUI::getStatusCode(const QByteArray& response)
{
    QString line = QString::fromUtf8(response.left(response.indexOf("\r\n")));
    QStringList parts = line.split(' ');
    if (parts.size() >= 2) return parts[1].toInt();
    return 0;
}

void TestWebUI::testStaticFileServing()
{
    QByteArray response = sendRequest("/css/style.css");
    QCOMPARE(getStatusCode(response), 200);
    QByteArray body = getBody(response);
    QCOMPARE(body, QByteArray("body { color: red; }"));
    QCOMPARE(getHeader(response, "Content-Type"), QByteArray("text/css"));
}

void TestWebUI::testRootPathServesIndex()
{
    QByteArray response = sendRequest("/");
    QCOMPARE(getStatusCode(response), 200);
    QByteArray body = getBody(response);
    QVERIFY(body.contains("<html><body>Test</body></html>"));
    QCOMPARE(getHeader(response, "Content-Type"), QByteArray("text/html"));
}

void TestWebUI::testDirectoryTraversalViaHttp()
{
    QStringList paths = {
        "/../../../etc/passwd",
        "/css/../../etc/passwd",
        "/js/../../../etc/shadow",
        "/../config.json"
    };

    for (const QString& path : paths) {
        QByteArray response = sendRequest(path);
        int status = getStatusCode(response);
        QVERIFY(status == 404 || status == 403);
        QByteArray body = getBody(response);
        QVERIFY(!body.contains("root:"));
    }
}

void TestWebUI::testContentTypeHtml()
{
    QByteArray response = sendRequest("/index.html");
    QCOMPARE(getStatusCode(response), 200);
    QCOMPARE(getHeader(response, "Content-Type"), QByteArray("text/html"));
}

void TestWebUI::testContentTypeCss()
{
    QByteArray response = sendRequest("/css/style.css");
    QCOMPARE(getStatusCode(response), 200);
    QCOMPARE(getHeader(response, "Content-Type"), QByteArray("text/css"));
}

void TestWebUI::testContentTypeJs()
{
    QByteArray response = sendRequest("/js/app.js");
    QCOMPARE(getStatusCode(response), 200);
    QCOMPARE(getHeader(response, "Content-Type"), QByteArray("application/javascript"));
}

void TestWebUI::testContentTypeJson()
{
    QByteArray response = server->handleStaticFile("/config.json");
    QCOMPARE(getStatusCode(response), 200);
    QCOMPARE(getHeader(response, "Content-Type"), QByteArray("application/json"));
}

void TestWebUI::testContentTypePng()
{
    QByteArray response = sendRequest("/images/icon.png");
    QCOMPARE(getStatusCode(response), 200);
    QCOMPARE(getHeader(response, "Content-Type"), QByteArray("image/png"));
}

void TestWebUI::testContentTypeSvg()
{
    QByteArray response = sendRequest("/logo.svg");
    QCOMPARE(getStatusCode(response), 200);
    QCOMPARE(getHeader(response, "Content-Type"), QByteArray("image/svg+xml"));
}

void TestWebUI::testFileNotFound()
{
    QByteArray response = sendRequest("/nonexistent.html");
    QCOMPARE(getStatusCode(response), 404);
}

void TestWebUI::testUnknownExtension()
{
    QByteArray response = sendRequest("/readme.txt");
    QCOMPARE(getStatusCode(response), 404);
}

void TestWebUI::testMissingWebuiDir()
{
    QByteArray response = server->handleStaticFile("/anything.html");
    QCOMPARE(getStatusCode(response), 404);
}

QTEST_MAIN(TestWebUI)
#include "test_webui.moc"
