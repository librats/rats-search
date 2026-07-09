#include "rest/api_router.h"

#include "app/application.h"
#include "app/config_store.h"
#include "common/infohash.h"
#include "data/torrent_repository.h"
#include "domain/peer.h"
#include "domain/torrent_codec.h"
#include "net/librats_convert.h"
#include "net/p2p_transport.h"
#include "net/torrent_engine.h"
#include "peer/peer_api.h"
#include "services/download_service.h"
#include "services/feed_service.h"
#include "services/indexing_service.h"
#include "services/peer_registry.h"
#include "services/search_service.h"
#include "services/torrent_creator.h"
#include "services/tracker_service.h"
#include "services/update_service.h"
#include "services/voting_service.h"

// The DHT metadata fallback reaches straight into the librats BitTorrent
// subsystem, whose headers expose an EventBus emit() that collides with Qt's
// `emit` keyword macro. Neutralise the macros across the librats includes, then
// restore them so the router's own `emit event(...)` keeps compiling.
#ifdef RATS_SEARCH_FEATURES
#pragma push_macro("emit")
#pragma push_macro("slots")
#pragma push_macro("signals")
#undef emit
#undef slots
#undef signals
#include "bittorrent/torrent_info.h"
#include "subsystems/bittorrent.h"
#pragma pop_macro("signals")
#pragma pop_macro("slots")
#pragma pop_macro("emit")
#endif

#include <QDateTime>
#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>
#include <memory>

namespace rats::rest {

namespace {

// --- Parameter helpers ------------------------------------------------------

// Query text may arrive as a JSON string or, when a numeric-looking query like
// "1987" is parsed by the HTTP layer, as a JSON number — toVariant().toString()
// handles both. Accepts either "text" or "query".
QString queryText(const QJsonObject& params)
{
    if (params.contains("text"))
        return params["text"].toVariant().toString();
    return params["query"].toVariant().toString();
}

QStringList jsonToStringList(const QJsonValue& value)
{
    QStringList list;
    for (const QJsonValue& v : value.toArray())
        list.append(v.toString());
    return list;
}

service::SearchService::Request buildSearchRequest(const QJsonObject& params)
{
    service::SearchService::Request req;
    req.query = queryText(params);
    req.offset = params.contains("offset") ? params["offset"].toInt(0) : params["index"].toInt(0);
    req.limit = params["limit"].toInt(10);
    req.sort = params.contains("sort") ? params["sort"].toString() : params["orderBy"].toString();
    req.descending = params["orderDesc"].toBool(true);
    req.safeSearch = params["safeSearch"].toBool(false);
    req.contentType = params.contains("contentType") ? params["contentType"].toString() : params["type"].toString();

    const QJsonObject size = params["size"].toObject();
    if (!size.isEmpty()) {
        req.sizeMin = size["min"].toVariant().toLongLong();
        req.sizeMax = size["max"].toVariant().toLongLong();
    }
    const QJsonObject files = params["files"].toObject();
    if (!files.isEmpty()) {
        req.filesMin = files["min"].toInt();
        req.filesMax = files["max"].toInt();
    }
    return req;
}

// --- DHT metadata fallback --------------------------------------------------

// Serialise a stored/fetched torrent for a DHT hit and tag it `fromDHT`.
QJsonObject dhtTorrentJson(const domain::Torrent& torrent, bool includeFiles)
{
    QJsonObject obj = domain::codec::toJson(torrent, { includeFiles });
    obj["fromDHT"] = true;
    return obj;
}

// Ask librats to fetch a torrent's metadata over the DHT/BEP 9, index the
// result, and answer `respond` exactly once. `asArray` shapes the payload:
// search.torrents wants an array (empty on miss), torrent.get wants an object
// (failure on miss). Always marshals back onto the router's thread so the index
// insert and the response happen there. Both search.torrents and torrent.get
// share this one async source-combine.
void dhtLookup(ApiRouter* ctx, app::Application* app, const QString& hash, bool includeFiles, bool asArray,
    const ResultCallback& respond)
{
    auto miss = [asArray, respond]() {
        if (asArray)
            respond(Result::success(QJsonArray()));
        else
            respond(Result::failure("Torrent not found"));
    };

#ifdef RATS_SEARCH_FEATURES
    net::P2PTransport* transport = app->transport();
    librats::Bittorrent* bt = transport ? transport->bittorrent() : nullptr;
    if (!transport || !transport->isBitTorrentEnabled() || !bt || !bt->is_running()) {
        miss();
        return;
    }

    bt->get_torrent_metadata(hash.toStdString(),
        [ctx, app, hash, includeFiles, asArray, respond, miss](
            const librats::bittorrent::TorrentInfo& info, bool success, const std::string& error) {
            Q_UNUSED(error);
            if (!success || !info.is_valid()) {
                QMetaObject::invokeMethod(ctx, [miss]() { miss(); }, Qt::QueuedConnection);
                return;
            }

            // Build the domain entity on this worker thread, then hop to the
            // router thread to touch the index and reply.
            domain::Torrent torrent = net::toDomainTorrent(hash, info);
            QMetaObject::invokeMethod(
                ctx,
                [app, torrent, includeFiles, asArray, respond]() {
                    service::IndexingService::Result inserted = app->indexing()->insert(torrent);
                    const domain::Torrent& stored
                        = (inserted.success || inserted.alreadyExists) ? inserted.torrent : torrent;
                    QJsonObject obj = dhtTorrentJson(stored, includeFiles);
                    if (asArray) {
                        QJsonArray arr;
                        arr.append(obj);
                        respond(Result::success(arr));
                    } else {
                        respond(Result::success(obj));
                    }
                },
                Qt::QueuedConnection);
        });
#else
    Q_UNUSED(ctx);
    Q_UNUSED(app);
    Q_UNUSED(hash);
    Q_UNUSED(includeFiles);
    miss();
#endif
}

} // namespace

// ===========================================================================
// ApiRouter
// ===========================================================================

ApiRouter::ApiRouter(app::Application* app, QObject* parent) : QObject(parent), app_(app)
{
    registerMethods();
    wireEvents();
}

void ApiRouter::wireEvents()
{
    // Bridge service-layer Qt signals to the unified WS event channel. The
    // ApiServer forwards every `event(name, data)` to all connected WebSocket
    // clients, so this restores the live push stream (download progress, votes,
    // feed, config, indexing, remote search) that the desktop GUI already gets
    // through direct signal connections. Event names and payload shapes are a
    // published contract — changing one breaks existing API clients.
    connect(app_->downloads(), &service::DownloadService::progressUpdated, this,
        [this](const QString& hash, const QJsonObject& progress) {
            QJsonObject data = progress;
            data["hash"] = hash;
            emit event(QStringLiteral("downloadProgress"), data);
        });
    connect(app_->downloads(), &service::DownloadService::downloadCompleted, this, [this](const QString& hash) {
        emit event(QStringLiteral("downloadCompleted"), QJsonObject { { "hash", hash }, { "cancelled", false } });
    });
    connect(app_->downloads(), &service::DownloadService::filesReady, this,
        [this](const QString& hash, const QJsonArray& files) {
            emit event(QStringLiteral("filesReady"), QJsonObject { { "hash", hash }, { "files", files } });
        });
    connect(app_->indexing(), &service::IndexingService::torrentIndexed, this, [this](const domain::Torrent& torrent) {
        emit event(
            QStringLiteral("torrentIndexed"), QJsonObject { { "hash", torrent.hash }, { "name", torrent.name } });
    });
    connect(
        app_->voting(), &service::VotingService::votesUpdated, this, [this](const QString& hash, int good, int bad) {
            emit event(
                QStringLiteral("votesUpdated"), QJsonObject { { "hash", hash }, { "good", good }, { "bad", bad } });
        });
    connect(app_->feed(), &service::FeedService::feedUpdated, this, [this]() {
        emit event(QStringLiteral("feedUpdated"), QJsonObject { { "feed", app_->feed()->toJsonArray() } });
    });
    connect(app_->config(), &app::ConfigStore::configChanged, this, [this](const QStringList& changedKeys) {
        QJsonObject data = app_->config()->toJson();
        data["changedKeys"] = QJsonArray::fromStringList(changedKeys);
        emit event(QStringLiteral("configChanged"), data);
    });
    if (app_->peerApi()) {
        connect(app_->peerApi(), &peer::PeerApi::remoteSearchResults, this,
            [this](const QString& query, const QJsonArray& torrents) {
                emit event(QStringLiteral("remoteSearchResults"),
                    QJsonObject { { "searchId", query }, { "torrents", torrents } });
            });
    }
}

void ApiRouter::add(const QString& name, Handler handler)
{
    handlers_.insert(name, std::move(handler));
}

void ApiRouter::call(const QString& method, const QJsonObject& params, const ResultCallback& respond)
{
    auto it = handlers_.constFind(method);
    if (it == handlers_.constEnd()) {
        respond(Result::failure(QStringLiteral("Unknown method: %1").arg(method), QStringLiteral("unknown_method")));
        return;
    }
    it.value()(params, respond);
}

QStringList ApiRouter::methods() const
{
    return handlers_.keys();
}

void ApiRouter::registerMethods()
{
    // -----------------------------------------------------------------------
    // Search
    // -----------------------------------------------------------------------
    add("search.torrents", [this](const QJsonObject& params, const ResultCallback& respond) {
        const service::SearchService::Request req = buildSearchRequest(params);
        const QVector<domain::SearchHit> hits = app_->search()->searchTorrents(req);

        QJsonArray results;
        for (const domain::SearchHit& hit : hits)
            results.append(domain::codec::toJson(hit));

        // An info-hash query (bare hash or magnet link) with no local hit falls
        // back to a DHT lookup on the extracted, normalized hash.
        const QString dhtHash = service::SearchService::extractInfoHash(req.query);
        if (hits.isEmpty() && !dhtHash.isEmpty()) {
            dhtLookup(this, app_, dhtHash, /*includeFiles*/ false, /*asArray*/ true, respond);
            return;
        }
        respond(Result::success(results));
    });

    add("search.files", [this](const QJsonObject& params, const ResultCallback& respond) {
        const service::SearchService::Request req = buildSearchRequest(params);
        if (req.query.length() <= 2) {
            respond(Result::failure("Query too short"));
            return;
        }
        const QVector<domain::SearchHit> hits = app_->search()->searchFiles(req);

        QJsonArray results;
        for (const domain::SearchHit& hit : hits)
            results.append(domain::codec::toJson(hit, { /*includeFiles*/ true }));
        respond(Result::success(results));
    });

    add("search.top", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QString type = params["type"].toString();
        const QString time = params["time"].toString();
        const int offset = params["index"].toInt(0);
        const int limit = params["limit"].toInt(20);
        const QVector<domain::Torrent> torrents = app_->search()->top(type, time, offset, limit);

        QJsonArray results;
        for (const domain::Torrent& t : torrents)
            results.append(domain::codec::toJson(t));
        respond(Result::success(results));
    });

    add("search.recent", [this](const QJsonObject& params, const ResultCallback& respond) {
        const int limit = params["limit"].toInt(10);
        const QVector<domain::Torrent> torrents = app_->search()->recent(limit);

        QJsonArray results;
        for (const domain::Torrent& t : torrents)
            results.append(domain::codec::toJson(t));
        respond(Result::success(results));
    });

    // -----------------------------------------------------------------------
    // Torrent lifecycle
    // -----------------------------------------------------------------------
    add("torrent.get", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QString hash = infohash::normalize(params["hash"].toString());
        if (!infohash::isValid(hash)) {
            respond(Result::failure("Invalid hash"));
            return;
        }
        const bool includeFiles = params["files"].toBool(false);

        std::optional<domain::Torrent> torrent = app_->search()->get(hash, includeFiles);
        if (!torrent) {
            // Not indexed locally: try to pull metadata from the DHT.
            dhtLookup(this, app_, hash, includeFiles, /*asArray*/ false, respond);
            return;
        }

        QJsonObject obj = domain::codec::toJson(*torrent, { includeFiles });
        if (app_->downloads()->isDownloading(hash))
            obj["download"] = app_->downloads()->getDownload(hash).toJson();
        respond(Result::success(obj));
    });

    add("torrent.remove", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QJsonArray hashes = params["hashes"].toArray();
        if (hashes.isEmpty()) {
            respond(Result::failure("No hashes provided"));
            return;
        }

        const int total = hashes.size();
        int removed = 0;
        int processed = 0;
        for (const QJsonValue& value : hashes) {
            const QString hash = infohash::normalize(value.toString());
            ++processed;
            if (infohash::isValid(hash) && app_->torrents()->remove(hash))
                ++removed;

            if (processed % 100 == 0 || processed == total) {
                QJsonObject progress;
                progress["processed"] = processed;
                progress["removed"] = removed;
                progress["total"] = total;
                emit event("torrent.remove.progress", progress);
            }
        }

        QJsonObject result;
        result["total"] = total;
        result["removed"] = removed;
        respond(Result::success(result));
    });

    // Re-apply the current filter policy across the whole index and remove
    // torrents that no longer pass (e.g. after tightening the adult/size
    // filters). With dryRun=true it only counts; otherwise it removes and emits
    // progress.
    add("torrent.cleanup", [this](const QJsonObject& params, const ResultCallback& respond) {
        const bool dryRun = params["dryRun"].toBool(false);
        constexpr int kBatch = 500;
        const qint64 total = app_->torrents()->statistics().torrents;
        int scanned = 0;
        int matched = 0;
        for (int offset = 0;; offset += kBatch) {
            const QVector<domain::Torrent> batch = app_->torrents()->page(offset, kBatch);
            if (batch.isEmpty())
                break;
            for (const domain::Torrent& t : batch) {
                ++scanned;
                if (!app_->indexing()->accepts(t)) {
                    ++matched;
                    if (!dryRun)
                        app_->torrents()->remove(t.hash);
                }
            }
            emit event("torrent.cleanup.progress",
                QJsonObject {
                    { "scanned", scanned }, { "matched", matched }, { "total", static_cast<double>(total) } });
        }
        respond(Result::success(QJsonObject { { "dryRun", dryRun }, { "scanned", scanned }, { "matched", matched },
            { "removed", dryRun ? 0 : matched } }));
    });

    add("torrent.create", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QString path = params["path"].toString();
        if (path.isEmpty()) {
            respond(Result::failure("Empty path"));
            return;
        }
        const QStringList trackers = jsonToStringList(params["trackers"]);
        const QString comment = params["comment"].toString();
        const bool seed = params["seed"].toBool(false);

        if (seed) {
            const QString output = params["output"].toString();
            const QString hash = app_->creator()->createAndSeed(path, trackers, comment, output);
            if (hash.isEmpty()) {
                respond(Result::failure("Failed to create torrent"));
                return;
            }
            QJsonObject result;
            result["hash"] = hash;
            result["seeding"] = true;
            respond(Result::success(result));
            return;
        }

        const QString output = params["output"].toString();
        if (output.isEmpty()) {
            respond(Result::failure("Output path required"));
            return;
        }
        if (!app_->creator()->createTorrentFile(path, output, trackers, comment)) {
            respond(Result::failure("Failed to create torrent file"));
            return;
        }
        QJsonObject result;
        result["file"] = output;
        respond(Result::success(result));
    });

    add("torrent.import", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QString file = params.contains("path") ? params["path"].toString() : params["file"].toString();
        if (file.isEmpty()) {
            respond(Result::failure("Empty file path"));
            return;
        }

        const net::TorrentMetadata meta = app_->engine()->readTorrentFile(file);
        if (!meta.valid || !infohash::isValid(meta.hash)) {
            respond(Result::failure("Failed to parse torrent file"));
            return;
        }

        domain::Torrent torrent;
        torrent.hash = infohash::normalize(meta.hash);
        torrent.name = meta.name;
        torrent.size = meta.totalSize;
        torrent.files = meta.files.size();
        torrent.added = QDateTime::currentDateTime();
        for (const net::EngineFile& f : meta.files)
            torrent.fileList.append(domain::File { f.path, f.size });

        const service::IndexingService::Result inserted = app_->indexing()->insert(torrent);
        if (!inserted.success && !inserted.alreadyExists) {
            respond(Result::failure(
                inserted.error.isEmpty() ? QStringLiteral("Failed to import torrent") : inserted.error));
            return;
        }

        QJsonObject result = domain::codec::toJson(inserted.torrent, { /*includeFiles*/ true });
        result["alreadyExists"] = inserted.alreadyExists;
        result["imported"] = !inserted.alreadyExists;
        respond(Result::success(result));
    });

    // -----------------------------------------------------------------------
    // Downloads
    // -----------------------------------------------------------------------
    add("download.add", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QString link = params.contains("hash") ? params["hash"].toString() : params["magnet"].toString();
        const QString savePath = params["savePath"].toString();
        const QString hash = net::TorrentEngine::parseInfoHash(link);
        if (!infohash::isValid(hash)) {
            respond(Result::failure("Invalid hash"));
            return;
        }

        // Reuse indexed metadata (name/size) when we already know the torrent.
        std::optional<domain::Torrent> known = app_->search()->get(hash, false);
        const bool ok
            = known ? app_->downloads()->addWithInfo(*known, savePath) : app_->downloads()->add(link, savePath);
        respond(ok ? Result::success() : Result::failure("Failed to start download"));
    });

    add("download.addFile", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QString file = params.contains("path") ? params["path"].toString() : params["file"].toString();
        const QString savePath = params["savePath"].toString();
        if (file.isEmpty()) {
            respond(Result::failure("Empty file path"));
            return;
        }
        const bool ok = app_->downloads()->addFromFile(file, savePath);
        respond(ok ? Result::success() : Result::failure("Failed to add torrent file"));
    });

    add("download.pause", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QString hash = infohash::normalize(params["hash"].toString());
        const bool ok = app_->downloads()->pause(hash);
        respond(ok ? Result::success() : Result::failure("Download not found"));
    });

    add("download.resume", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QString hash = infohash::normalize(params["hash"].toString());
        const bool ok = app_->downloads()->resume(hash);
        respond(ok ? Result::success() : Result::failure("Download not found"));
    });

    add("download.remove", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QString hash = infohash::normalize(params["hash"].toString());
        if (!app_->downloads()->isDownloading(hash)) {
            respond(Result::failure("Download not found"));
            return;
        }
        const bool saveResumeData = params["saveResumeData"].toBool(false);
        app_->downloads()->remove(hash, saveResumeData);
        respond(Result::success());
    });

    add("download.list", [this](const QJsonObject& /*params*/, const ResultCallback& respond) {
        respond(Result::success(app_->downloads()->toJsonArray()));
    });

    add("download.selectFiles", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QString hash = infohash::normalize(params["hash"].toString());
        const bool ok = app_->downloads()->selectFilesJson(hash, params["files"]);
        respond(ok ? Result::success() : Result::failure("Failed to select files"));
    });

    // -----------------------------------------------------------------------
    // Feed
    // -----------------------------------------------------------------------
    add("feed.get", [this](const QJsonObject& params, const ResultCallback& respond) {
        const int index = params["index"].toInt(0);
        const int limit = params["limit"].toInt(20);
        respond(Result::success(app_->feed()->toJsonArray(index, limit)));
    });

    // -----------------------------------------------------------------------
    // Voting (async: the distributed store answers via callback)
    // -----------------------------------------------------------------------
    add("vote.cast", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QString hash = infohash::normalize(params["hash"].toString());
        if (!infohash::isValid(hash)) {
            respond(Result::failure("Invalid hash"));
            return;
        }
        const bool good = params.contains("good") ? params["good"].toBool(true) : params["isGood"].toBool(true);
        app_->voting()->vote(hash, good, [respond](bool ok, const QJsonObject& result, const QString& error) {
            respond(ok ? Result::success(result) : Result::failure(error));
        });
    });

    add("vote.get", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QString hash = infohash::normalize(params["hash"].toString());
        if (!infohash::isValid(hash)) {
            respond(Result::failure("Invalid hash"));
            return;
        }
        app_->voting()->getVotes(hash, [respond](bool ok, const QJsonObject& result, const QString& error) {
            respond(ok ? Result::success(result) : Result::failure(error));
        });
    });

    // -----------------------------------------------------------------------
    // Config
    // -----------------------------------------------------------------------
    add("config.get", [this](const QJsonObject& /*params*/, const ResultCallback& respond) {
        respond(Result::success(app_->config()->toJson()));
    });

    add("config.set", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QStringList changed = app_->config()->fromJson(params);
        QJsonObject result;
        result["changed"] = QJsonArray::fromStringList(changed);
        respond(Result::success(result));
    });

    // -----------------------------------------------------------------------
    // Statistics & peers
    // -----------------------------------------------------------------------
    add("stats.get", [this](const QJsonObject& /*params*/, const ResultCallback& respond) {
        const data::TorrentRepository::Statistics stats = app_->torrents()->statistics();
        QJsonObject result;
        result["torrents"] = stats.torrents;
        result["files"] = stats.files;
        result["size"] = stats.totalSize;
        result["peers"] = app_->transport() ? app_->transport()->peerCount() : 0;
        respond(Result::success(result));
    });

    add("peers.list", [this](const QJsonObject& /*params*/, const ResultCallback& respond) {
        const QHash<QString, domain::PeerStats> peers = app_->peers()->connectedPeers();
        QJsonArray result;
        for (auto it = peers.constBegin(); it != peers.constEnd(); ++it) {
            QJsonObject peer = it.value().toJson();
            peer["peerId"] = it.key();
            result.append(peer);
        }
        respond(Result::success(result));
    });

    // -----------------------------------------------------------------------
    // Trackers (fire-and-forget scrape; results flow back into the index)
    // -----------------------------------------------------------------------
    add("tracker.check", [this](const QJsonObject& params, const ResultCallback& respond) {
        const QString hash = infohash::normalize(params["hash"].toString());
        if (!infohash::isValid(hash)) {
            respond(Result::failure("Invalid hash"));
            return;
        }
        app_->trackers()->checkCounts(hash);
        QJsonObject result;
        result["hash"] = hash;
        result["status"] = "checking";
        respond(Result::success(result));
    });

    // -----------------------------------------------------------------------
    // Updates (async: GitHub release check answers via signals)
    // -----------------------------------------------------------------------
    add("update.check", [this](const QJsonObject& /*params*/, const ResultCallback& respond) {
        service::UpdateService* svc = app_->updates();

        // First of the three terminal signals wins; it disconnects the rest so
        // `respond` runs exactly once.
        auto guard = std::make_shared<bool>(false);
        auto connections = std::make_shared<QVector<QMetaObject::Connection>>();
        auto finish = [guard, connections, respond](const Result& result) {
            if (*guard)
                return;
            *guard = true;
            for (const QMetaObject::Connection& c : *connections)
                QObject::disconnect(c);
            respond(result);
        };

        connections->append(connect(svc, &service::UpdateService::updateAvailable, this,
            [finish](const service::UpdateService::UpdateInfo& info) {
                QJsonObject result;
                result["available"] = true;
                result["version"] = info.version;
                result["downloadUrl"] = info.downloadUrl;
                result["releaseNotes"] = info.releaseNotes;
                result["downloadSize"] = info.downloadSize;
                result["publishedAt"] = info.publishedAt;
                result["prerelease"] = info.isPrerelease;
                finish(Result::success(result));
            }));
        connections->append(connect(svc, &service::UpdateService::noUpdateAvailable, this, [finish]() {
            QJsonObject result;
            result["available"] = false;
            finish(Result::success(result));
        }));
        connections->append(connect(svc, &service::UpdateService::errorOccurred, this,
            [finish](const QString& error) { finish(Result::failure(error)); }));

        svc->checkForUpdates();
    });
}

} // namespace rats::rest
