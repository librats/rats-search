#ifndef RATS_SERVICE_TORRENT_EXPORTER_H
#define RATS_SERVICE_TORRENT_EXPORTER_H

#include <QObject>
#include <QSet>
#include <QString>

namespace rats::net {
class TorrentEngine;
}

namespace rats::service {

// Produces a real .torrent file for an already-indexed torrent. The pure-librats
// work (BEP 9 metadata fetch + .torrent byte assembly) lives in TorrentEngine;
// this service adds the orchestration a front-end needs: an on-disk cache under
// {dataDir}/torrents/{hash}.torrent, de-duplication of concurrent fetches for the
// same hash, and front-end-agnostic result signals. It performs NO GUI work — a
// front-end listens for exportReady and prompts the user for a save location.
//
// This mirrors TorrentCreator: a thin service tying a librats engine operation to
// the rest of the app.
class TorrentExporter : public QObject {
    Q_OBJECT

public:
    // engine is borrowed (non-owning) and must outlive the exporter.
    TorrentExporter(net::TorrentEngine* engine, QString dataDirectory, QObject* parent = nullptr);

    void setDataDirectory(const QString& dir);

    // Request a .torrent for `hash`. If a cached copy exists, emits exportReady
    // immediately; otherwise fetches metadata (may take a while), writes it to the
    // cache, then emits exportReady. Concurrent requests for the same hash are
    // coalesced. `name` (optional) is echoed back for a friendly suggested filename.
    void requestExport(const QString& hash, const QString& name = QString());

    // Absolute path of the cached .torrent for `hash` (may not exist yet).
    QString cachePath(const QString& hash) const;

signals:
    // A .torrent is available at `cachePath` (cached or freshly fetched).
    void exportReady(const QString& hash, const QString& name, const QString& cachePath);
    // The export could not be produced.
    void exportFailed(const QString& hash, const QString& reason);
    // Human-readable progress for a status bar (timeoutMs 0 = show until replaced).
    void statusMessage(const QString& message, int timeoutMs);

private:
    bool ensureCacheDir() const;

    net::TorrentEngine* engine_; // borrowed, non-owning
    QString dataDirectory_;
    QSet<QString> inFlight_; // hashes with a fetch in progress (main-thread only)
};

} // namespace rats::service

#endif // RATS_SERVICE_TORRENT_EXPORTER_H
