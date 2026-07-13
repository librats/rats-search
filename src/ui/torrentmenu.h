#ifndef RATS_UI_TORRENTMENU_H
#define RATS_UI_TORRENTMENU_H

#include "domain/torrent.h"

#include <QString>
#include <functional>

class QMenu;
class QObject;

namespace rats {
namespace ui {

// Optional feedback hook: a front-end with a status bar shows the message,
// others leave it unset.
using NotifyFn = std::function<void(const QString& message)>;

// Append the actions every torrent context menu shares — open magnet link, copy
// info hash, copy magnet link. Callers add their own entries (export, favourites,
// details) afterwards, usually behind a separator.
//
// `context` owns the connections, so the actions stop firing once it dies.
void addTorrentActions(QMenu* menu, QObject* context, const rats::domain::Torrent& torrent, NotifyFn notify = {});

} // namespace ui
} // namespace rats

#endif // RATS_UI_TORRENTMENU_H
