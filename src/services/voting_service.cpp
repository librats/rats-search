#include "services/voting_service.h"

#include "data/torrent_repository.h"
#include "domain/torrent_codec.h"
#include "services/p2p_store.h"

#include <QDebug>

namespace rats::service {

VotingService::VotingService(P2PStore* store, data::TorrentRepository* repository, QObject* parent)
    : QObject(parent), store_(store), repository_(repository)
{
    if (store_) {
        connect(store_, &P2PStore::recordStored, this, &VotingService::onRecordStored);
    }
    qInfo() << "[VotingService] initialized";
}

// ============================================================================
// Public API
// ============================================================================

void VotingService::vote(const QString& hash, bool isGood, ResultCallback callback)
{
    qInfo() << "[VotingService] vote hash:" << hash.left(16) << "isGood:" << isGood;

    if (hash.length() != 40) {
        if (callback)
            callback(false, QJsonObject(), QStringLiteral("Invalid hash"));
        return;
    }

    if (!repository_) {
        if (callback)
            callback(false, QJsonObject(), QStringLiteral("Repository not initialized"));
        return;
    }

    // Fetch without files for the dedup/early-return path; the file list is only
    // needed to build the replicated payload, so it is loaded lazily below.
    std::optional<domain::Torrent> existing = repository_->get(hash);
    if (!existing) {
        if (callback)
            callback(false, QJsonObject(), QStringLiteral("Torrent not found"));
        return;
    }
    domain::Torrent torrent = *existing;

    const bool storeReady = store_ && store_->isAvailable();

    // Already voted: return the current counts without double counting. The store
    // gives cross-session dedup when available; the in-memory set covers the case
    // where the store is down (otherwise every repeated click would keep bumping
    // the local good/bad columns without bound).
    if (selfVotedHashes_.contains(hash) || (storeReady && hasVoted(hash))) {
        int goodCount = torrent.good;
        int badCount = torrent.bad;
        if (storeReady) {
            const VoteCounts votes = aggregate(hash);
            goodCount = votes.good;
            badCount = votes.bad;
        }
        qInfo() << "[VotingService] already voted on" << hash.left(8) << "good:" << goodCount << "bad:" << badCount;

        QJsonObject result;
        result["hash"] = hash;
        result["good"] = goodCount;
        result["bad"] = badCount;
        result["selfVoted"] = true;
        result["alreadyVoted"] = true;
        if (callback)
            callback(true, result, QString());
        return;
    }

    // Store the vote in the distributed store (this replicates to all peers).
    bool storedInP2P = false;
    if (storeReady) {
        // The replicated vote carries a full index entry (with files) for peers
        // that do not have this torrent yet — load the file list now, on the only
        // path that actually needs it.
        if (auto full = repository_->get(hash, /*includeFiles*/ true))
            torrent = *full;
        QJsonObject torrentData = domain::codec::toJson(torrent, { /*includeFiles*/ true, /*includeInfo*/ true });
        storedInP2P = storeVote(hash, isGood, torrentData);
    }

    // Mirror the vote onto the local torrent counts for fast local access.
    if (isGood) {
        torrent.good++;
    } else {
        torrent.bad++;
    }
    repository_->update(torrent);
    selfVotedHashes_.insert(hash);

    // Prefer the distributed aggregate for the returned counts.
    int goodCount = torrent.good;
    int badCount = torrent.bad;
    if (storeReady) {
        VoteCounts votes = aggregate(hash);
        goodCount = votes.good;
        badCount = votes.bad;
    }

    emit votesUpdated(hash, goodCount, badCount);

    QJsonObject result;
    result["hash"] = hash;
    result["good"] = goodCount;
    result["bad"] = badCount;
    result["selfVoted"] = true;
    result["distributed"] = storedInP2P;
    if (callback)
        callback(true, result, QString());
}

void VotingService::getVotes(const QString& hash, ResultCallback callback)
{
    if (hash.length() != 40) {
        if (callback)
            callback(false, QJsonObject(), QStringLiteral("Invalid hash"));
        return;
    }

    QJsonObject result;
    result["hash"] = hash;

    if (store_ && store_->isAvailable()) {
        // Aggregates every peer's vote records.
        VoteCounts votes = aggregate(hash);
        result["good"] = votes.good;
        result["bad"] = votes.bad;
        result["selfVoted"] = votes.selfVoted;
        result["source"] = "distributed";
    } else if (repository_) {
        // Fall back to the local torrent's columns.
        std::optional<domain::Torrent> torrent = repository_->get(hash);
        if (torrent) {
            result["good"] = torrent->good;
            result["bad"] = torrent->bad;
            result["selfVoted"] = false; // cannot be determined locally
            result["source"] = "local";
        } else {
            result["good"] = 0;
            result["bad"] = 0;
            result["selfVoted"] = false;
            result["source"] = "none";
        }
    } else {
        result["good"] = 0;
        result["bad"] = 0;
        result["selfVoted"] = false;
        result["source"] = "unavailable";
    }

    if (callback)
        callback(true, result, QString());
}

VotingService::VoteCounts VotingService::aggregate(const QString& hash) const
{
    VoteCounts result;

    if (hash.length() != 40) {
        return result;
    }
    if (!store_ || !store_->isAvailable()) {
        return result;
    }

    const QString peerId = store_->ourPeerId();

    // One record per peer: "vote:{hash}:{peerId}".
    const QString prefix = QStringLiteral("vote:%1:").arg(hash);
    const QList<StoredRecord> records = store_->find(prefix);

    for (const StoredRecord& record : records) {
        const QString vote = record.data.value("vote").toString();
        if (vote == QLatin1String("good")) {
            result.good++;
        } else if (vote == QLatin1String("bad")) {
            result.bad++;
        }

        if (record.peerId == peerId) {
            result.selfVoted = true;
        }
    }

    return result;
}

bool VotingService::hasVoted(const QString& hash) const
{
    if (hash.length() != 40) {
        return false;
    }
    if (!store_ || !store_->isAvailable()) {
        return false;
    }

    const QString key = QStringLiteral("vote:%1:%2").arg(hash, store_->ourPeerId());
    return store_->has(key);
}

// ============================================================================
// Private
// ============================================================================

bool VotingService::storeVote(const QString& hash, bool isGood, const QJsonObject& torrentData)
{
    if (!store_ || !store_->isAvailable()) {
        qWarning() << "[VotingService] Storage not available for voting";
        return false;
    }

    const QString peerId = store_->ourPeerId();

    // Key format: vote:{hash}:{peerId} — one vote per peer per torrent.
    const QString key = QStringLiteral("vote:%1:%2").arg(hash, peerId);

    QJsonObject voteData;
    voteData["type"] = "vote";
    voteData["torrentHash"] = hash;
    voteData["vote"] = isGood ? "good" : "bad";
    voteData["_index"] = QStringLiteral("vote:%1").arg(hash);

    // Include torrent data for replication (like the legacy _temp field).
    if (!torrentData.isEmpty()) {
        voteData["_torrent"] = torrentData;
    }

    const bool result = store_->put(key, voteData);
    if (result) {
        qInfo() << "[VotingService] stored" << (isGood ? "good" : "bad") << "vote for" << hash.left(8);
        emit voteStored(hash, isGood, peerId);
    }
    return result;
}

void VotingService::onRecordStored(const StoredRecord& record, bool isRemote)
{
    // Local votes already emit votesUpdated from vote(); only react to peers'.
    if (!isRemote || record.type != QLatin1String("vote")) {
        return;
    }

    const QString hash = record.data.value("torrentHash").toString();
    const bool isGood = record.data.value("vote").toString() == QLatin1String("good");
    emit voteStored(hash, isGood, record.peerId);

    if (hash.length() != 40 || !repository_) {
        return;
    }

    // A peer's vote changed the swarm aggregate: mirror it onto the local torrent
    // columns (so offline reads stay correct) and notify the UI/feed. Without this
    // a remote vote would never surface — voteStored has no other subscriber.
    const VoteCounts votes = aggregate(hash);
    std::optional<domain::Torrent> torrent = repository_->get(hash);
    if (torrent && (torrent->good != votes.good || torrent->bad != votes.bad)) {
        torrent->good = votes.good;
        torrent->bad = votes.bad;
        repository_->update(*torrent);
    }
    emit votesUpdated(hash, votes.good, votes.bad);
}

} // namespace rats::service
