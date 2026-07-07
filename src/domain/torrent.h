#ifndef RATS_DOMAIN_TORRENT_H
#define RATS_DOMAIN_TORRENT_H

#include "domain/content.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace rats::domain {

// A single file inside a torrent.
struct File {
    QString path;
    qint64 size = 0;
};

// A torrent as indexed by the search engine. This is a pure domain entity: it
// carries no search-result decoration (highlighted snippets, source peer) and
// no persistence detail (the full-text `nameIndex` is built by the repository).
// Content type/category are strong enums, not duplicated strings.
struct Torrent {
    qint64 id = 0;
    QString hash; // 40-char lower-case hex info-hash
    QString name;
    qint64 size = 0;
    int files = 0;
    int pieceLength = 0;
    QDateTime added;
    QString ipv4; // address that announced the torrent on the DHT
    int port = 0;
    ContentType contentType = ContentType::Unknown;
    ContentCategory contentCategory = ContentCategory::Unknown;
    int seeders = 0;
    int leechers = 0;
    int completed = 0;
    QDateTime trackersChecked;
    int good = 0; // up-votes
    int bad = 0; // down-votes
    QJsonObject info; // scraped extras: poster, description, tracker payloads
    QVector<File> fileList;

    bool isValid() const;
    QString magnetLink() const;
};

// A search result: a torrent plus the metadata specific to *how* it was found.
// Kept separate from Torrent so the entity stays clean and the search layer owns
// its own concerns (file-match highlighting, remote-peer provenance).
struct SearchHit {
    Torrent torrent;
    bool fromFileMatch = false; // matched on a file path rather than the name
    QStringList matchingPaths; // highlighted <b>…</b> file-path snippets
    QString sourcePeerId; // non-empty if this hit came from a remote peer
    bool remote = false;
};

} // namespace rats::domain

// Registered so these can travel across threads through queued signal/slot
// connections (e.g. the crawler emitting discovered() from a worker thread).
Q_DECLARE_METATYPE(rats::domain::Torrent)
Q_DECLARE_METATYPE(rats::domain::SearchHit)

#endif // RATS_DOMAIN_TORRENT_H
