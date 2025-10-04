const WebTorrent = require('webtorrent')
const fs = require('fs')

const torrentClient = new WebTorrent({webSeeds: false})
torrentClient.saveSession = (sessionFile) => {
	fs.writeFileSync(sessionFile, JSON.stringify({
		torrents: torrentClient.torrents.map(torrent => ({
			infoHash: torrent.infoHash,
			path: torrent.path,
			torrent: torrent.torrentObject,

			removeOnDone: torrent.removeOnDone,
			paused: torrent.paused || torrent._paused,
			selection: torrent.files.map(file => typeof file.selected === 'undefined' || file.selected)
		}))
	}, null, 4), 'utf8');
}
torrentClient.loadSession = (sessionFile) => {
	if(!fs.existsSync(sessionFile))
	{
		logT('downloader', 'no download sessions - ignore')
		return
	}

	const data = fs.readFileSync(sessionFile, 'utf8')
	const obj = JSON.parse(data);
	if(!obj.torrents)
	{
		logT('downloader', 'no torrents list for loading session')
		return
	}
	if(!torrentClient._add)
	{
		logT('downloader', 'no overriden _add() method')
		return
	}
	const {torrents} = obj
	torrents.forEach(({torrent, infoHash, path, removeOnDone, paused, selection}) => {
		if(!torrent || !infoHash || !path)
		{
			logT('downloader', 'no info for starting download this torrent')
			return
		}
		logT('downloader', 'restore download session:', torrent.name)
		const download = torrentClient._add(torrent, path)
		if(download)
		{
			logT('downloader', 'restore options')
			// restore options
			download.removeOnDone = removeOnDone
			if(paused)
			{
				download._paused = true
			}
			if(selection)
			{
				download.on('metadata', () => {
					logT('downloader', 'load torrent selection from session')
					download.selectFiles(selection)
				})
			}
		}
	})
}

const metaHashes = {}

torrentClient.dht.on('peer', (peer, infoHash) => {
	const hash = infoHash.toString('hex')
	if(!(hash in metaHashes))
		return

	if(torrentClient._downloader)
	{
		torrentClient._downloader(peer, infoHash, (...data) => {
			if(metaHashes[hash])
				metaHashes[hash](...data)
            
			delete metaHashes[hash]
		})
	}
	else
	{
		delete metaHashes[hash]
	}
})

torrentClient.getMetadata = (hash, callback) => {
	hash = hash.toLowerCase()
	torrentClient.dht.lookup(hash)
	metaHashes[hash] = callback
}

module.exports = torrentClient