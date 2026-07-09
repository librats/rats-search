#ifndef RATS_SERVICE_DOWNLOAD_SERVICE_H
#define RATS_SERVICE_DOWNLOAD_SERVICE_H

#include "domain/torrent.h"
#include "net/torrent_engine.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <memory>

namespace rats::service {

class TorrentSessionStore;

// Per-file download state. Extends the pure domain::File (path + size) with the
// download-runtime bits — selection and per-file progress.
struct DownloadFile {
    QString path;
    qint64 size = 0;
    int index = 0;
    bool selected = true;
    double progress = 0.0;

    QJsonObject toJson() const;
};

// The live state of one active download (or a completed torrent kept for
// seeding).
struct Download {
    QString hash;
    QString name;
    QString savePath;
    qint64 totalSize = 0;
    qint64 downloadedBytes = 0;
    double progress = 0.0;
    double downloadSpeed = 0.0;
    int peersConnected = 0;
    bool paused = false;
    bool removeOnDone = false;
    bool ready = false; // metadata (name/files/size) known
    bool completed = false;
    bool added = false; // handed to librats (add_* returned)
    QVector<DownloadFile> files;

    // Rolling sample used to derive downloadSpeed from the cumulative byte
    // counter between polls (librats exposes total bytes, not a per-torrent
    // rate).
    qint64 lastSampledBytes = 0;
    qint64 lastSampledMs = 0;

    // Last progress payload emitted, so the 1s poll can skip re-broadcasting an
    // unchanged snapshot for idle/paused/seeding downloads.
    QJsonObject lastProgress;

    QJsonObject toJson() const;
};

// Owns the active-download registry, the 1s progress-poll timer and the
// add/pause/resume/remove/select lifecycle. All librats work is delegated to
// TorrentEngine and all persistence to TorrentSessionStore, so this class holds
// no librats include and no file I/O of its own.
//
// Note on failure reporting: there is deliberately no async "downloadFailed"
// signal. A magnet whose metadata never arrives is retried by librats
// indefinitely (the crawler owns fetch timeouts), so the only failures are the
// synchronous ones — bad hash, engine not ready, add_* rejected — reported by
// the bool return of the add/restore methods.
class DownloadService : public QObject {
    Q_OBJECT

public:
    // engine is borrowed (non-owning) and must outlive the service.
    explicit DownloadService(net::TorrentEngine* engine, QObject* parent = nullptr);
    ~DownloadService() override;

    bool isReady() const;

    // --- Download lifecycle -------------------------------------------------
    // Add by magnet link or 40-char hex hash.
    bool add(const QString& magnetLink, const QString& savePath = QString());
    // Add by known metadata (hash + optional name/size), fetching content via
    // DHT.
    bool addWithInfo(const domain::Torrent& info, const QString& savePath = QString());
    // Add from a .torrent file (metadata is available immediately).
    bool addFromFile(const QString& torrentFile, const QString& savePath = QString());
    // Restore a torrent from a previous session, loading any resume data.
    bool restore(const QString& hash, const QString& name, const QString& savePath, bool wasCompleted);

    // Stop and remove a torrent. saveResumeData preserves downloaded pieces.
    void remove(const QString& hash, bool saveResumeData = false);
    bool pause(const QString& hash);
    bool resume(const QString& hash);
    bool togglePause(const QString& hash);
    bool selectFiles(const QString& hash, const QVector<bool>& selection);
    bool selectFilesJson(const QString& hash, const QJsonValue& selection);
    void setRemoveOnDone(const QString& hash, bool removeOnDone);

    // Register an already-created, seeding torrent in the registry (used by
    // TorrentCreator). Emits the started/files/completed/created signals. Returns
    // the (normalised) info hash.
    QString registerSeed(const net::SeedResult& seed);

    // --- Queries ------------------------------------------------------------
    bool isDownloading(const QString& hash) const;
    Download getDownload(const QString& hash) const;
    QVector<Download> allDownloads() const;
    int count() const;
    QJsonArray toJsonArray() const;

    // --- Configuration ------------------------------------------------------
    void setDefaultDownloadPath(const QString& path);

    // --- Session persistence (delegated to TorrentSessionStore) -------------
    bool saveSession(const QString& filePath);
    int loadSession(const QString& filePath);

    // The borrowed engine, so a TorrentCreator can share it.
    net::TorrentEngine* engine() const { return engine_; }

signals:
    void downloadStarted(const QString& hash);
    // File list available (after metadata), as an API-compatible JSON array.
    void filesReady(const QString& hash, const QJsonArray& files);
    void progressUpdated(const QString& hash, const QJsonObject& progress);
    void downloadCompleted(const QString& hash);
    void torrentRemoved(const QString& hash);
    void stateChanged(const QString& hash, const QJsonObject& state);

private slots:
    void onUpdateTimer();

private:
    // State changes detected during a poll, flushed as signals afterwards.
    struct Transitions {
        QStringList newlyReady;
        QStringList newlyCompleted;
        QStringList toRemove;
    };

    // onUpdateTimer split into its three concerns.
    Transitions pollStatus();
    void computeSpeed(Download& d, qint64 nowMs);
    void flushTransitions(const Transitions& t);

    // Copy metadata (name/size/files) from a librats snapshot into a Download.
    void applySnapshot(Download& d, const net::TorrentSnapshot& snap);

    QString resolveSavePath(const QString& savePath) const;
    static bool ensureDir(const QString& path);
    bool contains(const QString& hash) const;
    static QJsonArray filesToJson(const QVector<DownloadFile>& files);
    static QJsonObject progressJson(const Download& d);

    net::TorrentEngine* engine_; // borrowed, non-owning
    std::unique_ptr<TorrentSessionStore> sessionStore_;
    QString defaultDownloadPath_;

    QHash<QString, Download> downloads_;
    mutable QMutex mutex_;

    QTimer* updateTimer_ = nullptr;
};

} // namespace rats::service

#endif // RATS_SERVICE_DOWNLOAD_SERVICE_H
