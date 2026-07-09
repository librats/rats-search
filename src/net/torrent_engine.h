#ifndef RATS_NET_TORRENT_ENGINE_H
#define RATS_NET_TORRENT_ENGINE_H

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

namespace librats {
class Bittorrent;
namespace bittorrent {
class Client;
}
} // namespace librats

namespace rats::net {

class P2PTransport;

// A single file entry as reported by librats (path + size only). The download
// runtime state (selection, per-file progress) lives one layer up in the
// service — this stays a pure librats-level value.
struct EngineFile {
    QString path;
    qint64 size = 0;
};

// A thread-safe snapshot of a librats torrent's live state, translated into Qt
// types so nothing above the engine needs to include a librats header.
struct TorrentSnapshot {
    bool exists = false; // librats still tracks this info-hash
    bool hasMetadata = false; // name/files/size are known
    bool isComplete = false;
    QString name;
    qint64 totalSize = 0;
    qint64 downloaded = 0;
    double progress = 0.0;
    int numPeers = 0;
    QVector<EngineFile> files;
};

// Metadata parsed from a .torrent file. valid=false means the parse failed.
struct TorrentMetadata {
    bool valid = false;
    QString hash; // 40-char lower-case hex
    QString name;
    qint64 totalSize = 0;
    QVector<EngineFile> files;
};

// The outcome of a create-and-seed: everything needed to register the seeding
// torrent in the download registry without re-reading anything from librats.
struct SeedResult {
    bool ok = false;
    QString hash;
    QString name;
    qint64 totalSize = 0;
    QString savePath; // directory librats seeds the content from
    QVector<EngineFile> files;
};

// A THIN wrapper over the librats BitTorrent subsystem. It borrows the client
// from a P2PTransport (non-owning) and speaks only in librats-level operations:
// add / pause / resume / remove, status snapshots and torrent creation. It holds
// no registry, runs no timers and persists no session — those belong to the
// service layer. Every librats type stays behind this boundary.
class TorrentEngine {
public:
    // Drives a progress bar while the creator hashes each piece.
    using CreationProgressCallback = std::function<void(int currentPiece, int totalPieces)>;

    // transport is borrowed (non-owning) and must outlive the engine.
    explicit TorrentEngine(P2PTransport* transport);

    // True when the BitTorrent subsystem is up and a client is available.
    bool isReady() const;

    // --- Adding torrents ----------------------------------------------------
    // Add a metadata-less magnet download; metadata arrives later via BEP 9.
    bool addMagnet(const QString& hash, const QString& savePath);
    // Add a magnet, loading any resume file saved next to the download (restores
    // downloaded pieces and, if embedded, the metadata).
    bool addMagnetResumed(const QString& hash, const QString& savePath);
    // Parse a .torrent file (pure; no side effects). valid=false on failure.
    TorrentMetadata readTorrentFile(const QString& torrentFile) const;
    // Parse a .torrent file and hand it to librats. false on parse or add error.
    bool addTorrentFile(const QString& torrentFile, const QString& savePath);

    // --- Metadata fetch (export) --------------------------------------------
    // Assembled .torrent bytes delivered by fetchTorrentFile. On failure `bytes`
    // is empty and `error` is set; `name` is the metadata name (may be empty).
    // NOTE: invoked on a librats worker thread — marshal before touching Qt UI.
    using TorrentFileCallback = std::function<void(const QByteArray& bytes, const QString& name, const QString& error)>;

    // Fetch a torrent's metadata over the DHT/BEP 9 (no full content download) and
    // assemble a complete .torrent byte stream: the original info dict preserved
    // verbatim (so the info hash stays valid) plus any trackers. `callback` runs
    // exactly once. Returns false synchronously when BitTorrent is unavailable, in
    // which case the callback is NOT invoked.
    bool fetchTorrentFile(const QString& hash, TorrentFileCallback callback, int timeoutMs = 60000);

    // --- Lifecycle ----------------------------------------------------------
    void pause(const QString& hash);
    void resume(const QString& hash);
    void remove(const QString& hash);
    void saveResumeData(const QString& hash);

    // A thread-safe snapshot of the torrent's live state (exists=false if gone).
    TorrentSnapshot status(const QString& hash) const;

    // --- Creation / seeding -------------------------------------------------
    // Hash `path` into a new torrent and start seeding it. Optionally writes the
    // .torrent to saveTorrentFilePath, reusing the just-hashed pieces (no second
    // full hash). Returns ok=false on failure.
    SeedResult createAndSeed(const QString& path, const QStringList& trackers, const QString& comment,
        const QString& saveTorrentFilePath, const CreationProgressCallback& progress);
    // Hash `path` and write a .torrent to outputFile without seeding.
    bool createTorrentFile(const QString& path, const QString& outputFile, const QStringList& trackers,
        const QString& comment, const CreationProgressCallback& progress);

    // Extract a 40-char lower-case info-hash from a magnet URI or a raw hash.
    // Returns an empty string when none can be parsed. Pure — no librats needed.
    static QString parseInfoHash(const QString& magnetLink);

private:
    // The librats BitTorrent client (null when the subsystem is down).
    librats::bittorrent::Client* client() const;

    P2PTransport* transport_; // borrowed, non-owning
};

} // namespace rats::net

#endif // RATS_NET_TORRENT_ENGINE_H
