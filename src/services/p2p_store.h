#ifndef RATS_SERVICE_P2P_STORE_H
#define RATS_SERVICE_P2P_STORE_H

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

namespace rats::net {
class P2PTransport;
}

namespace rats::service {

// A record stored in the distributed P2P store. The `data` object is the full
// JSON payload as it lives in the librats StorageManager (including the
// internal
// `_key` / `_peerId` / `_timestamp` metadata fields).
struct StoredRecord {
    QString key;
    QString type;
    QJsonObject data;
    QString peerId; // peer that created this record
    qint64 timestamp = 0;

    bool isValid() const { return !key.isEmpty(); }

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["key"] = key;
        obj["type"] = type;
        obj["data"] = data;
        obj["peerId"] = peerId;
        obj["timestamp"] = timestamp;
        return obj;
    }

    static StoredRecord fromJson(const QJsonObject& obj)
    {
        StoredRecord r;
        r.key = obj["key"].toString();
        r.type = obj["type"].toString();
        r.data = obj["data"].toObject();
        r.peerId = obj["peerId"].toString();
        r.timestamp = obj["timestamp"].toVariant().toLongLong();
        return r;
    }
};

// Qt wrapper around the librats distributed key-value StorageManager, borrowed
// from the transport. Provides put/get/find/sync of JSON records that replicate
// across peers; all librats access is confined to this class so services above
// it (e.g. VotingService) never touch librats directly. Behaviour is gated on
// the RATS_STORAGE feature flag — isAvailable() reports false when disabled.
//
// The record contract (a `type` field plus the injected `_key` / `_peerId` /
// `_timestamp` metadata) and the key format are replicated across the swarm, so
// changing either stops existing records (including votes) from aggregating.
class P2PStore : public QObject {
    Q_OBJECT

public:
    explicit P2PStore(net::P2PTransport* transport, QObject* parent = nullptr);
    ~P2PStore() override;

    // Storage availability / status ------------------------------------------
    bool isAvailable() const;
    QString ourPeerId() const;

    // Generic store operations -----------------------------------------------

    // Store `obj` under an explicit key, injecting the internal metadata. Callers
    // manage their own key namespace (e.g. per-peer vote records). `obj` should
    // carry a `type` field.
    bool put(const QString& key, const QJsonObject& obj);

    // Find all records whose key starts with `indexPrefix`.
    QList<StoredRecord> find(const QString& indexPrefix) const;

    bool has(const QString& key) const;

signals:
    // A record was stored, either locally (isRemote == false) or received from
    // a peer (isRemote == true).
    void recordStored(const rats::service::StoredRecord& record, bool isRemote);

private:
    void setupStorageCallbacks();

    net::P2PTransport* transport_;
};

} // namespace rats::service

// Registered so records can travel across threads through queued signal/slot
// connections (the librats change callback fires on a reactor thread).
Q_DECLARE_METATYPE(rats::service::StoredRecord)

#endif // RATS_SERVICE_P2P_STORE_H
