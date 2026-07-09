#include "services/torrent_exporter.h"

#include "common/infohash.h"
#include "net/torrent_engine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QPointer>
#include <utility>

namespace rats::service {

TorrentExporter::TorrentExporter(net::TorrentEngine* engine, QString dataDirectory, QObject* parent)
    : QObject(parent), engine_(engine), dataDirectory_(std::move(dataDirectory))
{
}

void TorrentExporter::setDataDirectory(const QString& dir)
{
    dataDirectory_ = dir;
}

QString TorrentExporter::cachePath(const QString& hash) const
{
    return QDir(dataDirectory_)
        .absoluteFilePath(QStringLiteral("torrents/") + hash.toLower() + QStringLiteral(".torrent"));
}

bool TorrentExporter::ensureCacheDir() const
{
    QDir dir(dataDirectory_);
    return dir.exists(QStringLiteral("torrents")) || dir.mkpath(QStringLiteral("torrents"));
}

void TorrentExporter::requestExport(const QString& hash, const QString& name)
{
    const QString h = infohash::normalize(hash);
    if (!infohash::isValid(h)) {
        emit exportFailed(hash, tr("Torrent has no valid info hash."));
        return;
    }
    if (dataDirectory_.isEmpty()) {
        emit exportFailed(h, tr("Data directory is not configured."));
        return;
    }

    // A cached .torrent from a previous export/fetch is offered immediately.
    const QString cache = cachePath(h);
    if (QFile::exists(cache)) {
        emit exportReady(h, name, cache);
        return;
    }

    if (inFlight_.contains(h)) {
        emit statusMessage(tr("Already fetching metadata for %1…").arg(h.left(8)), 3000);
        return;
    }
    if (!engine_ || !engine_->isReady()) {
        emit exportFailed(h, tr("BitTorrent is not available — cannot fetch metadata."));
        return;
    }
    if (!ensureCacheDir()) {
        emit exportFailed(h, tr("Failed to create the torrent cache directory."));
        return;
    }

    inFlight_.insert(h);
    emit statusMessage(tr("Fetching metadata for %1… this may take a while.").arg(h.left(8)), 0);

    QPointer<TorrentExporter> self(this);
    const bool started = engine_->fetchTorrentFile(
        h, [self, h, name, cache](const QByteArray& bytes, const QString& detectedName, const QString& error) {
            // fetchTorrentFile's callback runs on a librats worker thread; hop back
            // to the exporter's (main) thread for file I/O and signal emission.
            QMetaObject::invokeMethod(
                qApp,
                [self, h, name, cache, bytes, detectedName, error]() {
                    if (!self) {
                        return;
                    }
                    self->inFlight_.remove(h);

                    if (bytes.isEmpty()) {
                        const QString reason = error.isEmpty() ? tr("metadata download failed") : error;
                        emit self->statusMessage(
                            tr("Failed to fetch metadata for %1: %2").arg(h.left(8), reason), 5000);
                        emit self->exportFailed(h, reason);
                        return;
                    }

                    QFile out(cache);
                    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        emit self->exportFailed(h, tr("Failed to write the cached torrent file."));
                        return;
                    }
                    out.write(bytes);
                    out.close();

                    emit self->statusMessage(tr("Metadata received for %1.").arg(h.left(8)), 2000);
                    emit self->exportReady(h, name.isEmpty() ? detectedName : name, cache);
                },
                Qt::QueuedConnection);
        });

    // fetchTorrentFile only returns false when BitTorrent went away between the
    // isReady() check and the call; undo the in-flight marker and report it.
    if (!started) {
        inFlight_.remove(h);
        emit exportFailed(h, tr("BitTorrent is not available — cannot fetch metadata."));
    }
}

} // namespace rats::service
