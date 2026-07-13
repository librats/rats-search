#include "torrentmenu.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QMenu>
#include <QUrl>

namespace rats {
namespace ui {

void addTorrentActions(QMenu* menu, QObject* context, const rats::domain::Torrent& torrent, NotifyFn notify)
{
    QAction* magnetAction = menu->addAction(QObject::tr("Open Magnet Link"));
    QObject::connect(magnetAction, &QAction::triggered, context,
        [torrent]() { QDesktopServices::openUrl(QUrl(torrent.magnetLink())); });

    QAction* copyHashAction = menu->addAction(QObject::tr("Copy Info Hash"));
    QObject::connect(copyHashAction, &QAction::triggered, context, [torrent, notify]() {
        QApplication::clipboard()->setText(torrent.hash);
        if (notify)
            notify(QObject::tr("Hash copied to clipboard"));
    });

    QAction* copyMagnetAction = menu->addAction(QObject::tr("Copy Magnet Link"));
    QObject::connect(copyMagnetAction, &QAction::triggered, context, [torrent, notify]() {
        QApplication::clipboard()->setText(torrent.magnetLink());
        if (notify)
            notify(QObject::tr("Magnet link copied to clipboard"));
    });
}

} // namespace ui
} // namespace rats
