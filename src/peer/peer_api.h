#ifndef RATS_PEER_PEER_API_H
#define RATS_PEER_PEER_API_H

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace rats::app {
class Application;
}

namespace rats::peer {

// The P2P front-end. It answers other peers' requests (search / files / top /
// torrent / feed / randomTorrents) by running local service queries and
// replying over the transport, and it processes their responses by funnelling
// every received torrent through the one IndexingService insertion path — never
// touching the database directly.
//
// Wire message-type names are preserved verbatim so already-deployed peers keep
// interoperating. The client_info handshake is deliberately NOT handled here:
// it lives in PeerRegistry. On connect this only drives the follow-up work the
// old handleP2PPeerConnected did on top of the handshake (feed + replication
// pull).
//
// Business logic stays in the services; PeerApi only marshals between the wire
// (QJsonObject) and the domain (via domain::codec) and emits Qt signals so the
// UI / REST layer can observe remote activity.
class PeerApi : public QObject {
    Q_OBJECT

public:
    explicit PeerApi(app::Application* app, QObject* parent = nullptr);

    // Register every transport handler and the peer-connected hook. Called from
    // the constructor; exposed so it can be re-run if the transport restarts.
    void install();

signals:
    // A remote peer sent search hits (torrent_search_result). Query is empty —
    // the wire protocol never echoes it back — and torrents is the raw wire array
    // with remote/peer provenance stamped on.
    void remoteSearchResults(const QString& query, const QJsonArray& torrents);
    // A remote peer sent a file-search hit (searchFiles_result).
    void remoteFileSearchResults(const QString& query, const QJsonArray& torrents);
    // A remote peer answered a single-torrent request (torrent_response). Emitted
    // even when the torrent already exists locally, so callers awaiting a fetch
    // still receive the payload.
    void remoteTorrentReceived(const QString& hash, const QJsonObject& data);

private:
    // Request handlers (we answer these) ---------------------------------------
    void handleSearchRequest(const QString& peerId, const QJsonObject& data);
    void handleSearchFilesRequest(const QString& peerId, const QJsonObject& data);
    void handleTopTorrentsRequest(const QString& peerId, const QJsonObject& data);
    void handleTorrentRequest(const QString& peerId, const QJsonObject& data);
    void handleFeedRequest(const QString& peerId, const QJsonObject& data);
    void handleRandomTorrentsRequest(const QString& peerId, const QJsonObject& data);

    // Response handlers (we consume these) -------------------------------------
    void handleSearchResult(const QString& peerId, const QJsonObject& data);
    void handleSearchFilesResult(const QString& peerId, const QJsonObject& data);
    void handleTorrentResponse(const QString& peerId, const QJsonObject& data);
    void handleFeedResponse(const QString& peerId, const QJsonObject& data);
    void handleRandomTorrentsResponse(const QString& peerId, const QJsonObject& data);
    void handleTorrentAnnounce(const QString& peerId, const QJsonObject& data);

    // Peer lifecycle -----------------------------------------------------------
    void onPeerConnected(const QString& peerId);

    // Insert one received torrent through IndexingService (the single write
    // path). Returns true only when a genuinely new torrent was stored. When
    // trackReplication is set, a new insert also pings ReplicationService.
    bool insertFromPeer(const QJsonObject& data, bool trackReplication);

    app::Application* app_;
};

} // namespace rats::peer

#endif // RATS_PEER_PEER_API_H
