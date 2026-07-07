#include "domain/torrent.h"

#include "common/infohash.h"

#include <QUrl>

namespace rats::domain {

bool Torrent::isValid() const
{
    return infohash::isValid(hash);
}

QString Torrent::magnetLink() const
{
    return QStringLiteral("magnet:?xt=urn:btih:%1&dn=%2").arg(hash, QString::fromLatin1(QUrl::toPercentEncoding(name)));
}

} // namespace rats::domain
