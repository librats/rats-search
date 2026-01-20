#include "trackerchecker.h"
#include <QDebug>
#include <QHostInfo>
#include <QDataStream>
#include <QRandomGenerator>
#include <QtEndian>

// ============================================================================
// Constructor / Destructor
// ============================================================================

TrackerChecker::TrackerChecker(QObject *parent)
    : QObject(parent)
    , socket_(nullptr)
    , cleanupTimer_(nullptr)
    , timeoutMs_(15000)  // 15 seconds default timeout
{
}

TrackerChecker::~TrackerChecker()
{
    close();
}

// ============================================================================
// Initialization
// ============================================================================

bool TrackerChecker::initialize(quint16 port)
{
    if (socket_) {
        return true;  // Already initialized
    }
    
    socket_ = new QUdpSocket(this);
    
    if (!socket_->bind(QHostAddress::Any, port)) {
        qWarning() << "TrackerChecker: Failed to bind UDP socket:" << socket_->errorString();
        delete socket_;
        socket_ = nullptr;
        return false;
    }
    
    connect(socket_, &QUdpSocket::readyRead, this, &TrackerChecker::onReadyRead);
    
    // Setup cleanup timer for timed out requests
    cleanupTimer_ = new QTimer(this);
    cleanupTimer_->setInterval(5000);  // Cleanup every 5 seconds
    connect(cleanupTimer_, &QTimer::timeout, this, &TrackerChecker::onCleanupTimer);
    cleanupTimer_->start();
    
    qInfo() << "TrackerChecker initialized on port" << socket_->localPort();
    return true;
}

void TrackerChecker::close()
{
    if (cleanupTimer_) {
        cleanupTimer_->stop();
        delete cleanupTimer_;
        cleanupTimer_ = nullptr;
    }
    
    if (socket_) {
        socket_->close();
        delete socket_;
        socket_ = nullptr;
    }
    
    requests_.clear();
    multiScrapes_.clear();
}

bool TrackerChecker::isReady() const
{
    return socket_ && socket_->state() == QAbstractSocket::BoundState;
}

quint16 TrackerChecker::localPort() const
{
    return socket_ ? socket_->localPort() : 0;
}

// ============================================================================
// Public Methods
// ============================================================================

void TrackerChecker::scrape(const QString& host, quint16 port, const QString& hash,
                            std::function<void(const TrackerResult&)> callback)
{
    if (!isReady()) {
        if (!initialize()) {
            TrackerResult result;
            result.error = "Failed to initialize UDP socket";
            if (callback) callback(result);
            return;
        }
    }
    
    if (hash.length() != 40) {
        TrackerResult result;
        result.error = "Invalid hash length";
        if (callback) callback(result);
        return;
    }
    
    // Generate unique transaction ID
    quint32 transactionId = QRandomGenerator::global()->generate();
    while (requests_.contains(transactionId)) {
        transactionId = QRandomGenerator::global()->generate();
    }
    
    // Create request
    TrackerRequest request;
    request.hash = hash;
    request.host = host;
    request.port = port;
    request.created = QDateTime::currentDateTime();
    request.callback = callback;
    
    requests_[transactionId] = request;
    
    // Resolve hostname and send connect
    QHostInfo::lookupHost(host, this, [this, transactionId, port](const QHostInfo& hostInfo) {
        if (hostInfo.error() != QHostInfo::NoError || hostInfo.addresses().isEmpty()) {
            handleError(transactionId, "DNS resolution failed: " + hostInfo.errorString());
            return;
        }
        
        // Update host to resolved IP
        if (requests_.contains(transactionId)) {
            QString resolvedHost = hostInfo.addresses().first().toString();
            requests_[transactionId].host = resolvedHost;
            sendConnect(transactionId, resolvedHost, port);
        }
    });
}

void TrackerChecker::scrapeMultiple(const QString& hash,
                                    std::function<void(const TrackerResult&)> callback)
{
    QStringList trackers = defaultTrackers();
    
    if (trackers.isEmpty()) {
        TrackerResult result;
        result.error = "No trackers available";
        if (callback) callback(result);
        return;
    }
    
    // Setup multi-scrape state
    MultiScrapeState state;
    state.pending = trackers.size();
    state.callback = callback;
    multiScrapes_[hash] = state;
    
    // Scrape from all trackers
    for (const QString& tracker : trackers) {
        // Parse tracker URL: host:port
        QStringList parts = tracker.split(':');
        if (parts.size() != 2) continue;
        
        QString host = parts[0];
        quint16 port = parts[1].toUShort();
        
        scrape(host, port, hash, [this, hash](const TrackerResult& result) {
            if (!multiScrapes_.contains(hash)) {
                return;  // Already finished
            }
            
            MultiScrapeState& state = multiScrapes_[hash];
            state.pending--;
            
            // Update best result if this one is better
            if (result.success) {
                if (!state.best.success || result.seeders > state.best.seeders) {
                    state.best = result;
                    state.best.success = true;
                }
            }
            
            // Check if all requests completed
            if (state.pending <= 0) {
                auto cb = state.callback;
                TrackerResult best = state.best;
                multiScrapes_.remove(hash);
                
                if (cb) cb(best);
            }
        });
    }
}

QStringList TrackerChecker::defaultTrackers()
{
    // Common open trackers that support UDP scrape
    return {
        "tracker.opentrackr.org:1337",
        "tracker.openbittorrent.com:6969",
        "open.stealth.si:80",
        "tracker.torrent.eu.org:451",
        "exodus.desync.com:6969",
        "tracker.tiny-vps.com:6969",
        "tracker.moeking.me:6969",
        "opentracker.i2p.rocks:6969"
    };
}

// ============================================================================
// UDP Protocol Implementation
// ============================================================================

void TrackerChecker::sendConnect(quint32 transactionId, const QString& host, quint16 port)
{
    QByteArray buffer(16, 0);
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    
    stream << CONNECT_ID_HIGH;       // Connection ID high
    stream << CONNECT_ID_LOW;        // Connection ID low
    stream << ACTION_CONNECT;        // Action = connect
    stream << transactionId;         // Transaction ID
    
    QHostAddress addr(host);
    socket_->writeDatagram(buffer, addr, port);
    
    qDebug() << "TrackerChecker: Sent connect to" << host << ":" << port;
}

void TrackerChecker::sendScrape(quint32 transactionId, const TrackerRequest& request)
{
    // Build scrape packet: 8 bytes connection ID + 4 bytes action + 4 bytes transaction + 20 bytes hash
    QByteArray buffer(36, 0);
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    
    stream << request.connectionIdHigh;   // Connection ID high
    stream << request.connectionIdLow;    // Connection ID low
    stream << ACTION_SCRAPE;              // Action = scrape
    stream << transactionId;              // Transaction ID
    
    // Append info hash (20 bytes from 40 hex chars)
    QByteArray hashBytes = hexToBytes(request.hash);
    if (hashBytes.size() != 20) {
        handleError(transactionId, "Invalid hash format");
        return;
    }
    buffer.append(hashBytes);
    
    QHostAddress addr(request.host);
    socket_->writeDatagram(buffer, addr, request.port);
    
    qDebug() << "TrackerChecker: Sent scrape for" << request.hash.left(8) << "to" << request.host;
}

void TrackerChecker::onReadyRead()
{
    while (socket_->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(socket_->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;
        
        socket_->readDatagram(data.data(), data.size(), &sender, &senderPort);
        
        if (data.size() < 8) {
            continue;  // Too small
        }
        
        quint32 action = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData()));
        quint32 transactionId = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + 4));
        
        if (!requests_.contains(transactionId)) {
            continue;  // Unknown transaction
        }
        
        if (action == ACTION_CONNECT) {
            handleConnectResponse(data);
        } else if (action == ACTION_SCRAPE) {
            handleScrapeResponse(data);
        } else if (action == ACTION_ERROR) {
            QString error = "Tracker returned error";
            if (data.size() > 8) {
                error = QString::fromUtf8(data.mid(8));
            }
            handleError(transactionId, error);
        }
    }
}

void TrackerChecker::handleConnectResponse(const QByteArray& data)
{
    if (data.size() < 16) {
        return;
    }
    
    quint32 transactionId = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + 4));
    quint32 connectionIdHigh = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + 8));
    quint32 connectionIdLow = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + 12));
    
    if (!requests_.contains(transactionId)) {
        return;
    }
    
    TrackerRequest& request = requests_[transactionId];
    request.connectionIdHigh = connectionIdHigh;
    request.connectionIdLow = connectionIdLow;
    request.connected = true;
    
    qDebug() << "TrackerChecker: Connected to" << request.host;
    
    // Now send the scrape request
    sendScrape(transactionId, request);
}

void TrackerChecker::handleScrapeResponse(const QByteArray& data)
{
    if (data.size() < 20) {
        return;
    }
    
    quint32 transactionId = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + 4));
    
    if (!requests_.contains(transactionId)) {
        return;
    }
    
    // Parse scrape response: seeders (4), completed (4), leechers (4)
    quint32 seeders = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + 8));
    quint32 completed = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + 12));
    quint32 leechers = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + 16));
    
    TrackerRequest request = requests_.take(transactionId);
    
    TrackerResult result;
    result.tracker = QString("%1:%2").arg(request.host).arg(request.port);
    result.seeders = static_cast<int>(seeders);
    result.completed = static_cast<int>(completed);
    result.leechers = static_cast<int>(leechers);
    result.success = true;
    
    qDebug() << "TrackerChecker: Scrape result for" << request.hash.left(8) 
             << "- seeders:" << result.seeders << "leechers:" << result.leechers;
    
    emit scrapeResult(request.hash, result);
    
    if (request.callback) {
        request.callback(result);
    }
}

void TrackerChecker::handleError(quint32 transactionId, const QString& error)
{
    if (!requests_.contains(transactionId)) {
        return;
    }
    
    TrackerRequest request = requests_.take(transactionId);
    
    TrackerResult result;
    result.tracker = QString("%1:%2").arg(request.host).arg(request.port);
    result.success = false;
    result.error = error;
    
    qDebug() << "TrackerChecker: Error for" << request.hash.left(8) << "-" << error;
    
    if (request.callback) {
        request.callback(result);
    }
}

void TrackerChecker::onCleanupTimer()
{
    QDateTime now = QDateTime::currentDateTime();
    QList<quint32> expiredIds;
    
    for (auto it = requests_.begin(); it != requests_.end(); ++it) {
        if (it->created.msecsTo(now) > timeoutMs_) {
            expiredIds.append(it.key());
        }
    }
    
    for (quint32 transactionId : expiredIds) {
        handleError(transactionId, "Request timed out");
    }
}

QByteArray TrackerChecker::hexToBytes(const QString& hex) const
{
    QByteArray result;
    result.reserve(hex.length() / 2);
    
    for (int i = 0; i < hex.length(); i += 2) {
        bool ok;
        uchar byte = hex.mid(i, 2).toUShort(&ok, 16);
        if (ok) {
            result.append(static_cast<char>(byte));
        } else {
            return QByteArray();  // Invalid hex
        }
    }
    
    return result;
}
