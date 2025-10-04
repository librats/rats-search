#include "torrentclient.h"
#include <QDebug>

TorrentClient::TorrentClient(QObject *parent)
    : QObject(parent)
{
}

TorrentClient::~TorrentClient()
{
}

void TorrentClient::downloadTorrent(const QString& magnetLink)
{
    qDebug() << "Download torrent:" << magnetLink;
    // TODO: Implement torrent downloading
}

void TorrentClient::stopTorrent(const QString& infoHash)
{
    qDebug() << "Stop torrent:" << infoHash;
    // TODO: Implement torrent stopping
}

