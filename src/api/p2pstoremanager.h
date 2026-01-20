#ifndef P2PSTOREMANAGER_H
#define P2PSTOREMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <functional>
#include <memory>

class P2PNetwork;

/**
 * @brief VoteCounts - Result of vote aggregation
 */
struct VoteCounts {
    int good = 0;
    int bad = 0;
    bool selfVoted = false;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["good"] = good;
        obj["bad"] = bad;
        obj["selfVoted"] = selfVoted;
        return obj;
    }
};

/**
 * @brief StoredRecord - A record stored in the distributed P2P store
 */
struct StoredRecord {
    QString key;
    QString type;
    QJsonObject data;
    QString peerId;  // Peer that created this record
    qint64 timestamp = 0;
    
    bool isValid() const { return !key.isEmpty(); }
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["key"] = key;
        obj["type"] = type;
        obj["data"] = data;
        obj["peerId"] = peerId;
        obj["timestamp"] = timestamp;
        return obj;
    }
    
    static StoredRecord fromJson(const QJsonObject& obj) {
        StoredRecord r;
        r.key = obj["key"].toString();
        r.type = obj["type"].toString();
        r.data = obj["data"].toObject();
        r.peerId = obj["peerId"].toString();
        r.timestamp = obj["timestamp"].toVariant().toLongLong();
        return r;
    }
};

/**
 * @brief P2PStoreManager - Qt wrapper for librats distributed storage
 * 
 * This provides a Qt-friendly interface to the librats StorageManager,
 * enabling distributed key-value storage that syncs across peers.
 * 
 * Key features:
 * - Store/retrieve JSON objects
 * - Find records by index prefix
 * - Voting system for torrents
 * - Automatic P2P synchronization
 * 
 * Usage pattern (like legacy store.js):
 * 
 *   // Store a vote
 *   store->storeVote(hash, true);
 *   
 *   // Get all votes for a hash
 *   VoteCounts votes = store->getVotes(hash);
 *   
 *   // Find records by index
 *   QList<StoredRecord> records = store->find("vote:" + hash);
 */
class P2PStoreManager : public QObject
{
    Q_OBJECT

public:
    explicit P2PStoreManager(P2PNetwork* p2p, QObject *parent = nullptr);
    ~P2PStoreManager();
    
    /**
     * @brief Check if storage is available
     */
    bool isAvailable() const;
    
    /**
     * @brief Check if storage is synchronized with peers
     */
    bool isSynchronized() const;
    
    /**
     * @brief Get our peer ID
     */
    QString ourPeerId() const;
    
    // =========================================================================
    // Generic Store Operations
    // =========================================================================
    
    /**
     * @brief Store a JSON object
     * 
     * The object should contain:
     * - type: Record type (e.g., "vote")
     * - _index: Optional index for find() queries
     * - ... other fields
     * 
     * @param obj Object to store
     * @return true if stored successfully
     */
    bool store(const QJsonObject& obj);
    
    /**
     * @brief Find records by index prefix
     * 
     * Uses the _index field stored with records.
     * 
     * @param indexPrefix Prefix to search for (e.g., "vote:HASH")
     * @return List of matching records
     */
    QList<StoredRecord> find(const QString& indexPrefix) const;
    
    /**
     * @brief Get a stored value by key
     */
    QJsonObject get(const QString& key) const;
    
    /**
     * @brief Check if key exists
     */
    bool has(const QString& key) const;
    
    /**
     * @brief Delete a key
     */
    bool remove(const QString& key);
    
    /**
     * @brief Get all keys with prefix
     */
    QStringList keysWithPrefix(const QString& prefix) const;
    
    /**
     * @brief Get store size
     */
    int size() const;
    
    // =========================================================================
    // Voting System (convenience methods for torrent voting)
    // =========================================================================
    
    /**
     * @brief Store a vote for a torrent
     * 
     * Creates a vote record with:
     * - type: "vote"
     * - torrentHash: hash
     * - vote: "good" or "bad"
     * - _index: "vote:{hash}"
     * 
     * @param hash Torrent hash
     * @param isGood true for upvote, false for downvote
     * @param torrentData Optional torrent data to include for replication
     * @return true if vote was stored (false if already voted)
     */
    bool storeVote(const QString& hash, bool isGood, const QJsonObject& torrentData = QJsonObject());
    
    /**
     * @brief Get aggregated votes for a torrent
     * 
     * Aggregates all vote records from all peers for this hash.
     * 
     * @param hash Torrent hash
     * @return Vote counts including selfVoted flag
     */
    VoteCounts getVotes(const QString& hash) const;
    
    /**
     * @brief Check if we have voted on a torrent
     */
    bool hasVoted(const QString& hash) const;
    
    // =========================================================================
    // Synchronization
    // =========================================================================
    
    /**
     * @brief Request synchronization from peers
     */
    bool requestSync();
    
    /**
     * @brief Get statistics
     */
    QJsonObject getStatistics() const;

signals:
    /**
     * @brief Emitted when a record is stored (locally or remotely)
     */
    void recordStored(const StoredRecord& record, bool isRemote);
    
    /**
     * @brief Emitted when synchronization completes
     */
    void syncCompleted(bool success, const QString& error);
    
    /**
     * @brief Emitted when a vote is stored
     */
    void voteStored(const QString& hash, bool isGood, const QString& peerId);

private:
    P2PNetwork* p2p_;
    
    void setupStorageCallbacks();
    QString generateKey(const QString& type, const QString& index) const;
};

#endif // P2PSTOREMANAGER_H
