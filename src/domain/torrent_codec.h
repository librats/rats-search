#ifndef RATS_DOMAIN_TORRENT_CODEC_H
#define RATS_DOMAIN_TORRENT_CODEC_H

#include "domain/torrent.h"

#include <QJsonArray>
#include <QJsonObject>

// The single source of truth for turning torrents into JSON and back. Both the
// REST/WS API and the P2P peer API serialise through these, so the two wire
// formats cannot drift apart.
namespace rats::domain::codec {

struct ToJsonOptions {
    bool includeFiles = false; // embed the "files" array (path/size objects)
    bool includeInfo = true; // embed the scraped "info" object when non-empty
};

QJsonObject toJson(const Torrent& torrent, ToJsonOptions options = {});
QJsonObject toJson(const SearchHit& hit, ToJsonOptions options = {});

// Tolerant parser: accepts either "hash" or the legacy "info_hash" key, parses
// content type/category from their string names, and reads an embedded "files"
// array if present.
Torrent torrentFromJson(const QJsonObject& obj);
SearchHit searchHitFromJson(const QJsonObject& obj);

QJsonArray filesToJson(const QVector<File>& files);
QVector<File> filesFromJson(const QJsonArray& array);

} // namespace rats::domain::codec

#endif // RATS_DOMAIN_TORRENT_CODEC_H
