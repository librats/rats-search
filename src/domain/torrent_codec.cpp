#include "domain/torrent_codec.h"

#include <QJsonValue>

namespace rats::domain::codec {

QJsonArray filesToJson(const QVector<File>& files)
{
    QJsonArray array;
    for (const File& f : files) {
        QJsonObject obj;
        obj["path"] = f.path;
        obj["size"] = f.size;
        array.append(obj);
    }
    return array;
}

QVector<File> filesFromJson(const QJsonArray& array)
{
    QVector<File> files;
    files.reserve(array.size());
    for (const QJsonValue& v : array) {
        const QJsonObject obj = v.toObject();
        files.append(File { obj["path"].toString(), obj["size"].toVariant().toLongLong() });
    }
    return files;
}

QJsonObject toJson(const Torrent& t, ToJsonOptions options)
{
    QJsonObject obj;
    obj["hash"] = t.hash;
    obj["name"] = t.name;
    obj["size"] = t.size;
    obj["files"] = t.files;
    obj["pieceLength"] = t.pieceLength;
    obj["added"] = t.added.isValid() ? t.added.toMSecsSinceEpoch() : 0;
    obj["contentType"] = toString(t.contentType);
    obj["contentCategory"] = toString(t.contentCategory);
    obj["seeders"] = t.seeders;
    obj["leechers"] = t.leechers;
    obj["completed"] = t.completed;
    obj["trackersChecked"] = t.trackersChecked.isValid() ? t.trackersChecked.toMSecsSinceEpoch() : 0;
    obj["good"] = t.good;
    obj["bad"] = t.bad;

    if (options.includeInfo && !t.info.isEmpty())
        obj["info"] = t.info;
    if (options.includeFiles)
        obj["files_list"] = filesToJson(t.fileList);

    return obj;
}

QJsonObject toJson(const SearchHit& hit, ToJsonOptions options)
{
    QJsonObject obj = toJson(hit.torrent, options);
    if (hit.fromFileMatch)
        obj["fileMatch"] = true;
    if (!hit.matchingPaths.isEmpty()) {
        QJsonArray paths;
        for (const QString& p : hit.matchingPaths)
            paths.append(p);
        obj["matchingPaths"] = paths;
    }
    if (!hit.sourcePeerId.isEmpty())
        obj["peer"] = hit.sourcePeerId;
    if (hit.remote)
        obj["remote"] = true;
    return obj;
}

Torrent torrentFromJson(const QJsonObject& obj)
{
    Torrent t;
    t.hash = obj["hash"].toString().toLower();
    if (t.hash.isEmpty())
        t.hash = obj["info_hash"].toString().toLower(); // legacy alias

    t.name = obj["name"].toString();
    t.size = obj["size"].toVariant().toLongLong();
    t.files = obj["files"].toInt();
    t.pieceLength = obj["pieceLength"].toInt();
    t.seeders = obj["seeders"].toInt();
    t.leechers = obj["leechers"].toInt();
    t.completed = obj["completed"].toInt();
    t.good = obj["good"].toInt();
    t.bad = obj["bad"].toInt();

    const qint64 addedMs = obj["added"].toVariant().toLongLong();
    t.added = addedMs > 0 ? QDateTime::fromMSecsSinceEpoch(addedMs) : QDateTime::currentDateTime();

    const qint64 checkedMs = obj["trackersChecked"].toVariant().toLongLong();
    if (checkedMs > 0)
        t.trackersChecked = QDateTime::fromMSecsSinceEpoch(checkedMs);

    t.contentType = contentTypeFromString(obj["contentType"].toString());
    t.contentCategory = contentCategoryFromString(obj["contentCategory"].toString());

    if (obj.contains("info"))
        t.info = obj["info"].toObject();

    if (obj.contains("files_list"))
        t.fileList = filesFromJson(obj["files_list"].toArray());
    else if (obj.contains("filesList")) // legacy key
        t.fileList = filesFromJson(obj["filesList"].toArray());

    if (t.files == 0 && !t.fileList.isEmpty())
        t.files = t.fileList.size();

    return t;
}

SearchHit searchHitFromJson(const QJsonObject& obj)
{
    SearchHit hit;
    hit.torrent = torrentFromJson(obj);
    hit.fromFileMatch = obj["fileMatch"].toBool(false) || obj["isFileMatch"].toBool(false);
    for (const QJsonValue& v : obj["matchingPaths"].toArray())
        hit.matchingPaths.append(v.toString());
    hit.sourcePeerId = obj["peer"].toString();
    hit.remote = obj["remote"].toBool(false) || !hit.sourcePeerId.isEmpty();
    return hit;
}

} // namespace rats::domain::codec
