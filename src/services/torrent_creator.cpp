#include "services/torrent_creator.h"

#include "services/download_service.h"

#include <QDebug>

namespace rats::service {

TorrentCreator::TorrentCreator(net::TorrentEngine* engine, DownloadService* downloads)
    : engine_(engine), downloads_(downloads)
{
}

QString TorrentCreator::createAndSeed(const QString& path, const QStringList& trackers, const QString& comment,
    const QString& saveTorrentFilePath, const CreationProgressCallback& progress)
{
    if (!engine_ || !engine_->isReady()) {
        qWarning() << "TorrentCreator: Not ready for torrent creation";
        return QString();
    }

    qInfo() << "TorrentCreator: Creating and seeding torrent from:" << path;

    net::SeedResult seed = engine_->createAndSeed(path, trackers, comment, saveTorrentFilePath, progress);
    if (!seed.ok) {
        qWarning() << "TorrentCreator: Failed to create/seed torrent from:" << path;
        return QString();
    }

    // Register the completed torrent so the UI/API tracks it like any download.
    // (Without a registry there is nothing to show, but the seed is already live.)
    return downloads_ ? downloads_->registerSeed(seed) : seed.hash;
}

bool TorrentCreator::createTorrentFile(const QString& path, const QString& outputFile, const QStringList& trackers,
    const QString& comment, const CreationProgressCallback& progress)
{
    if (!engine_ || !engine_->isReady()) {
        qWarning() << "TorrentCreator: Not ready for torrent creation";
        return false;
    }

    qInfo() << "TorrentCreator: Creating torrent file from:" << path << "to:" << outputFile;
    return engine_->createTorrentFile(path, outputFile, trackers, comment, progress);
}

} // namespace rats::service
