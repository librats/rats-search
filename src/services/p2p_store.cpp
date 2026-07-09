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

    storage->set_sync_complete_callback([](bool success, const std::string& error) {
        const QString errorMsg = QString::fromStdString(error);
        qInfo() << "[P2PStore] Sync completed, success:" << success
                << (errorMsg.isEmpty() ? "" : ", error: " + errorMsg);
    });

    qInfo() << "[P2PStore] Storage callbacks set up";
#endif
}

} // namespace rats::service
