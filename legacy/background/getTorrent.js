const parseTorrentFiles = require('./parsetTorrentFiles')

module.exports = async (sphinx, hash, torrent) => {
	torrent = (torrent && [torrent]) || await sphinx.query(`SELECT * FROM torrents WHERE hash = '${hash}'`)
	if(torrent && torrent.length > 0)
	{
		torrent[0].filesList = (await sphinx.query(`SELECT * FROM files WHERE hash = '${torrent[0].hash}'`)) || []
		torrent[0].filesList = parseTorrentFiles(torrent[0].filesList)
		return torrent[0]
	}
}