#ifndef SEARCHAPI_H
#define SEARCHAPI_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
#include "torrentdatabase.h"

class P2PNetwork;

/**
 * @brief SearchAPI - API layer for torrent search operations
 * 
 * Provides a unified API for:
 * - Local database search
 * - P2P network search
 * - DHT search
 * - Combined results
 * 
 * Can be used by GUI, CLI, REST server, or socket.io
 */
class SearchAPI : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(const QJsonObject& result)>;
    using TorrentsCallback = std::function<void(const QJsonArray& torrents)>;

    explicit SearchAPI(TorrentDatabase* database, P2PNetwork* p2p = nullptr, QObject *parent = nullptr);
    ~SearchAPI();

    // =========================================================================
    // Search Operations
    // =========================================================================

    /**
     * @brief Search torrents by text query
     * @param text Search query
     * @param navigation Search options (limit, offset, sort, filters)
     * @param callback Callback with results
     */
    void searchTorrent(const QString& text, 
                       const QJsonObject& navigation,
                       TorrentsCallback callback);

    /**
     * @brief Search files within torrents
     * @param text Search query
     * @param navigation Search options
     * @param callback Callback with results
     */
    void searchFiles(const QString& text,
                     const QJsonObject& navigation, 
                     TorrentsCallback callback);

    /**
     * @brief Get torrent by hash
     * @param hash Info hash (40 char hex)
     * @param includeFiles Whether to include file list
     * @param callback Callback with torrent data
     */
    void getTorrent(const QString& hash, 
                    bool includeFiles,
                    Callback callback);

    // =========================================================================
    // Browse Operations
    // =========================================================================

    /**
     * @brief Get recent torrents
     * @param limit Number of torrents to return
     * @param callback Callback with results
     */
    void getRecentTorrents(int limit, TorrentsCallback callback);

    /**
     * @brief Get top torrents by seeders
     * @param type Content type filter (optional)
     * @param navigation Navigation options (limit, offset, time)
     * @param callback Callback with results
     */
    void getTopTorrents(const QString& type,
                        const QJsonObject& navigation,
                        TorrentsCallback callback);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get database statistics
     * @param callback Callback with stats
     */
    void getStatistics(Callback callback);

    /**
     * @brief Get P2P network peers info
     * @param callback Callback with peers info
     */
    void getPeers(Callback callback);

    // =========================================================================
    // Torrent Operations
    // =========================================================================

    /**
     * @brief Check tracker info for torrent
     * @param hash Info hash
     */
    void checkTrackers(const QString& hash);

    /**
     * @brief Vote on a torrent (good/bad)
     * @param hash Info hash
     * @param isGood true for good vote, false for bad
     * @param callback Callback with vote result
     */
    void vote(const QString& hash, bool isGood, Callback callback);

    // =========================================================================
    // Helpers
    // =========================================================================

    /**
     * @brief Convert TorrentInfo to JSON
     */
    static QJsonObject torrentToJson(const TorrentInfo& torrent);

    /**
     * @brief Convert search options from JSON
     */
    static SearchOptions jsonToSearchOptions(const QString& query, const QJsonObject& navigation);

signals:
    void remoteSearchResults(const QJsonArray& torrents, const QString& searchId);
    void trackerUpdate(const QString& hash, int seeders, int leechers, int completed);
    void votesUpdated(const QString& hash, int good, int bad);

private:
    TorrentDatabase* database_;
    P2PNetwork* p2p_;
};

#endif // SEARCHAPI_H

