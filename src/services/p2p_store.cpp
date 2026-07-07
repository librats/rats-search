#include "services/p2p_store.h"

#include "net/p2p_transport.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

// Include librats headers. Neutralise Qt's `emit` macro across them (librats'
// EventBus::emit collides with the Qt keyword macro).
#pragma push_macro("emit")
#undef emit
#ifdef RATS_STORAGE
#include "storage/storage.h"
#include "util/json.h"
#endif
#pragma pop_macro("emit")

namespace rats::service {

// ============================================================================
// Constructor / Destructor
// ============================================================================

P2PStore::P2PStore(net::P2PTransport* transport, QObject* parent) : QObject(parent), transport_(transport)
{
    qRegisterMetaType<rats::service::StoredRecord>("rats::service::StoredRecord");
    setupStorageCallbacks();
    qInfo() << "[P2PStore] initialized";
}

P2PStore::~P2PStore()
{
    qInfo() << "[P2PStore] destroyed";
}

// ============================================================================
// Availability and Status
// ============================================================================

bool P2PStore::isAvailable() const
{
#ifdef RATS_STORAGE
    return transport_ && transport_->storage() != nullptr;
#else
    return false;
#endif
}

bool P2PStore::isSynchronized() const
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return false;
    }
    return transport_->storage()->is_synced();
#else
    return false;
#endif
}

QString P2PStore::ourPeerId() const
{
    if (!transport_) {
        return QString();
    }
    return transport_->ourPeerId();
}

// ============================================================================
// Generic Store Operations
// ============================================================================

bool P2PStore::store(const QJsonObject& obj)
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        qWarning() << "[P2PStore] Storage not available";
        return false;
    }

    if (!isSynchronized()) {
        qWarning() << "[P2PStore] Cannot store - not synchronized with peers";
        return false;
    }

    // Get type and index
    QString type = obj["type"].toString();
    QString index = obj["_index"].toString();

    if (type.isEmpty()) {
        qWarning() << "[P2PStore] Object must have 'type' field";
        return false;
    }

    // Generate unique key and write it (put() injects the internal metadata).
    return put(generateKey(type, index), obj);
#else
    Q_UNUSED(obj);
    qWarning() << "[P2PStore] Storage feature not enabled (RATS_STORAGE not defined)";
    return false;
#endif
}

bool P2PStore::put(const QString& key, const QJsonObject& obj)
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        qWarning() << "[P2PStore] Storage not available";
        return false;
    }

    auto* storage = transport_->storage();

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
            qDebug() << "[P2PStore] Stored record with key:" << key;

            StoredRecord record;
            record.key = key;
            record.type = obj["type"].toString();
            record.data = obj;
            record.peerId = ourPeerId();
            record.timestamp = QDateTime::currentMSecsSinceEpoch();

            emit recordStored(record, false);
        }

        return result;
    } catch (const std::exception& e) {
        qWarning() << "[P2PStore] Failed to parse JSON:" << e.what();
        return false;
    }
#else
    Q_UNUSED(key);
    Q_UNUSED(obj);
    qWarning() << "[P2PStore] Storage feature not enabled (RATS_STORAGE not defined)";
    return false;
#endif
}

QList<StoredRecord> P2PStore::find(const QString& indexPrefix) const
{
    QList<StoredRecord> results;

#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return results;
    }

    auto* storage = transport_->storage();

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
                qWarning() << "[P2PStore] Failed to parse record:" << e.what();
            }
        }
    }
#else
    Q_UNUSED(indexPrefix);
#endif

    return results;
}

QJsonObject P2PStore::get(const QString& key) const
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return QJsonObject();
    }

    auto* storage = transport_->storage();
    auto jsonOpt = storage->get_json(key.toStdString());

    if (jsonOpt) {
        try {
            std::string jsonStr = jsonOpt->dump();
            QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(jsonStr));
            return doc.object();
        } catch (const std::exception& e) {
            qWarning() << "[P2PStore] Failed to parse JSON:" << e.what();
        }
    }
#else
    Q_UNUSED(key);
#endif

    return QJsonObject();
}

bool P2PStore::has(const QString& key) const
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return false;
    }
    return transport_->storage()->has(key.toStdString());
#else
    Q_UNUSED(key);
    return false;
#endif
}

bool P2PStore::remove(const QString& key)
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return false;
    }
    return transport_->storage()->remove(key.toStdString());
#else
    Q_UNUSED(key);
    return false;
#endif
}

QStringList P2PStore::keysWithPrefix(const QString& prefix) const
{
    QStringList results;

#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return results;
    }

    std::vector<std::string> keys = transport_->storage()->keys_with_prefix(prefix.toStdString());
    for (const auto& key : keys) {
        results.append(QString::fromStdString(key));
    }
#else
    Q_UNUSED(prefix);
#endif

    return results;
}

int P2PStore::size() const
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return 0;
    }
    return static_cast<int>(transport_->storage()->size());
#else
    return 0;
#endif
}

// ============================================================================
// Synchronization
// ============================================================================

bool P2PStore::requestSync()
{
#ifdef RATS_STORAGE
    if (!isAvailable()) {
        return false;
    }
    return transport_->storage()->request_sync();
#else
    return false;
#endif
}

QJsonObject P2PStore::getStatistics() const
{
    QJsonObject stats;

#ifdef RATS_STORAGE
    if (!isAvailable()) {
        stats["available"] = false;
        return stats;
    }

    auto* storage = transport_->storage();

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

void P2PStore::setupStorageCallbacks()
{
#ifdef RATS_STORAGE
    if (!transport_ || !transport_->storage()) {
        return;
    }

    auto* storage = transport_->storage();

    // Emit recordStored when a remote record arrives. Local changes are already
    // signalled from store().
    storage->set_change_callback([this](const librats::StorageChangeEvent& event) {
        if (!event.is_remote) {
            return;
        }

        try {
            std::string jsonStr(event.new_data.begin(), event.new_data.end());
            librats::Json data = librats::Json::parse(jsonStr);

            StoredRecord record;
            record.key = QString::fromStdString(event.key);
            record.type = QString::fromStdString(data.value("type", ""));
            record.peerId = QString::fromStdString(event.origin_peer_id);
            record.timestamp = static_cast<qint64>(event.timestamp_ms);

            QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(jsonStr));
            record.data = doc.object();

            emit recordStored(record, true);
        } catch (const std::exception& e) {
            qWarning() << "[P2PStore] Failed to process storage change:" << e.what();
        }
    });

    storage->set_sync_complete_callback([this](bool success, const std::string& error) {
        QString errorMsg = QString::fromStdString(error);
        qInfo() << "[P2PStore] Sync completed, success:" << success
                << (errorMsg.isEmpty() ? "" : ", error: " + errorMsg);
        emit syncCompleted(success, errorMsg);
    });

    qInfo() << "[P2PStore] Storage callbacks set up";
#endif
}

QString P2PStore::generateKey(const QString& type, const QString& index) const
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

} // namespace rats::service
