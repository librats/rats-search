#include "app/application.h"

#include "app/config_store.h"
#include "app/favorites_store.h"
#include "app/translation_manager.h"
#include "data/database.h"
#include "data/feed_repository.h"
#include "data/manticore.h"
#include "data/torrent_repository.h"
#include "domain/peer.h"
#include "domain/torrent.h"
#include "net/crawler.h"
#include "net/p2p_transport.h"
#include "net/torrent_engine.h"
#include "net/tracker_info_scraper.h"
#include "net/tracker_scraper.h"
#include "peer/peer_api.h"
#include "rest/api_router.h"
#include "rest/api_server.h"
#include "services/download_service.h"
#include "services/feed_service.h"
#include "services/filter_policy.h"
#include "services/indexing_service.h"
#include "services/migration_service.h"
#include "services/p2p_store.h"
#include "services/peer_registry.h"
#include "services/replication_service.h"
#include "services/search_service.h"
#include "services/torrent_creator.h"
#include "services/torrent_exporter.h"
#include "services/tracker_service.h"
#include "services/update_service.h"
#include "services/voting_service.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>

namespace rats::app {

using namespace rats;

struct Application::Private {
    Application::Options options;
    bool running = false;

    // Adapters (owned, constructed in dependency order)
    std::unique_ptr<ConfigStore> config;
    std::unique_ptr<FavoritesStore> favorites;
    std::unique_ptr<data::Manticore> manticore;
    std::unique_ptr<data::Database> database;
    std::unique_ptr<data::TorrentRepository> torrents;
    std::unique_ptr<data::FeedRepository> feedRepo;
    std::unique_ptr<net::P2PTransport> transport;
    std::unique_ptr<net::TorrentEngine> engine;
    std::unique_ptr<net::Crawler> crawler;
    std::unique_ptr<net::TrackerScraper> trackerScraper;
    std::unique_ptr<net::TrackerInfoScraper> trackerInfoScraper;

    // Services
    std::unique_ptr<service::FilterPolicy> filter;
    std::unique_ptr<service::IndexingService> indexing;
    std::unique_ptr<service::SearchService> search;
    std::unique_ptr<service::PeerRegistry> peers;
    std::unique_ptr<service::DownloadService> downloads;
    std::unique_ptr<service::TorrentCreator> creator;
    std::unique_ptr<service::TorrentExporter> exporter;
    std::unique_ptr<service::FeedService> feed;
    std::unique_ptr<service::P2PStore> p2pStore;
    std::unique_ptr<service::VotingService> voting;
    std::unique_ptr<service::ReplicationService> replication;
    std::unique_ptr<service::TrackerService> trackers;
    std::unique_ptr<service::MigrationService> migrations;
    std::unique_ptr<service::UpdateService> updates;

    // Front-ends
    std::unique_ptr<rest::ApiRouter> apiRouter;
    std::unique_ptr<peer::PeerApi> peerApi;
    std::unique_ptr<rest::ApiServer> apiServer;
};

Application::Application(Options options, QObject* parent) : QObject(parent), d_(std::make_unique<Private>())
{
    d_->options = std::move(options);

    // Metatypes for cross-thread queued signals carrying our value types.
    qRegisterMetaType<domain::Torrent>("rats::domain::Torrent");
    qRegisterMetaType<domain::SearchHit>("rats::domain::SearchHit");

    const QString dataDir = d_->options.dataDirectory;
    auto* cfg = new ConfigStore(dataDir + QStringLiteral("/rats.json"), this);
    d_->config.reset(cfg);
    d_->config->load();

    // Translators must be installed before any front-end builds its UI, so the
    // first tr() call already resolves in the user's language.
    TranslationManager& translations = TranslationManager::instance();
    translations.initialize(QCoreApplication::instance(), dataDir + QStringLiteral("/translations"));
    QString language = cfg->language();
    if (language.isEmpty())
        language = TranslationManager::systemLanguage();
    translations.setLanguage(language);

    d_->favorites = std::make_unique<FavoritesStore>(dataDir);

    // --- Data layer -------------------------------------------------------
    d_->manticore = std::make_unique<data::Manticore>(dataDir);
    d_->database = std::make_unique<data::Database>(d_->manticore.get());
    d_->torrents = std::make_unique<data::TorrentRepository>(d_->database.get());
    d_->feedRepo = std::make_unique<data::FeedRepository>(d_->database.get());

    // --- Net layer --------------------------------------------------------
    const int p2pPort = d_->options.p2pPort > 0 ? d_->options.p2pPort : cfg->p2pPort();
    const int dhtPort = d_->options.dhtPort > 0 ? d_->options.dhtPort : cfg->dhtPort();
    const int maxPeers = d_->options.maxPeers > 0 ? d_->options.maxPeers : cfg->p2pConnections();
    d_->transport = std::make_unique<net::P2PTransport>(p2pPort, dhtPort, dataDir, maxPeers);
    d_->engine = std::make_unique<net::TorrentEngine>(d_->transport.get());
    d_->crawler = std::make_unique<net::Crawler>(d_->transport.get());
    d_->trackerScraper = std::make_unique<net::TrackerScraper>();
    d_->trackerInfoScraper = std::make_unique<net::TrackerInfoScraper>();

    // --- Services ---------------------------------------------------------
    d_->filter = std::make_unique<service::FilterPolicy>();
    d_->indexing = std::make_unique<service::IndexingService>(d_->torrents.get(), d_->filter.get());
    d_->search = std::make_unique<service::SearchService>(d_->torrents.get());
    d_->peers = std::make_unique<service::PeerRegistry>(d_->transport.get(), d_->options.clientVersion);
    d_->downloads = std::make_unique<service::DownloadService>(d_->engine.get());
    d_->creator = std::make_unique<service::TorrentCreator>(d_->engine.get(), d_->downloads.get());
    d_->exporter = std::make_unique<service::TorrentExporter>(d_->engine.get(), dataDir, this);
    d_->feed = std::make_unique<service::FeedService>(d_->feedRepo.get(), d_->torrents.get());
    d_->p2pStore = std::make_unique<service::P2PStore>(d_->transport.get());
    d_->voting = std::make_unique<service::VotingService>(d_->p2pStore.get(), d_->torrents.get());
    d_->replication = std::make_unique<service::ReplicationService>(d_->transport.get());
    d_->trackers = std::make_unique<service::TrackerService>(
        d_->trackerScraper.get(), d_->trackerInfoScraper.get(), d_->torrents.get());
    d_->migrations = std::make_unique<service::MigrationService>(
        dataDir, d_->database.get(), d_->torrents.get(), d_->config.get());
    d_->updates = std::make_unique<service::UpdateService>();

    // --- Front-ends -------------------------------------------------------
    // PeerApi is built before ApiRouter: the router's wireEvents() connects to
    // PeerApi::remoteSearchResults during construction, so PeerApi must exist.
    d_->peerApi = std::make_unique<peer::PeerApi>(this); // registers P2P handlers
    d_->apiRouter = std::make_unique<rest::ApiRouter>(this);
    d_->apiServer = std::make_unique<rest::ApiServer>(d_->apiRouter.get());

    wireSignals();
    applyConfig();
}

Application::~Application()
{
    stop();
}

void Application::applyConfig()
{
    ConfigStore* c = d_->config.get();

    service::FilterSettings fs;
    fs.maxFiles = c->filtersMaxFiles();
    fs.sizeMin = c->filtersSizeMin();
    fs.sizeMax = c->filtersSizeMax();
    fs.adultFilter = c->filtersAdultFilter();
    fs.namingRegExp = c->filtersNamingRegExp();
    fs.namingRegExpNegative = c->filtersNamingRegExpNegative();
    fs.contentTypeFilter = c->filtersContentType();
    d_->filter->setSettings(fs);

    d_->downloads->setDefaultDownloadPath(c->downloadPath());
    d_->trackers->setCountScrapingEnabled(c->trackersEnabled());
    d_->trackers->setInfoScrapingEnabled(c->trackersEnabled());
    d_->replication->setEnabled(c->p2pReplication());
    d_->transport->setPortMappingEnabled(c->upnpEnabled());
    d_->crawler->setWalkInterval(c->spiderWalkInterval());

    // Reflect runtime toggles of the crawler and replication. Only act once the
    // subsystems are up (applyConfig also runs at construction, before start(),
    // where start() itself does the initial launch). start()/stop() are
    // idempotent.
    if (d_->running) {
        if (c->indexerEnabled() || d_->options.forceSpider)
            d_->crawler->start();
        else
            d_->crawler->stop();

        if (c->p2pReplication())
            d_->replication->start();
        else
            d_->replication->stop();
    }
}

void Application::wireSignals()
{
    // Newly crawled torrents flow into the single insertion path.
    connect(d_->crawler.get(), &net::Crawler::discovered, d_->indexing.get(),
        [this](const domain::Torrent& t) { d_->indexing->insert(t); });

    // Let the crawler skip a metadata fetch for torrents already in the index.
    d_->crawler->setKnownHashFilter([this](const QString& hash) { return d_->torrents->exists(hash); });

    // A freshly indexed torrent triggers tracker scrapes.
    connect(d_->indexing.get(), &service::IndexingService::torrentIndexed, d_->trackers.get(),
        &service::TrackerService::onTorrentIndexed);

    // Keep our advertised peer stats in sync with the database totals.
    connect(d_->torrents.get(), &data::TorrentRepository::statisticsChanged, d_->peers.get(),
        &service::PeerRegistry::updateOurStats);

    // A vote surfaces the torrent into the feed.
    connect(d_->voting.get(), &service::VotingService::votesUpdated, d_->feed.get(),
        [this](const QString& hash, int, int) { d_->feed->addByHash(hash); });

    // Re-push config into the services whenever it changes.
    connect(d_->config.get(), &ConfigStore::configChanged, this, [this](const QStringList&) { applyConfig(); });

    // A language switch in the settings dialog swaps the installed translators.
    connect(d_->config.get(), &ConfigStore::languageChanged, this,
        [](const QString& code) { TranslationManager::instance().setLanguage(code); });
}

bool Application::start()
{
    if (d_->running)
        return true;

    qInfo() << "[Application] starting (dataDir:" << d_->options.dataDirectory << "headless:" << d_->options.headless
            << ")";

    if (!d_->manticore->start() || !d_->manticore->waitForReady()) {
        qCritical() << "[Application] database failed to start";
        return false;
    }
    d_->torrents->primeFromDatabase();

    // Blocking pre-start migrations run before anything reads the data. These are
    // housekeeping migrations, so a failure is non-fatal — but it must not pass
    // silently (the service logs the specific migration; we log the outcome).
    // They finish before any GUI exists, so they can't be surfaced via signals;
    // the async migrations below run while the window is up and ARE surfaced
    // there.
    if (!d_->migrations->runSyncMigrations()) {
        qWarning() << "[Application] one or more synchronous migrations failed; "
                      "continuing startup";
    }

    d_->feed->load();
    d_->transport->start();

    if (d_->config->indexerEnabled() || d_->options.forceSpider)
        d_->crawler->start();
    if (d_->config->p2pReplication())
        d_->replication->start();

    // Restore any in-progress downloads.
    d_->downloads->loadSession(d_->options.dataDirectory + QStringLiteral("/torrents_session.json"));

    // Background (resumable) migrations.
    d_->migrations->startAsyncMigrations();

    if (d_->config->restApiEnabled())
        d_->apiServer->start(d_->config->httpPort());

    d_->running = true;
    emit started();
    return true;
}

void Application::stop()
{
    if (!d_->running)
        return;
    emit stopping();
    qInfo() << "[Application] stopping";

    d_->apiServer->stop();
    d_->downloads->saveSession(d_->options.dataDirectory + QStringLiteral("/torrents_session.json"));
    d_->feed->save();
    d_->replication->stop();
    d_->crawler->stop();
    d_->transport->stop();
    d_->manticore->stop();

    d_->running = false;
}

const Application::Options& Application::options() const
{
    return d_->options;
}
bool Application::isRunning() const
{
    return d_->running;
}

ConfigStore* Application::config() const
{
    return d_->config.get();
}
TranslationManager* Application::translation() const
{
    return &TranslationManager::instance();
}
FavoritesStore* Application::favorites() const
{
    return d_->favorites.get();
}
data::Database* Application::database() const
{
    return d_->database.get();
}
data::TorrentRepository* Application::torrents() const
{
    return d_->torrents.get();
}
net::P2PTransport* Application::transport() const
{
    return d_->transport.get();
}
net::TorrentEngine* Application::engine() const
{
    return d_->engine.get();
}
net::Crawler* Application::crawler() const
{
    return d_->crawler.get();
}
service::IndexingService* Application::indexing() const
{
    return d_->indexing.get();
}
service::SearchService* Application::search() const
{
    return d_->search.get();
}
service::DownloadService* Application::downloads() const
{
    return d_->downloads.get();
}
service::TorrentCreator* Application::creator() const
{
    return d_->creator.get();
}

service::TorrentExporter* Application::exporter() const
{
    return d_->exporter.get();
}
service::FeedService* Application::feed() const
{
    return d_->feed.get();
}
service::VotingService* Application::voting() const
{
    return d_->voting.get();
}
service::ReplicationService* Application::replication() const
{
    return d_->replication.get();
}
service::TrackerService* Application::trackers() const
{
    return d_->trackers.get();
}
service::PeerRegistry* Application::peers() const
{
    return d_->peers.get();
}
service::UpdateService* Application::updates() const
{
    return d_->updates.get();
}
service::MigrationService* Application::migrations() const
{
    return d_->migrations.get();
}
rest::ApiRouter* Application::api() const
{
    return d_->apiRouter.get();
}
peer::PeerApi* Application::peerApi() const
{
    return d_->peerApi.get();
}

} // namespace rats::app
