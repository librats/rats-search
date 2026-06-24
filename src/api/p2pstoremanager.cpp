#include "p2pstoremanager.h"
#include "../p2pnetwork.h"

#include <QDebug>
#include <QDateTime>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// Include librats headers. Neutralise Qt's `emit` macro across them
// (librats' EventBus::emit collides with the Qt keyword macro).
#pragma push_macro("emit")
#undef emit
#ifdef RATS_STORAGE
#include "storage/storage.h"
#include "util/json.h"
#endif
#pragma pop_macro("emit")

// ============================================================================
// Constructor / Destructor
// ============================================================================

P2PStoreManager::P2PStoreManager(P2PNetwork* p2p, QObject *parent)
    : QObject(parent)
    , p2p_(p2p)
{
    setupStorageCallbacks();
    qInfo() << "P2PStoreManager initialized";
}

P2PStoreManager::~P2PStoreManager()
{
    qInfo() << "P2PStoreManager destroyed";
}

// ============================================================================
// Availability and Status
// ============================================================================

bool P2PStoreManager::isAvailable() const
{
#ifdef RATS_STORAGE
    return p2p_ && p2p_->storage() != nullptr;
#else
    return false;
#endif
}

bool P2PStoreManager::isSynchronized() const
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return false;
    }
    return p2p_->storage()->is_synced();
#else
    return false;
#endif
}

QString P2PStoreManager::ourPeerId() const
{
    if (!p2p_) {
        return QString();
    }
    return p2p_->getOurPeerId();
}

// ============================================================================
// Generic Store Operations
// ============================================================================

bool P2PStoreManager::store(const QJsonObject& obj)
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        qWarning() << "P2PStoreManager: Storage not available";
        return false;
    }
    
    if (!isSynchronized()) {
        qWarning() << "P2PStoreManager: Cannot store - not synchronized with peers";
        return false;
    }
    
    auto* storage = p2p_->storage();

    // Get type and index
    QString type = obj["type"].toString();
    QString index = obj["_index"].toString();

    if (type.isEmpty()) {
        qWarning() << "P2PStoreManager: Object must have 'type' field";
        return false;
    }

    // Generate unique key
    QString key = generateKey(type, index);

    // Convert to librats::Json
    QJsonDocument doc(obj);
    std::string jsonStr = doc.toJson(QJsonDocument::Compact).toStdString();

    try {
        librats::Json data = librats::Json::parse(jsonStr);

        // Add metadata
        data["_key"] = key.toStdString();
        data["_peerId"] = ourPeerId().toStdString();
        data["_timestamp"] = static_cast<int64_t>(QDateTime::currentMSecsSinceEpoch());

        bool result = storage->put_json(key.toStdString(), data);
        
        if (result) {
            qDebug() << "P2PStoreManager: Stored record with key:" << key;
            
            // Emit signal
            StoredRecord record;
            record.key = key;
            record.type = type;
            record.data = obj;
            record.peerId = ourPeerId();
            record.timestamp = QDateTime::currentMSecsSinceEpoch();
            
            emit recordStored(record, false);
        }
        
        return result;
    } catch (const std::exception& e) {
        qWarning() << "P2PStoreManager: Failed to parse JSON:" << e.what();
        return false;
    }
#else
    Q_UNUSED(obj);
    qWarning() << "P2PStoreManager: Storage feature not enabled (RATS_STORAGE not defined)";
    return false;
#endif
}

QList<StoredRecord> P2PStoreManager::find(const QString& indexPrefix) const
{
    QList<StoredRecord> results;
    
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return results;
    }
    
    auto* storage = p2p_->storage();

    // Get all keys with the index prefix
    std::vector<std::string> keys = storage->keys_with_prefix(indexPrefix.toStdString());

    for (const auto& key : keys) {
        auto jsonOpt = storage->get_json(key);
        if (jsonOpt) {
            try {
                StoredRecord record;
                record.key = QString::fromStdString(key);
                record.type = QString::fromStdString((*jsonOpt)["type"].get<std::string>());
                record.peerId = QString::fromStdString((*jsonOpt).value("_peerId", ""));
                record.timestamp = (*jsonOpt).value("_timestamp", 0LL);
                
                // Convert librats::Json to QJsonObject
                std::string jsonStr = jsonOpt->dump();
                QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(jsonStr));
                record.data = doc.object();
                
                results.append(record);
            } catch (const std::exception& e) {
                qWarning() << "P2PStoreManager: Failed to parse record:" << e.what();
            }
        }
    }
#else
    Q_UNUSED(indexPrefix);
#endif
    
    return results;
}

QJsonObject P2PStoreManager::get(const QString& key) const
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return QJsonObject();
    }
    
    auto* storage = p2p_->storage();
    auto jsonOpt = storage->get_json(key.toStdString());

    if (jsonOpt) {
        try {
            std::string jsonStr = jsonOpt->dump();
            QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(jsonStr));
            return doc.object();
        } catch (const std::exception& e) {
            qWarning() << "P2PStoreManager: Failed to parse JSON:" << e.what();
        }
    }
#else
    Q_UNUSED(key);
#endif
    
    return QJsonObject();
}

bool P2PStoreManager::has(const QString& key) const
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return false;
    }
    return p2p_->storage()->has(key.toStdString());
#else
    Q_UNUSED(key);
    return false;
#endif
}

bool P2PStoreManager::remove(const QString& key)
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return false;
    }
    return p2p_->storage()->remove(key.toStdString());
#else
    Q_UNUSED(key);
    return false;
#endif
}

QStringList P2PStoreManager::keysWithPrefix(const QString& prefix) const
{
    QStringList results;
    
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return results;
    }
    
    std::vector<std::string> keys = p2p_->storage()->keys_with_prefix(prefix.toStdString());
    for (const auto& key : keys) {
        results.append(QString::fromStdString(key));
    }
#else
    Q_UNUSED(prefix);
#endif
    
    return results;
}

int P2PStoreManager::size() const
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return 0;
    }
    return static_cast<int>(p2p_->storage()->size());
#else
    return 0;
#endif
}

// ============================================================================
// Voting System
// ============================================================================

bool P2PStoreManager::storeVote(const QString& hash, bool isGood, const QJsonObject& torrentData)
{
    if (hash.length() != 40) {
        qWarning() << "P2PStoreManager: Invalid hash for vote";
        return false;
    }
    
    // Check if already voted
    if (hasVoted(hash)) {
        qInfo() << "P2PStoreManager: Already voted on" << hash.left(8);
        return false;
    }
    
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        qWarning() << "P2PStoreManager: Storage not available for voting";
        return false;
    }
    
    auto* storage = p2p_->storage();
    QString peerId = ourPeerId();

    // Create vote record
    // Key format: vote:{hash}:{peerId} - ensures one vote per peer per torrent
    QString key = QString("vote:%1:%2").arg(hash, peerId);
    
    QJsonObject voteData;
    voteData["type"] = "vote";
    voteData["torrentHash"] = hash;
    voteData["vote"] = isGood ? "good" : "bad";
    voteData["_index"] = QString("vote:%1").arg(hash);
    
    // Include torrent data for replication (like legacy _temp field)
    if (!torrentData.isEmpty()) {
        voteData["_torrent"] = torrentData;
    }
    
    // Convert to librats::Json
    QJsonDocument doc(voteData);
    std::string jsonStr = doc.toJson(QJsonDocument::Compact).toStdString();
    
    try {
        librats::Json data = librats::Json::parse(jsonStr);
        data["_key"] = key.toStdString();
        data["_peerId"] = peerId.toStdString();
        data["_timestamp"] = static_cast<int64_t>(QDateTime::currentMSecsSinceEpoch());

        bool result = storage->put_json(key.toStdString(), data);
        
        if (result) {
            qInfo() << "P2PStoreManager: Stored" << (isGood ? "good" : "bad") 
                    << "vote for" << hash.left(8);
            emit voteStored(hash, isGood, peerId);
        }
        
        return result;
    } catch (const std::exception& e) {
        qWarning() << "P2PStoreManager: Failed to store vote:" << e.what();
        return false;
    }
#else
    Q_UNUSED(hash);
    Q_UNUSED(isGood);
    Q_UNUSED(torrentData);
    return false;
#endif
}

VoteCounts P2PStoreManager::getVotes(const QString& hash) const
{
    VoteCounts result;
    
    if (hash.length() != 40) {
        return result;
    }
    
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return result;
    }
    
    auto* storage = p2p_->storage();
    QString peerId = ourPeerId();

    // Find all vote records for this hash
    QString prefix = QString("vote:%1:").arg(hash);
    std::vector<std::string> keys = storage->keys_with_prefix(prefix.toStdString());

    for (const auto& key : keys) {
        auto jsonOpt = storage->get_json(key);
        if (jsonOpt) {
            try {
                std::string vote = (*jsonOpt).value("vote", "");
                std::string votePeerId = (*jsonOpt).value("_peerId", "");
                
                if (vote == "good") {
                    result.good++;
                } else if (vote == "bad") {
                    result.bad++;
                }
                
                // Check if this is our vote
                if (votePeerId == peerId.toStdString()) {
                    result.selfVoted = true;
                }
            } catch (const std::exception& e) {
                qWarning() << "P2PStoreManager: Failed to parse vote:" << e.what();
            }
        }
    }
#else
    Q_UNUSED(hash);
#endif
    
    return result;
}

bool P2PStoreManager::hasVoted(const QString& hash) const
{
    if (hash.length() != 40) {
        return false;
    }
    
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return false;
    }
    
    QString key = QString("vote:%1:%2").arg(hash, ourPeerId());
    return p2p_->storage()->has(key.toStdString());
#else
    Q_UNUSED(hash);
    return false;
#endif
}

// ============================================================================
// Synchronization
// ============================================================================

bool P2PStoreManager::requestSync()
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return false;
    }
    return p2p_->storage()->request_sync();
#else
    return false;
#endif
}

QJsonObject P2PStoreManager::getStatistics() const
{
    QJsonObject stats;
    
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        stats["available"] = false;
        return stats;
    }
    
    auto* storage = p2p_->storage();

    stats["available"] = true;
    stats["synchronized"] = isSynchronized();
    stats["size"] = size();
    stats["peerId"] = ourPeerId();

    // Get detailed stats from librats
    librats::Json libStats = storage->get_statistics_json();
    std::string statsStr = libStats.dump();
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(statsStr));
    
    if (!doc.isNull()) {
        QJsonObject libStatsObj = doc.object();
        for (auto it = libStatsObj.begin(); it != libStatsObj.end(); ++it) {
            stats[it.key()] = it.value();
        }
    }
#else
    stats["available"] = false;
    stats["error"] = "RATS_STORAGE not enabled";
#endif
    
    return stats;
}

// ============================================================================
// Private Methods
// ============================================================================

void P2PStoreManager::setupStorageCallbacks()
{
#ifdef RATS_STORAGE
    if (!p2p_ || !p2p_->storage()) {
        return;
    }

    auto* storage = p2p_->storage();

    // Setup change callback to emit signals when remote records arrive
    storage->set_change_callback([this](const librats::StorageChangeEvent& event) {
        if (!event.is_remote) {
            return;  // We already handle local changes in store()
        }

        try {
            // Parse the new data as JSON
            std::string jsonStr(event.new_data.begin(), event.new_data.end());
            librats::Json data = librats::Json::parse(jsonStr);
            
            StoredRecord record;
            record.key = QString::fromStdString(event.key);
            record.type = QString::fromStdString(data.value("type", ""));
            record.peerId = QString::fromStdString(event.origin_peer_id);
            record.timestamp = event.timestamp_ms;
            
            // Convert to QJsonObject
            QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(jsonStr));
            record.data = doc.object();
            
            emit recordStored(record, true);
            
            // If it's a vote, emit vote signal
            if (record.type == "vote") {
                QString hash = record.data["torrentHash"].toString();
                bool isGood = record.data["vote"].toString() == "good";
                emit voteStored(hash, isGood, record.peerId);
            }
            
        } catch (const std::exception& e) {
            qWarning() << "P2PStoreManager: Failed to process storage change:" << e.what();
        }
    });
    
    // Setup sync complete callback
    storage->set_sync_complete_callback([this](bool success, const std::string& error) {
        QString errorMsg = QString::fromStdString(error);
        qInfo() << "P2PStoreManager: Sync completed, success:" << success 
                << (errorMsg.isEmpty() ? "" : ", error: " + errorMsg);
        emit syncCompleted(success, errorMsg);
    });
    
    qInfo() << "P2PStoreManager: Storage callbacks set up";
#endif
}

QString P2PStoreManager::generateKey(const QString& type, const QString& index) const
{
    // Generate unique key: {type}:{index_or_uuid}:{peerId}
    QString peerId = ourPeerId();
    QString indexPart = index.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : index;
    
    // If index already contains the type, use it directly
    if (indexPart.startsWith(type + ":")) {
        return QString("%1:%2").arg(indexPart, peerId);
    }
    
    return QString("%1:%2:%3").arg(type, indexPart, peerId);
}
