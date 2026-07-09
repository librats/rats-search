#include "peer/peer_api.h"

#include "app/application.h"
#include "app/config_store.h"
#include "data/torrent_repository.h"
#include "domain/torrent_codec.h"
#include "net/p2p_transport.h"
#include "services/feed_service.h"
#include "services/indexing_service.h"
#include "services/replication_service.h"
#include "services/search_service.h"

#include <QDebug>

namespace rats::peer {

namespace {

// Build a SearchService request from a peer message, tolerating both the legacy
// "navigation" wrapper and the flat top-level form the newer peers send.
service::SearchService::Request parseSearchRequest(const QJsonObject& data)
{
    service::SearchService::Request req;
    req.query = data["text"].toString();
    if (req.query.isEmpty())
        req.query = data["query"].toString();

    if (data.contains("navigation")) {
        const QJsonObject nav = data["navigation"].toObject();
        req.limit = nav["limit"].toInt(10);
        req.offset = nav["index"].toInt(0);
        req.sort = nav["orderBy"].toString();
        req.descending = nav["orderDesc"].toBool(true);
        req.safeSearch = nav["safeSearch"].toBool(false);
        req.contentType = nav["type"].toString();
    } else {
        req.limit = data["limit"].toInt(10);
        req.offset = data["index"].toInt(0);
        req.sort = data["orderBy"].toString();
        req.descending = data["orderDesc"].toBool(true);
        req.safeSearch = data["safeSearch"].toBool(false);
    }
    return req;
}

QString shortId(const QString& peerId)
{
    return peerId.left(8);
}

} // namespace

PeerApi::PeerApi(app::Application* app, QObject* parent) : QObject(parent), app_(app)
{
    install();
}

void PeerApi::install()
{
    net::P2PTransport* transport = app_->transport();
    if (!transport) {
        qWarning() << "[PeerApi] no transport available; handlers not installed";
        return;
    }

    // --- Requests we answer -------------------------------------------------
    // "searchTorrent" is the legacy alias of "torrent_search"; both route here.
    transport->registerHandler("torrent_search",
        [this](const QString& peerId, const QJsonObject& data) { handleSearchRequest(peerId, data); });
    transport->registerHandler(
        "searchTorrent", [this](const QString& peerId, const QJsonObject& data) { handleSearchRequest(peerId, data); });
    transport->registerHandler("searchFiles",
        [this](const QString& peerId, const QJsonObject& data) { handleSearchFilesRequest(peerId, data); });
    transport->registerHandler("topTorrents",
        [this](const QString& peerId, const QJsonObject& data) { handleTopTorrentsRequest(peerId, data); });
    transport->registerHandler(
        "torrent", [this](const QString& peerId, const QJsonObject& data) { handleTorrentRequest(peerId, data); });
    transport->registerHandler(
        "feed", [this](const QString& peerId, const QJsonObject& data) { handleFeedRequest(peerId, data); });
    transport->registerHandler("randomTorrents",
        [this](const QString& peerId, const QJsonObject& data) { handleRandomTorrentsRequest(peerId, data); });

    // --- Responses we consume ----------------------------------------------
    transport->registerHandler("torrent_search_result",
        [this](const QString& peerId, const QJsonObject& data) { handleSearchResult(peerId, data); });
    transport->registerHandler("searchFiles_result",
        [this](const QString& peerId, const QJsonObject& data) { handleSearchFilesResult(peerId, data); });
    transport->registerHandler("torrent_response",
        [this](const QString& peerId, const QJsonObject& data) { handleTorrentResponse(peerId, data); });
    transport->registerHandler(
        "feed_response", [this](const QString& peerId, const QJsonObject& data) { handleFeedResponse(peerId, data); });
    transport->registerHandler("randomTorrents_response",
        [this](const QString& peerId, const QJsonObject& data) { handleRandomTorrentsResponse(peerId, data); });
    transport->registerHandler("torrent_announce",
        [this](const QString& peerId, const QJsonObject& data) { handleTorrentAnnounce(peerId, data); });

    // Follow-up work on connect (the client_info handshake itself is
    // PeerRegistry's).
    connect(transport, &net::P2PTransport::peerConnected, this, &PeerApi::onPeerConnected);

    qInfo() << "[PeerApi] P2P handlers installed";
}

// ============================================================================
// Requests we answer
// ============================================================================

void PeerApi::handleSearchRequest(const QString& peerId, const QJsonObject& data)
{
    service::SearchService::Request req = parseSearchRequest(data);
    if (req.query.length() <= 2) {
        qInfo() << "[PeerApi] search query too short from" << shortId(peerId) << "- ignoring";
        return;
    }

    const QVector<domain::SearchHit> hits = app_->search()->searchTorrents(req);
    qInfo() << "[PeerApi] search" << req.query << "->" << hits.size() << "results for" << shortId(peerId);

    for (const domain::SearchHit& hit : hits)
        app_->transport()->sendMessage(peerId, "torrent_search_result", domain::codec::toJson(hit.torrent));
}

void PeerApi::handleSearchFilesRequest(const QString& peerId, const QJsonObject& data)
{
    service::SearchService::Request req = parseSearchRequest(data);
    if (req.query.length() <= 2)
        return;

    const QVector<domain::SearchHit> hits = app_->search()->searchFiles(req);
    qInfo() << "[PeerApi] searchFiles" << req.query << "->" << hits.size() << "results for" << shortId(peerId);

    for (const domain::SearchHit& hit : hits) {
        QJsonObject result = domain::codec::toJson(hit);
        // Legacy peers read matching file paths from the "path" key.
        if (!hit.matchingPaths.isEmpty()) {
            QJsonArray paths;
            for (const QString& p : hit.matchingPaths)
                paths.append(p);
            result["path"] = paths;
        }
        app_->transport()->sendMessage(peerId, "searchFiles_result", result);
    }
}

void PeerApi::handleTopTorrentsRequest(const QString& peerId, const QJsonObject& data)
{
    QString type = data["type"].toString();
    QString time;
    int index = 0;
    int limit = 20;
    if (data.contains("navigation")) {
        const QJsonObject nav = data["navigation"].toObject();
        time = nav["time"].toString();
        index = nav["index"].toInt(0);
        limit = nav["limit"].toInt(20);
    } else {
        time = data["time"].toString();
        index = data["index"].toInt(0);
        limit = data["limit"].toInt(20);
    }

    const QVector<domain::Torrent> results = app_->search()->top(type, time, index, limit);
    qInfo() << "[PeerApi] topTorrents ->" << results.size() << "results for" << shortId(peerId);

    QJsonArray torrents;
    for (const domain::Torrent& t : results)
        torrents.append(domain::codec::toJson(t));

    app_->transport()->sendMessage(
        peerId, "topTorrents_response", QJsonObject { { "torrents", torrents }, { "type", type }, { "time", time } });
}

void PeerApi::handleTorrentRequest(const QString& peerId, const QJsonObject& data)
{
    const QString hash = data["hash"].toString();
    if (hash.length() != 40)
        return;

    bool includeFiles = false;
    if (data.contains("options"))
        includeFiles = data["options"].toObject()["files"].toBool(false);
    else
        includeFiles = data["files"].toBool(false);

    const std::optional<domain::Torrent> torrent = app_->search()->get(hash, includeFiles);
    if (!torrent || !torrent->isValid())
        return; // not found: the wire protocol has no "miss" reply, so stay silent

    qInfo() << "[PeerApi] torrent" << hash.left(8) << "for" << shortId(peerId);
    app_->transport()->sendMessage(peerId, "torrent_response",
        domain::codec::toJson(*torrent, { /*includeFiles*/ includeFiles, /*includeInfo*/ true }));
}

void PeerApi::handleFeedRequest(const QString& peerId, const QJsonObject& data)
{
    Q_UNUSED(data);
    service::FeedService* feed = app_->feed();
    if (!feed)
        return;

    qInfo() << "[PeerApi] feed request from" << shortId(peerId);

    // FeedService serialises each item with its file list already, so no per-item
    // enrichment query is needed here.
    QJsonObject response;
    response["feed"] = feed->toJsonArray(0, feed->size());
    response["feedDate"] = feed->feedDate();
    response["size"] = feed->size();
    app_->transport()->sendMessage(peerId, "feed_response", response);
}

void PeerApi::handleRandomTorrentsRequest(const QString& peerId, const QJsonObject& data)
{
    if (!app_->config() || !app_->config()->p2pReplicationServer()) {
        qInfo() << "[PeerApi] replication server disabled; ignoring randomTorrents from" << shortId(peerId);
        return;
    }

    int limit = qBound(1, data["limit"].toInt(5), 10);
    const QVector<domain::Torrent> torrents = app_->torrents()->random(limit, /*includeFiles*/ true);
    qInfo() << "[PeerApi] randomTorrents ->" << torrents.size() << "for" << shortId(peerId);

    QJsonArray array;
    for (const domain::Torrent& t : torrents)
        array.append(domain::codec::toJson(t, { /*includeFiles*/ true, /*includeInfo*/ true }));

    app_->transport()->sendMessage(peerId, "randomTorrents_response", QJsonObject { { "torrents", array } });
}

// ============================================================================
// Responses we consume
// ============================================================================

void PeerApi::handleSearchResult(const QString& peerId, const QJsonObject& data)
{
    QString hash = data["hash"].toString();
    if (hash.isEmpty())
        hash = data["info_hash"].toString();
    if (hash.isEmpty())
        return;

    // Surface the hit to the UI with remote provenance stamped on.
    QJsonObject result = data;
    result["remote"] = true;
    result["peer"] = peerId;
    emit remoteSearchResults(QString(), QJsonArray { result });

    // Index the received torrent through the single insertion path.
    insertFromPeer(data, /*trackReplication*/ false);
}

void PeerApi::handleSearchFilesResult(const QString& peerId, const QJsonObject& data)
{
    const QString query = data["text"].toString();

    QJsonObject item = data;
    item["isFileMatch"] = true;
    item["remote"] = true;
    item["peer"] = peerId;
    // Normalise legacy "path" onto "matchingPaths" for consistent consumers.
    if (data.contains("path"))
        item["matchingPaths"] = data["path"];

    emit remoteFileSearchResults(query, QJsonArray { item });
    qInfo() << "[PeerApi] file search result from" << shortId(peerId) << "for" << query;
}

void PeerApi::handleTorrentResponse(const QString& peerId, const QJsonObject& data)
{
    QString hash = data["hash"].toString();
    if (hash.isEmpty())
        hash = data["info_hash"].toString();
    if (hash.length() != 40) {
        qWarning() << "[PeerApi] invalid torrent_response from" << shortId(peerId);
        return;
    }

    // Emit before insertion so callers awaiting a fetch get the payload even when
    // the torrent already exists locally.
    QJsonObject payload = data;
    payload["peer"] = peerId;
    payload["remote"] = true;
    emit remoteTorrentReceived(hash, payload);

    insertFromPeer(data, /*trackReplication*/ true);
}

void PeerApi::handleFeedResponse(const QString& peerId, const QJsonObject& data)
{
    service::FeedService* feed = app_->feed();
    if (!feed)
        return;

    const QJsonArray remoteFeed = data["feed"].toArray();
    const int remoteSize = data["size"].toInt(remoteFeed.size());
    const qint64 remoteFeedDate = data["feedDate"].toVariant().toLongLong();

    const int localSize = feed->size();
    const qint64 localFeedDate = feed->feedDate();

    // Replace when the remote feed is strictly larger, or equal-sized but newer.
    const bool replace = remoteSize > localSize || (remoteSize == localSize && remoteFeedDate > localFeedDate);
    if (!replace)
        return;

    qInfo() << "[PeerApi] replacing feed with" << remoteFeed.size() << "items from" << shortId(peerId);
    feed->fromJsonArray(remoteFeed, remoteFeedDate);

    // Replicate the torrents behind the feed into the index. These deliberately
    // do not count toward replication accounting.
    for (const QJsonValue& v : remoteFeed) {
        if (v.isObject())
            insertFromPeer(v.toObject(), /*trackReplication*/ false);
    }
}

void PeerApi::handleRandomTorrentsResponse(const QString& peerId, const QJsonObject& data)
{
    const QJsonArray torrents = data["torrents"].toArray();
    int inserted = 0;
    for (const QJsonValue& v : torrents) {
        if (v.isObject() && insertFromPeer(v.toObject(), /*trackReplication*/ true))
            ++inserted;
    }
    if (inserted > 0)
        qInfo() << "[PeerApi] replicated" << inserted << "torrents from" << shortId(peerId);
}

void PeerApi::handleTorrentAnnounce(const QString& peerId, const QJsonObject& data)
{
    const QString hash = data["info_hash"].toString();
    const QString name = data["name"].toString();
    if (hash.isEmpty() || name.isEmpty())
        return;

    qDebug() << "[PeerApi] torrent announce from" << shortId(peerId) << ":" << name;
    insertFromPeer(data, /*trackReplication*/ false);
}

// ============================================================================
// Peer lifecycle
// ============================================================================

void PeerApi::onPeerConnected(const QString& peerId)
{
    net::P2PTransport* transport = app_->transport();
    if (!transport)
        return;

    qInfo() << "[PeerApi] peer connected:" << peerId.left(16);

    // Pull the peer's feed for P2P feed sync.
    if (service::FeedService* feed = app_->feed()) {
        transport->sendMessage(
            peerId, "feed", QJsonObject { { "localSize", feed->size() }, { "localFeedDate", feed->feedDate() } });
    }

    // Ask for an initial replication batch when replication is enabled.
    if (app_->config() && app_->config()->p2pReplication())
        transport->sendMessage(peerId, "randomTorrents", QJsonObject { { "limit", 5 } });
}

// ============================================================================
// Insertion (single write path)
// ============================================================================

bool PeerApi::insertFromPeer(const QJsonObject& data, bool trackReplication)
{
    domain::Torrent torrent = domain::codec::torrentFromJson(data);
    if (torrent.hash.length() != 40)
        return false;

    const service::IndexingService::Result result = app_->indexing()->insert(std::move(torrent));
    if (!result.success || result.alreadyExists)
        return false;

    if (trackReplication && app_->replication())
        app_->replication()->notifyReceived();

    return true;
}

} // namespace rats::peer
