#ifndef RATS_APP_APPLICATION_H
#define RATS_APP_APPLICATION_H

#include <QObject>
#include <QString>
#include <memory>

namespace rats::app {
class ConfigStore;
class TranslationManager;
class FavoritesStore;
} // namespace rats::app
namespace rats::data {
class Database;
class TorrentRepository;
} // namespace rats::data
namespace rats::net {
class P2PTransport;
class TorrentEngine;
class Crawler;
} // namespace rats::net
namespace rats::rest {
class ApiRouter;
class ApiServer;
} // namespace rats::rest
namespace rats::peer {
class PeerApi;
}
namespace rats::service {
class IndexingService;
class SearchService;
class DownloadService;
class TorrentCreator;
class TorrentExporter;
class FeedService;
class VotingService;
class ReplicationService;
class TrackerService;
class PeerRegistry;
class UpdateService;
class MigrationService;
} // namespace rats::service

namespace rats::app {

// Composition root: owns every adapter and service, wires them together in
// dependency order, and drives the lifecycle.
//
// Front-ends (GUI window, REST/WS router, console shell, P2P peer API) receive
// a running Application and only call into its services through the accessors
// below; they hold no business logic.
class Application : public QObject {
    Q_OBJECT

public:
    struct Options {
        QString dataDirectory;
        QString clientVersion;
        bool headless = false; // --console mode (no widgets)
        // CLI overrides (0 / false => use the stored config value).
        int p2pPort = 0;
        int dhtPort = 0;
        int maxPeers = 0;
        bool forceSpider = false;
    };

    explicit Application(Options options, QObject* parent = nullptr);
    ~Application() override;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Bring all subsystems up in dependency order. Returns false if a critical
    // subsystem (the database) fails. Idempotent.
    bool start();
    void stop();

    const Options& options() const;
    bool isRunning() const;

    // Service accessors (non-owning). Valid after construction; live subsystems
    // (database, transport) only after start().
    ConfigStore* config() const;
    TranslationManager* translation() const;
    FavoritesStore* favorites() const;
    data::Database* database() const;
    data::TorrentRepository* torrents() const;
    net::P2PTransport* transport() const;
    net::TorrentEngine* engine() const;
    net::Crawler* crawler() const;
    service::IndexingService* indexing() const;
    service::SearchService* search() const;
    service::DownloadService* downloads() const;
    service::TorrentCreator* creator() const;
    service::TorrentExporter* exporter() const;
    service::FeedService* feed() const;
    service::VotingService* voting() const;
    service::ReplicationService* replication() const;
    service::TrackerService* trackers() const;
    service::PeerRegistry* peers() const;
    service::UpdateService* updates() const;
    service::MigrationService* migrations() const;
    rest::ApiRouter* api() const;
    peer::PeerApi* peerApi() const;

signals:
    void started();
    void stopping();

private:
    void applyConfig(); // push current config values into the services
    void wireSignals(); // connect cross-service signals

    struct Private;
    std::unique_ptr<Private> d_;
};

} // namespace rats::app

#endif // RATS_APP_APPLICATION_H
