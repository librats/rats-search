#include "torrentexporter.h"
#include "p2pnetwork.h"
#include "torrentdatabase.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QPointer>
#include <QWidget>
#include <QDebug>
#include <QRegularExpression>
#include <QMetaObject>
#include <QCoreApplication>
#include <QStringList>

// Neutralise Qt's `emit` macro across librats includes (EventBus::emit collides).
#pragma push_macro("emit")
#pragma push_macro("slots")
#pragma push_macro("signals")
#undef emit
#undef slots
#undef signals
#ifdef RATS_SEARCH_FEATURES
#include "subsystems/bittorrent.h"
#include "bittorrent/torrent_info.h"
#endif
#pragma pop_macro("signals")
#pragma pop_macro("slots")
#pragma pop_macro("emit")

namespace {

// Sanitise a torrent name for use as a file name (keep readable chars only).
QString sanitizeFileName(const QString &name)
{
    QString out = name;
    static const QRegularExpression illegal(R"([<>:"/\\|?*\x00-\x1f])");
    out.replace(illegal, "_");
    out = out.trimmed();
    if (out.length() > 200) out.truncate(200);
    if (out.isEmpty()) out = QStringLiteral("torrent");
    return out;
}

// Build a .torrent file by wrapping a raw info-dict with announce/announce-list
// keys at the byte level. Re-encoding via BencodeValue would re-sort keys and
// could change the info dict bytes, invalidating the info hash; this preserves
// the original bytes exactly.
QByteArray buildTorrentBytes(const QByteArray &infoDictBytes,
                             const QStringList &trackers)
{
    QByteArray out;
    out.reserve(infoDictBytes.size() + 256);
    out.append('d');

    // "announce" - primary tracker (sorted: announce < announce-list < info)
    if (!trackers.isEmpty()) {
        const QByteArray url = trackers.first().toUtf8();
        out.append("8:announce");
        out.append(QByteArray::number(url.size()));
        out.append(':');
        out.append(url);
    }

    // "announce-list" - all trackers (one per tier)
    if (trackers.size() > 1) {
        out.append("13:announce-list");
        out.append('l');
        for (const QString &t : trackers) {
            const QByteArray url = t.toUtf8();
            out.append('l');
            out.append(QByteArray::number(url.size()));
            out.append(':');
            out.append(url);
            out.append('e');
        }
        out.append('e');
    }

    // "info" - raw info dict bytes preserved verbatim
    out.append("4:info");
    out.append(infoDictBytes);

    out.append('e');
    return out;
}

#ifdef RATS_SEARCH_FEATURES
QStringList trackersFromLibrats(const librats::bittorrent::TorrentInfo &t)
{
    QStringList list;
    for (const std::string &url : t.all_trackers()) {
        list.append(QString::fromStdString(url));
    }
    return list;
}
#endif

} // namespace

TorrentExporter::TorrentExporter(QObject *parent)
    : QObject(parent)
{
}

TorrentExporter::~TorrentExporter() = default;

void TorrentExporter::setDataDirectory(const QString &dir)
{
    dataDirectory_ = dir;
}

QString TorrentExporter::cacheFilePath(const QString &hash) const
{
    return QDir(dataDirectory_).absoluteFilePath(QStringLiteral("torrents/") + hash.toLower() + ".torrent");
}

QString TorrentExporter::suggestedFileName(const TorrentInfo &torrent) const
{
    QString base = torrent.name.isEmpty() ? torrent.hash : torrent.name;
    return sanitizeFileName(base) + ".torrent";
}

bool TorrentExporter::ensureCacheDir() const
{
    QDir dir(dataDirectory_);
    if (!dir.exists("torrents")) {
        return dir.mkpath("torrents");
    }
    return true;
}

void TorrentExporter::exportTorrent(QWidget *parent, const TorrentInfo &torrent)
{
    if (!torrent.isValid()) {
        QMessageBox::warning(parent, tr("Export Torrent"), tr("Torrent has no valid info hash."));
        return;
    }

    if (dataDirectory_.isEmpty()) {
        qWarning() << "TorrentExporter: data directory not set";
        QMessageBox::critical(parent, tr("Export Torrent"), tr("Data directory not configured."));
        return;
    }

    const QString cachePath = cacheFilePath(torrent.hash);
    if (QFile::exists(cachePath)) {
        promptAndCopy(parent, cachePath, torrent);
        return;
    }

    fetchMetadataAndSave(parent, torrent);
}

void TorrentExporter::promptAndCopy(QWidget *parent, const QString &cachePath, const TorrentInfo &torrent)
{
    const QString suggested = suggestedFileName(torrent);
    const QString defaultDir = QDir::homePath();
    const QString defaultPath = QDir(defaultDir).absoluteFilePath(suggested);

    QString destination = QFileDialog::getSaveFileName(
        parent,
        tr("Export Torrent As"),
        defaultPath,
        tr("Torrent Files (*.torrent);;All Files (*)"));

    if (destination.isEmpty()) {
        return; // user cancelled
    }

    if (!destination.endsWith(QStringLiteral(".torrent"), Qt::CaseInsensitive)) {
        destination += QStringLiteral(".torrent");
    }

    if (QFile::exists(destination)) {
        QFile::remove(destination);
    }

    if (!QFile::copy(cachePath, destination)) {
        QMessageBox::critical(parent, tr("Export Torrent"),
                              tr("Failed to save torrent to:\n%1").arg(destination));
        emit exportFailed(torrent.hash, tr("Failed to write file"));
        return;
    }

    emit statusMessage(tr("Torrent exported to %1").arg(destination), 4000);
    emit exportSucceeded(torrent.hash, destination);
}

void TorrentExporter::fetchMetadataAndSave(QWidget *parent, const TorrentInfo &torrent)
{
#ifdef RATS_SEARCH_FEATURES
    if (!p2pNetwork_) {
        QMessageBox::critical(parent, tr("Export Torrent"), tr("P2P network not available."));
        return;
    }

    if (!p2pNetwork_->isBitTorrentEnabled()) {
        QMessageBox::critical(parent, tr("Export Torrent"),
                              tr("BitTorrent is not enabled — cannot fetch metadata."));
        return;
    }

    auto *bt = p2pNetwork_->bittorrent();
    if (!bt || !bt->is_running()) {
        QMessageBox::critical(parent, tr("Export Torrent"), tr("BitTorrent client unavailable."));
        return;
    }

    const QString hash = torrent.hash.toLower();

    // De-duplicate concurrent fetches for the same hash
    {
        QMutexLocker lock(&inFlightMutex_);
        if (inFlight_.contains(hash)) {
            emit statusMessage(tr("Already fetching metadata for %1...").arg(hash.left(8)), 3000);
            return;
        }
        inFlight_.insert(hash);
    }

    if (!ensureCacheDir()) {
        QMessageBox::critical(parent, tr("Export Torrent"),
                              tr("Failed to create torrent cache directory."));
        QMutexLocker lock(&inFlightMutex_);
        inFlight_.remove(hash);
        return;
    }

    emit statusMessage(tr("Fetching metadata for %1... this may take a while").arg(hash.left(8)), 0);

    QPointer<QWidget> parentPtr(parent);
    QPointer<TorrentExporter> selfPtr(this);
    TorrentInfo torrentCopy = torrent;
    const QString cachePath = cacheFilePath(hash);

    bt->get_torrent_metadata(hash.toStdString(),
        [selfPtr, parentPtr, torrentCopy, cachePath, hash]
        (const librats::bittorrent::TorrentInfo &meta, bool success, const std::string &error) {
            // Capture the data we need on the I/O thread, then marshal to GUI thread
            QString errMsg = QString::fromStdString(error);

            QByteArray infoDict;
            QStringList trackers;
            QString detectedName;
            if (success && meta.is_valid() && meta.has_metadata()) {
                const auto &bytes = meta.info_dict_bytes();
                infoDict = QByteArray(reinterpret_cast<const char*>(bytes.data()),
                                       static_cast<int>(bytes.size()));
                trackers = trackersFromLibrats(meta);
                detectedName = QString::fromStdString(meta.name());
            }

            // Hop to the GUI thread to do file I/O and show dialogs.
            QMetaObject::invokeMethod(qApp, [selfPtr, parentPtr, torrentCopy, cachePath, hash,
                                             success, errMsg, infoDict, trackers, detectedName]() mutable {
                if (!selfPtr) return;

                {
                    QMutexLocker lock(&selfPtr->inFlightMutex_);
                    selfPtr->inFlight_.remove(hash);
                }

                if (!success || infoDict.isEmpty()) {
                    QString reason = errMsg.isEmpty() ? tr("metadata download failed") : errMsg;
                    emit selfPtr->statusMessage(tr("Failed to fetch metadata for %1: %2")
                                                 .arg(hash.left(8), reason), 5000);
                    if (parentPtr) {
                        QMessageBox::warning(parentPtr, tr("Export Torrent"),
                            tr("Failed to fetch metadata for the torrent.\n\n"
                               "Reason: %1\n\nMake sure the torrent has active peers.").arg(reason));
                    }
                    emit selfPtr->exportFailed(hash, reason);
                    return;
                }

                // Augment trackers with anything the database knows about
                if (selfPtr->database_) {
                    // Database may store extra tracker info in TorrentInfo::info json.
                    // We don't currently mine that, but leave the hook for future use.
                }

                QByteArray torrentBytes = buildTorrentBytes(infoDict, trackers);

                QFile out(cachePath);
                if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    if (parentPtr) {
                        QMessageBox::critical(parentPtr, tr("Export Torrent"),
                            tr("Failed to write cached torrent file:\n%1").arg(cachePath));
                    }
                    emit selfPtr->exportFailed(hash, tr("Failed to write cache"));
                    return;
                }
                out.write(torrentBytes);
                out.close();

                // Update the displayed name if we just learned a better one
                TorrentInfo enriched = torrentCopy;
                if (enriched.name.isEmpty() && !detectedName.isEmpty()) {
                    enriched.name = detectedName;
                }

                emit selfPtr->statusMessage(tr("Metadata received for %1").arg(hash.left(8)), 2000);
                selfPtr->promptAndCopy(parentPtr, cachePath, enriched);
            }, Qt::QueuedConnection);
        });
#else
    Q_UNUSED(parent);
    Q_UNUSED(torrent);
    QMessageBox::critical(parent, tr("Export Torrent"),
                          tr("BitTorrent support was not compiled in this build."));
#endif
}
