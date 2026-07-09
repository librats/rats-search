#ifndef RATS_SERVICE_TORRENT_CREATOR_H
#define RATS_SERVICE_TORRENT_CREATOR_H

#include "net/torrent_engine.h"

#include <QString>
#include <QStringList>

namespace rats::service {

class DownloadService;

// Create-and-seed orchestration layered on TorrentEngine. The pure-librats work
// (hashing content, writing the .torrent, handing the torrent to librats) lives
// in TorrentEngine; this class ties it to the download registry: a freshly
// created torrent is registered as a completed, seeding download so it shows up
// alongside ordinary downloads in the UI/API.
class TorrentCreator {
public:
    using CreationProgressCallback = net::TorrentEngine::CreationProgressCallback;

    // Both dependencies are borrowed (non-owning) and must outlive the creator.
    TorrentCreator(net::TorrentEngine* engine, DownloadService* downloads);

    // Create a torrent from a file/directory and start seeding it. Returns the
    // info hash, or an empty string on failure. When saveTorrentFilePath is set,
    // the .torrent is written there reusing the pieces hashed for seeding.
    QString createAndSeed(const QString& path, const QStringList& trackers = QStringList(),
        const QString& comment = QString(), const QString& saveTorrentFilePath = QString(),
        const CreationProgressCallback& progress = nullptr);

    // Create a .torrent file without seeding. Returns true on success.
    bool createTorrentFile(const QString& path, const QString& outputFile, const QStringList& trackers = QStringList(),
        const QString& comment = QString(), const CreationProgressCallback& progress = nullptr);

private:
    net::TorrentEngine* engine_; // borrowed, non-owning
    DownloadService* downloads_; // borrowed, non-owning
};

} // namespace rats::service

#endif // RATS_SERVICE_TORRENT_CREATOR_H
