#ifndef RATS_SERVICE_TORRENT_SESSION_STORE_H
#define RATS_SERVICE_TORRENT_SESSION_STORE_H

#include "services/download_service.h" // for Download / DownloadFile

#include <QString>
#include <QVector>

namespace rats::service {

// Reads and writes the download session file (torrents_session.json): the set
// of active downloads to restore on the next launch. This is pure JSON file I/O
// — it holds no state, touches no librats and saves no resume data (the service
// asks the engine to do that separately).
//
// Serialisation reuses DownloadFile::toJson(), the very same helper the live
// progress path uses, so the persisted per-file shape cannot drift from the
// live one. Never hand-build the file object here.
class TorrentSessionStore {
public:
    // Overwrite the session file with `downloads`. Removes the file when the list
    // is empty. Returns false only on a real write error.
    bool save(const QString& filePath, const QVector<Download>& downloads) const;

    // Parse the session file into restore entries (empty on a missing or
    // malformed file). Populated fields: hash, name, savePath, totalSize, paused,
    // removeOnDone, completed, downloadedBytes, progress and the per-file
    // selection/progress.
    QVector<Download> load(const QString& filePath) const;
};

} // namespace rats::service

#endif // RATS_SERVICE_TORRENT_SESSION_STORE_H
