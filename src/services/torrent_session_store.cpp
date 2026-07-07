#include "services/torrent_session_store.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace rats::service {

bool TorrentSessionStore::save(const QString& filePath, const QVector<Download>& downloads) const
{
    if (downloads.isEmpty()) {
        QFile::remove(filePath);
        return true;
    }

    QJsonArray sessionsArray;
    for (const Download& d : downloads) {
        QJsonObject session;
        session["hash"] = d.hash;
        session["name"] = d.name;
        session["savePath"] = d.savePath;
        session["totalSize"] = d.totalSize;
        session["paused"] = d.paused;
        session["removeOnDone"] = d.removeOnDone;
        session["completed"] = d.completed; // completed torrents are kept for seeding
        session["downloadedBytes"] = d.downloadedBytes;
        session["progress"] = d.progress;

        // Reuse the shared file->JSON helper so the persisted per-file shape
        // (including `progress`) matches the live progress JSON.
        QJsonArray filesArr;
        for (const DownloadFile& f : d.files) {
            filesArr.append(f.toJson());
        }
        session["files"] = filesArr;

        sessionsArray.append(session);
    }

    // Ensure the target directory exists.
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "TorrentSessionStore: Failed to save session to" << filePath;
        return false;
    }

    QJsonDocument doc(sessionsArray);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qInfo() << "TorrentSessionStore: Saved" << sessionsArray.size() << "torrents to session file";
    return true;
}

QVector<Download> TorrentSessionStore::load(const QString& filePath) const
{
    QVector<Download> entries;

    QFile file(filePath);
    if (!file.exists()) {
        return entries;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "TorrentSessionStore: Failed to open session file:" << filePath;
        return entries;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "TorrentSessionStore: Failed to parse session file:" << parseError.errorString();
        return entries;
    }
    if (!doc.isArray()) {
        qWarning() << "TorrentSessionStore: Invalid session file format";
        return entries;
    }

    for (const QJsonValue& val : doc.array()) {
        if (!val.isObject()) {
            continue;
        }
        QJsonObject session = val.toObject();

        Download d;
        d.hash = session["hash"].toString();
        d.name = session["name"].toString();
        d.savePath = session["savePath"].toString();
        d.totalSize = session["totalSize"].toVariant().toLongLong();
        d.paused = session["paused"].toBool();
        d.removeOnDone = session["removeOnDone"].toBool();
        d.completed = session["completed"].toBool();
        d.downloadedBytes = session["downloadedBytes"].toVariant().toLongLong();
        d.progress = session["progress"].toDouble();

        const QJsonArray filesArr = session["files"].toArray();
        for (int i = 0; i < filesArr.size(); ++i) {
            QJsonObject fileObj = filesArr[i].toObject();
            DownloadFile f;
            f.path = fileObj["path"].toString();
            f.size = fileObj["size"].toVariant().toLongLong();
            f.index = fileObj.contains("index") ? fileObj["index"].toInt() : i;
            f.selected = fileObj["selected"].toBool(true);
            f.progress = fileObj["progress"].toDouble();
            d.files.append(f);
        }

        entries.append(d);
    }

    return entries;
}

} // namespace rats::service
