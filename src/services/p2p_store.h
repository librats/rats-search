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
// JSON payload as it lives in the librats StorageManager (including the internal
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
// Ported from the old api/P2PStoreManager; the generic record contract (type +
// optional `_index`, plus injected `_key` / `_peerId` / `_timestamp` metadata)
// and the generated key format are preserved so existing replicated records
// (including votes) keep aggregating.
class P2PStore : public QObject {
    Q_OBJECT

public:
    explicit P2PStore(net::P2PTransport* transport, QObject* parent = nullptr);
    ~P2PStore() override;

    // Storage availability / status ------------------------------------------
    bool isAvailable() const;
    bool isSynchronized() const;
    QString ourPeerId() const;

    // Generic store operations -----------------------------------------------

    // Store a JSON object. It must carry a `type` field; an optional `_index`
    // field controls the generated key (and find() prefix). Injects internal
    // metadata and replicates to peers. Returns false if not synced/available.
    bool store(const QJsonObject& obj);

    // Store `obj` under an explicit key, injecting the internal metadata. Unlike
    // store() this does not gate on peer synchronization, so callers that manage
    // their own key namespace (e.g. per-peer vote records) can write before the
    // store has synced. `obj` should still carry a `type` field.
    bool put(const QString& key, const QJsonObject& obj);

    // Find all records whose key starts with `indexPrefix`.
    QList<StoredRecord> find(const QString& indexPrefix) const;

    QJsonObject get(const QString& key) const;
    bool has(const QString& key) const;
    bool remove(const QString& key);
    QStringList keysWithPrefix(const QString& prefix) const;
    int size() const;

    // Synchronization --------------------------------------------------------
    bool requestSync();
    QJsonObject getStatistics() const;

signals:
    // A record was stored, either locally (isRemote == false) or received from
    // a peer (isRemote == true).
    void recordStored(const rats::service::StoredRecord& record, bool isRemote);

    // Peer synchronization finished.
    void syncCompleted(bool success, const QString& error);

private:
    void setupStorageCallbacks();
    QString generateKey(const QString& type, const QString& index) const;

    net::P2PTransport* transport_;
};

} // namespace rats::service

// Registered so records can travel across threads through queued signal/slot
// connections (the librats change callback fires on a reactor thread).
Q_DECLARE_METATYPE(rats::service::StoredRecord)

#endif // RATS_SERVICE_P2P_STORE_H
