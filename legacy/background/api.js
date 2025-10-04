const ipaddr = require('ipaddr.js');
const forBigTable = require('./forBigTable')
const compareVersions = require('compare-versions');
const getTorrent = require('./getTorrent')
const _ = require('lodash')
const asyncForEach = require('./asyncForEach')
const cpuUsage = require('./bt/cpu-usage-global')
const magnetParse = require('./magnetParse')
const parseTorrentFiles = require('./parsetTorrentFiles')
const {torrentTypeId, torrentIdToType, torrentCategoryId, torrentIdToCategory} = require('../app/content');

module.exports = async ({
	sphinx,
	sphinxSingle,
	send,
	recive,
	p2p,
	config,
	baseRowData,
	torrentClient,
	spider,
	upnp,
	crypto,
	insertTorrentToDB,
	removeTorrentFromDB,
	updateTorrentToDB,
	checkTorrent,
	setupTorrentRecord,
	p2pStore,
	feed,
	updateTorrentTrackers,
	remoteTrackers
}) => {
	let torrentClientHashMap = {}

	let topCache = {};
	setInterval(() => {
		topCache = {};
	}, 24 * 60 * 60 * 1000);


	const mergeTorrentsWithDownloads = (torrents, copy) => {
		if(!torrents)
			return torrents

		if(copy)
			torrents = _.cloneDeep(torrents)

		const mergeTorrent = (torrent) => {
			const id = torrentClientHashMap[torrent.hash]
			if(id)
			{
				const download = torrentClient.get(id)
				torrent.download = {
					received: download.received,
					downloaded: download.downloaded,
					progress: download.progress,
					downloadSpeed: download.downloadSpeed,
                    
					removeOnDone: download.removeOnDone,
					paused: torrent.paused || torrent._paused
				}
				torrent.filesList = download.files.length > 0 ? downloadFilesList(download) : torrent.filesList
			}
		}

		if(Array.isArray(torrents))
		{
			for(const torrent of torrents)
			{
				mergeTorrent(torrent)
			}
		}
		else
		{
			mergeTorrent(torrents)
		}

		return torrents
	}

	const mergeTorrentsWithDownloadsFn = (Fn, copy) => (...args) => { 
		const callback = args[args.length - 2]
		const rest = args.slice(0, -2)
		Fn(...rest, (data) => callback(mergeTorrentsWithDownloads(data, copy)), args[args.length - 1]) 
	}

	const downloadFilesList = (torrent) => torrent.files.map((file, index) => ({
		path: file.path.replace(/\\/g, '/'),
		size: file.length,
		downloadIndex: index,
		downloadSelected: file.selected
	}))


	recive('recentTorrents', function(callback)
	{
		if(typeof callback != 'function')
			return;

		sphinx.query('SELECT * FROM `torrents` ORDER BY added DESC LIMIT 0,10', function (error, rows, fields) {
			if(!rows) {
				callback(undefined)
				return;
			}

			let torrents = [];
			rows.forEach((row) => {
				torrents.push(baseRowData(row));
			});

			callback(torrents)
		});
	});

	recive('statistic', function(callback)
	{
		if(typeof callback != 'function')
			return;

		callback({
			torrents: p2p.info.torrents,
			size: p2p.info.filesSize,
			files: p2p.info.files
		})
	});

	const onTorrent = ({hash, options}, callback) => {
		if(hash.length != 40)
			return;

		if(typeof callback != 'function')
			return;

		// remote request
		if(options.peer)
		{
			logT('search', 'remote torrent request to peer')
			const peer = p2p.find(options.peer)
			if(!peer)
			{
				logT('search', 'dont found requested peer in peers')
				callback(undefined)
				return;
			}
			delete options.peer;
			peer.emit('torrent', {hash, options}, (data, nil, address) => {
				logT('search', 'remote torrent result', hash)
				callback(data)

				if(compareVersions(address.version, '0.19.0') < 0)
				{
					logT('search', 'replication selected torrent now works only with 0.19.0 version, ignore this torrent')
					return
				}

				if(data)
					insertTorrentToDB(data, true) // copy torrent to our db
			})
			return;
		}

		sphinx.query('SELECT * FROM `torrents` WHERE `hash` = ?', hash, async function (error, rows, fields) {
			if(!rows || rows.length == 0) {
				callback(undefined)
				return;
			}
			let torrent = rows[0];

			if(options.files)
			{
				const filesList = await sphinx.query('SELECT * FROM `files` WHERE `hash` = ?', hash);
				if (filesList)
				{
					torrent.filesList = parseTorrentFiles(filesList);
				}
				else
				{
					logTE('search', "can't find filesList for torrent", hash, " - ignore it.")
				}
				callback(baseRowData(torrent))
			}
			else
			{
				callback(baseRowData(torrent))
			}

			// get votes
			const {good, bad, selfVote} = await getVotes(hash)
			send('votes', {
				hash, good, bad, selfVote
			});
			if(torrent.good != good || torrent.bad != bad)
			{
				logT('rating', 'finded new rating on', torrent.name, 'update votes to it')
				torrent.good = good
				torrent.bad = bad
				updateTorrentToDB(torrent)
			}
		});
	}

	recive('torrent', mergeTorrentsWithDownloadsFn(onTorrent));
	p2p.on('torrent', ({hash, options} = {}, callback) => {
		if(!hash)
			return;

		onTorrent(hash, options, (data) => callback(data))
	})

	if(config.p2pReplicationServer)
	{
		logT('replication', 'p2p replication server enabled')

		p2p.on('randomTorrents', (nil, callback) => {
			if(typeof callback != 'function')
				return;
    
			const cpu = cpuUsage()
			const limit = Math.max(1, 5 - (cpu / 20) | 0)

			sphinxSingle.query(`SELECT * FROM torrents ORDER BY rand() limit ${limit}`, (error, torrents) => {
				if(!torrents || torrents.length == 0) {
					callback(undefined)
					return;
				}
    
				let hashes = {}
				for(const torrent of torrents)
				{
					delete torrent.id
					hashes[torrent.hash] = torrent
				}
    
				const inSql = Object.keys(hashes).map(hash => sphinx.escape(hash)).join(',');
				sphinxSingle.query(`SELECT * FROM files WHERE hash IN(${inSql})`, (error, files) => {
					for(const file of files)
						hashes[file.hash].filesList = parseTorrentFiles(file);
                    
					callback(Object.values(hashes))
				})
			})
		});

		if(config.p2pReplication)
		{
			const getReplicationTorrents = (nextTimeout = 5000) => {
				let gotTorrents = 0
				p2p.emit('randomTorrents', null, (torrents, nil, address) => {
					if(!torrents || torrents.length == 0)
						return

					if(compareVersions(address.version, '0.19.0') < 0)
					{
						logT('replication', 'replication now works only with 0.19.0 version, ignore this torrent')
						return
					}

					gotTorrents += torrents.length

					torrents.forEach((torrent) => {
						logT('replication', 'replicate remote torrent', torrent && torrent.name)
						insertTorrentToDB(torrent)
					})
				})

				setTimeout(() => getReplicationTorrents(gotTorrents > 8 ? gotTorrents * 600 : 10000), nextTimeout)
			}
			// start
			logT('replication', 'replication client is enabled')
			getReplicationTorrents()
		}
	}

	const isSH1Hash = (hash) =>
	{
		if(hash.length !== 40)
			return false
		return /[0-9a-f]+/i.test(hash)
	}

	let dhtCheckTimeout;
	const searchTorrentCall = function(text, navigation, callback, isP2P)
	{
		if(typeof callback != 'function')
			return;

		if(!text || text.length <= 2) {
			callback(undefined);
			return;
		}

		// check magnet
		text = magnetParse(text) || text

		const safeSearch = !!navigation.safeSearch;

		const index = navigation.index || 0;
		const limit = navigation.limit || 10;
		let args = [text, index, limit];
		const orderBy = navigation.orderBy;
		let order = '';
		let where = '';
		if(orderBy && orderBy.length > 0)
		{
			const orderDesc = navigation.orderDesc ? 'DESC' : 'ASC';
			args.splice(1, 0, orderBy);
			order = 'ORDER BY ?? ' + orderDesc;
		}
		if(safeSearch)
		{
			where += ` and contentCategory != ${torrentCategoryId('xxx')} `;
		}
		if(navigation.type && navigation.type.length > 0)
		{
			where += ' and contentType = ' + torrentTypeId(navigation.type) + ' ';
		}
		if(navigation.size)
		{
			if(navigation.size.max > 0)
				where += ' and size < ' + sphinx.escape(navigation.size.max) + ' ';
			if(navigation.size.min > 0)
				where += ' and size > ' + sphinx.escape(navigation.size.min) + ' ';
		}
		if(navigation.files)
		{
			if(navigation.files.max > 0)
				where += ' and files < ' + sphinx.escape(navigation.files.max) + ' ';
			if(navigation.files.min > 0)
				where += ' and files > ' + sphinx.escape(navigation.files.min) + ' ';
		}

		if (dhtCheckTimeout) {
			clearTimeout(dhtCheckTimeout);
			dhtCheckTimeout = null;
		}

		let searchList = [];
		const isSHA1 = isSH1Hash(text)
		sphinx.query('SELECT * FROM `torrents` WHERE ' + (isSHA1 ? 'hash = ?' : 'MATCH(?)') + ' ' + where + ' ' + order + ' LIMIT ?,?', args, function (error, rows, fields) {
			if(!rows) {
				logT('search', error)
				callback(undefined)
				return;
			}
			if(rows.length === 0 && isSHA1 && !isP2P) // trying to get via dht
			{
				logT('search', 'get torrent via infohash with dht')
				// 3 try to get torrent from metadata
				const getTorrentMetadata = (tryCount = 4) => {
					if(tryCount <= 0) {
						logT('search', 'dht NOT found anything with dht', text);
						return
					}
					let lock = false;
					dhtCheckTimeout = setTimeout(() => {
						lock = true
						getTorrentMetadata(--tryCount)
					}, 8000);
					torrentClient.getMetadata(text, (torrent) => {
						if(lock) {
							logT('search', 'this dht response not actual for', text);
							return
						}
						clearTimeout(dhtCheckTimeout);
						dhtCheckTimeout = null;
						logT('search', 'dht search found something on try', tryCount)
						searchList.push(baseRowData(torrent));
						callback(searchList);
					})
				}
				getTorrentMetadata();
			}
			else
			{
				rows.forEach((row) => {
					searchList.push(baseRowData(row));
				});
				callback(searchList);
			}
		});
	}

	recive('searchTorrent', mergeTorrentsWithDownloadsFn(({text, navigation}, callback, id) => {
		searchTorrentCall(text, navigation, callback)
		p2p.emit('searchTorrent', {text, navigation}, (remote, socketObject) => {
			logT('search', 'remote search results', remote && remote.length)
			if(remote && remote.length > 0)
			{
				const { _socket: socket } = socketObject
				const peer = { address: socket.remoteAddress, port: socket.remotePort }
				remote = remote.map(torrent => Object.assign(torrent, {peer}))
			}
			send('remoteSearchTorrent', {torrents: mergeTorrentsWithDownloads(remote), id})
		})
	}));

	p2p.on('searchTorrent', ({text, navigation} = {}, callback) => {
		if(!text)
			return;

		searchTorrentCall(text, navigation, (data) => callback(data), true) // 4 args means remote
	})

	const searchFilesCall = function(text, navigation, callback)
	{
		if(typeof callback != 'function')
			return;

		if(!text || text.length <= 2) {
			callback(undefined);
			return;
		}

		const safeSearch = !!navigation.safeSearch;

		const index = navigation.index || 0;
		const limit = navigation.limit || 10;
		let args = [text, text, index, limit];
		const orderBy = navigation.orderBy;
		let order = '';
		let where = '';

		let search = {};
		sphinx.query('SELECT *, SNIPPET(path, ?, \'around=100\', \'force_all_words=1\') as snipplet FROM `files` WHERE MATCH(?) ' + where + ' ' + order + ' LIMIT ?,?', args, function (error, searchTorrents) {
			if(!searchTorrents) {
				logTE('search', error)
				callback(undefined)
				return;
			}
			if(searchTorrents.length === 0)
			{
				logT('search', 'not torrents founded for files search');
				callback(undefined)
				return;
			}
			for(const torrent of searchTorrents)
			{
				if(!search[torrent.hash])
				{
					search[torrent.hash] = { path: [] }
				}
				for(const file of torrent.snipplet.split('\n').filter(text => text.includes('<b>')).slice(0, 25))
				{
					search[torrent.hash].path.push(file)
				}
			}
			const inSql = Object.keys(search).map(hash => sphinx.escape(hash)).join(',');
			sphinx.query(`SELECT * FROM torrents WHERE hash IN(${inSql})`, (err, torrents) => {
				if(!torrents) {
					logTE('search', err)
					return;
				}

				for(const torrent of torrents)
				{
					search[torrent.hash] = Object.assign(baseRowData(torrent), search[torrent.hash])
                    
					// temporary ignore adult content in search (workaroud)
					if(safeSearch && search[torrent.hash].contentCategory == 'xxx')
						delete search[torrent.hash]
				}

				let searchResult = Object.values(search)

				if(orderBy && orderBy.length > 0 && searchResult.length > 0)
				{
					searchResult = _.orderBy(searchResult, [orderBy], [navigation.orderDesc ? 'desc' : 'asc'])
				}

				callback(searchResult);
			})
		});
	}

	recive('searchFiles', mergeTorrentsWithDownloadsFn(({text, navigation}, callback, id) => {
		searchFilesCall(text, navigation, callback)
		p2p.emit('searchFiles', {text, navigation}, (remote, socketObject) => {
			logT('search', 'remote search files results', remote && remote.length)
			if(remote && remote.length > 0)
			{
				const { _socket: socket } = socketObject
				const peer = { address: socket.remoteAddress, port: socket.remotePort }
				remote = remote.map(torrent => Object.assign(torrent, {peer}))
			}
			send('remoteSearchFiles', {torrents: mergeTorrentsWithDownloads(remote), id})
		})
	}));

	p2p.on('searchFiles', ({text, navigation} = {}, callback) => {
		if(!text)
			return;

		searchFilesCall(text, navigation, (data) => callback(data))
	})

	recive('checkTrackers', function({hash})
	{
		if(hash.length != 40)
			return;

		logT('tracker', 'recheck trackers for', hash)
		updateTorrentTrackers(hash);
		remoteTrackers.update({hash, name: hash})
	});

	const topTorrentsCall = (type, navigation = {}, callback) => {
		let where = '';

		const index = parseInt(navigation.index) || 0;
		const limit = parseInt(navigation.limit) || 20;
		const time = navigation.time

		if(type && type.length > 0)
		{
			where += ' and contentType = ' + torrentTypeId(type) + ' ';
		}

		if(time)
		{
			if(time == 'hours')
			{
				where += ' and `added` > ' + (Math.floor(Date.now() / 1000) - (60 * 60 * 24))
			}
			else if(time == 'week')
			{
				where += ' and `added` > ' + (Math.floor(Date.now() / 1000) - (60 * 60 * 24 * 7))
			}
			else if(time == 'month')
			{
				where += ' and `added` > ' + (Math.floor(Date.now() / 1000) - (60 * 60 * 24 * 30))
			}
		}
        
		const query = `SELECT * FROM torrents WHERE seeders > 0 and contentCategory != ${torrentCategoryId('xxx')} ${where} ORDER BY seeders DESC LIMIT ${index},${limit}`;
		if(topCache[query])
		{
			callback(topCache[query]);
			return;
		}
		sphinx.query(query, function (error, rows) {
			if(!rows || rows.length == 0) {
				callback(undefined)
				return;
			}
            
			rows = rows.map((row) => baseRowData(row));
			topCache[query] = rows;
			callback(rows);
		});
	}

	recive('topTorrents', mergeTorrentsWithDownloadsFn(({type, navigation}, callback) =>
	{
		topTorrentsCall(type, navigation, callback)
		p2p.emit('topTorrents', {type, navigation}, (remote, socketObject) => {
			logT('top', 'remote top results', remote && remote.length)
			if(remote && remote.length > 0)
			{
				const { _socket: socket } = socketObject
				const peer = { address: socket.remoteAddress, port: socket.remotePort }
				remote = remote.map(torrent => Object.assign(torrent, {peer}))
			}
			send('remoteTopTorrents', {torrents: mergeTorrentsWithDownloads(remote), type, time: navigation && navigation.time})
		})
	}));

	p2p.on('topTorrents', ({type, navigation} = {}, callback) => {
		topTorrentsCall(type, navigation, (data) => callback(data))
	})

	recive('peers', (callback) =>
	{
		if(typeof callback != 'function')
			return;

		callback({
			size: p2p.peersProtocolSize,
			torrents: p2p.protocolPeersList().reduce((prev, peer) => prev + (peer.info ? peer.info.torrents || 0 : 0), 0)
		})
	});

	recive('p2pStatus', (callback) =>
	{
		if(typeof callback != 'function')
			return;

		callback(p2p.p2pStatus)
	});

	recive('config', (callback) =>
	{
		if(typeof callback != 'function')
			return;

		callback(config)
	});

	recive('setConfig', ({options}, callback) =>
	{
		if(typeof options !== 'object')
			return;

		if(typeof options.indexer !== 'undefined')
		{
			const upSpider = () => {
				if(options.indexer)
					spider.listen(config.spiderPort)
				else
					spider.close()
			}

			if(options.spiderPort !== config.spiderPort)
				spider.close(upSpider)
			else
				upSpider()
		}

		if(upnp)
			upnp.ratsUnmap()

		for(const option in options)
		{
			if(option in config)
				config[option] = options[option]
		}

		if(upnp)
			upnp.ratsMap()

		if(config.p2p)
		{
			spider.announceHashes = [crypto.createHash('sha1').update('degrats-v1').digest()]
		}
		else
		{
			spider.announceHashes = []
		}

		const copyConfig = Object.assign({}, config);
		delete copyConfig['load'];
		delete copyConfig['reload'];
		send('configChanged', copyConfig)
        
		if(typeof callback === 'function')
			callback(true)
	});

	torrentClient._add = (torrentObject, savePath, callback) =>
	{
		const magnet = `magnet:?xt=urn:btih:${torrentObject.hash}`
		logT('downloader', 'download', magnet)
		if(torrentClient.get(magnet)) {
			logT('downloader', 'aready added')
			if(callback)
				callback(false)
			return
		}

		const torrent = torrentClient.add(magnet, {path: savePath || config.client.downloadPath})
		torrentClientHashMap[torrent.infoHash] = magnet
		torrent.torrentObject = torrentObject

		torrent.secondTry = setTimeout(() => {
			logT('downloader', 'second try to find hash', torrent.infoHash)
			torrentClient.remove(magnet, (err) => {
				if(err)
				{
					logTE('downloader', 'cant remove torrent for second try', torrent.infoHash)
					return
				}
				delete torrentClientHashMap[torrentObject.hash];
				torrentClient._add(torrentObject, savePath, callback)
			})
		}, 8000);

		const progress = (bytes) => {
			send('downloadProgress', torrent.infoHash, {
				received: bytes,
				downloaded: torrent.downloaded,
				downloadSpeed: torrent.downloadSpeed,
				progress: torrent.progress,
				timeRemaining: torrent.timeRemaining
			})
		}

		torrent.on('ready', () => {
			logT('downloader', 'start downloading', torrent.infoHash, 'to', torrent.path)
			send('downloading', torrent.infoHash)
			clearTimeout(torrent.secondTry);
			progress(0) // immediately display progress
			if(torrent._paused)
			{
				delete torrent._paused
				torrent._pause()
			}
			send('filesReady', torrent.infoHash, downloadFilesList(torrent))
		})

		torrent.on('done', () => { 
			logT('downloader', 'download done', torrent.infoHash)
			progress(0) // update progress
			// remove torrent if marked
			if(torrent.removeOnDone)
			{
				torrentClient.remove(magnet, (err) => {
					if(err)
					{
						logT('downloader', 'download removing error', err)
						return
					}
        
					delete torrentClientHashMap[torrent.infoHash]
					send('downloadDone', torrent.infoHash)
				})
			}
			else
			{
				send('downloadDone', torrent.infoHash)
			}
		})

		let now = Date.now()
		torrent.on('download', (bytes) => {
			if(Date.now() - now < 100)
				return
			now = Date.now()

			progress(bytes)
		})

		//custom api pause
		torrent._pause = () => {
			logT('downloader', 'pause torrent downloading', torrent.infoHash)
			torrent.pause()
			torrent.wires = [];
			setTimeout(() => {
				if(torrent.paused)
					torrent.wires = [];
			}, 100) // make sure no more wires appears
		}

		torrent._restoreWires = () => {
			for(const p in torrent._peers){
				const wire = torrent._peers[p].wire
				if(wire)
					torrent.wires.push(wire);
			}
		}

		torrent._resume = () => {
			logT('downloader', 'resume torrent downloading', torrent.infoHash)
			torrent._restoreWires()
			torrent.resume()
		}

		// fix wires after pause
		const _destroy = torrent._destroy
		torrent._destroy = (...args) => {
			// fix pause wires closing
			if(torrent.paused)
			{
				torrent._restoreWires()
			}
			return _destroy.call(torrent, ...args)
		}

		torrent.selectFiles = (selection) => {
			if(Array.isArray(selection))
			{
				if(selection.length !== torrent.files.length)
				{
					logTE('downloader', 'selection map not full', torrent.files.length, selection.length)
					return
				}
				for(let i = 0; i < selection.length; i++)
				{
					torrent.files[i].selected = !!selection[i]
				}
			}
			else
			{
				for(let fileId in selection)
				{
					fileId = parseInt(fileId)
					if(fileId >= torrent.files.length)
					{
						logTE('downloader', 'selection map wrong', selection)
						return
					}
					torrent.files[fileId].selected = !!selection[fileId]
				}
			}
			torrent.updateFilesSelection()
		}
        
		torrent.updateFilesSelection = () => {
			torrent.deselect(0, torrent.pieces.length - 1, false)
        
			for(const file of torrent.files)
			{
				const {selected} = file
				if(typeof selected === 'undefined' || selected)
					file.select()
				else
					file.deselect()
			}
			logT('downloader', 'selection updated')
		}


		if(callback)
			callback(true)

		return torrent
	}

	recive('download', ({torrent, savePath}, callback) => torrentClient._add(torrent, savePath, callback));

	recive('downloadUpdate', ({hash, options}) =>
	{
		const id = torrentClientHashMap[hash]
		if(!id)
		{
			logT('downloader', 'cant find torrent for removing', hash)
			return
		}
        
		const torrent = torrentClient.get(id)
		if(!torrent) {
			logT('downloader', 'no torrent for update founded')
			return
		}

		if(options.removeOnDone)
		{
			torrent.removeOnDone = options.removeOnDone == 'switch' ? !torrent.removeOnDone : options.removeOnDone
		}

		if(typeof options.pause !== 'undefined')
		{
			const pause = options.pause == 'switch' ? !torrent.paused : options.pause
			if(pause)
				torrent._pause()
			else
				torrent._resume()
		}

		send('downloadUpdate', torrent.infoHash, {
			removeOnDone: torrent.removeOnDone,
			paused: torrent.paused || torrent._paused
		})
	})

	recive('downloadCancel', ({hash}, callback) =>
	{
		const id = torrentClientHashMap[hash]
		if(!id)
		{
			logT('downloader', 'cant find torrent for removing', hash)
			if(callback)
				callback(false)
			return
		}
		clearTimeout(torrentClient.get(id).secondTry)
		torrentClient.remove(id, (err) => {
			if(err)
			{
				logT('downloader', 'download removing error', err)
				if(callback)
					callback(false)
				return
			}

			delete torrentClientHashMap[hash]
			send('downloadDone', hash, true)

			if(callback)
				callback(true)
		})
	})

	recive('downloadSelectFiles', ({hash, files}, callback) =>
	{
		logT('downloader', 'call update selection', hash, files.length)
		const id = torrentClientHashMap[hash]
		if(!id)
		{
			logT('downloader', 'cant find torrent for selection', hash)
			if(callback)
				callback(false)
			return
		}

		const torrent = torrentClient.get(id)
		if(!torrent) {
			logT('downloader', 'no torrent for selection founded')
			return
		}

		torrent.selectFiles(files)
		send('filesReady', torrent.infoHash, downloadFilesList(torrent))

		if(callback)
			callback(true)
	})

	recive('downloads', (callback) =>
	{
		callback(torrentClient.torrents.map(torrent => ({
			torrentObject: torrent.torrentObject,
			received: torrent.received,
			downloaded: torrent.downloaded,
			progress: torrent.progress,
			downloadSpeed: torrent.downloadSpeed,
            
			removeOnDone: torrent.removeOnDone,
			paused: torrent.paused || torrent._paused
		})))
	})

	let removeProtect = false
	recive('removeTorrents', ({checkOnly}, callback) =>
	{
		if(removeProtect)
			return
		removeProtect = true

		logT('clean', 'checktorrents call')

		const toRemove = []

		const done = async () => {
			logT('clean', 'torrents to remove founded', toRemove.length)
			if(checkOnly)
			{
				callback(toRemove.length)
				removeProtect = false
				return
			}

			await asyncForEach(toRemove, async (torrent, index) => {
				await removeTorrentFromDB(torrent)
				send('cleanTorrent', index + 1, toRemove.length, 'clean');
			})
			callback(toRemove.length)
			removeProtect = false
			logT('clean', 'removed torrents by filter:', toRemove.length)
		}

		let i = 1
		forBigTable(sphinx, 'torrents', (torrent) => {
			setupTorrentRecord(torrent)
			if(!checkTorrent(torrent))
			{
				toRemove.push({hash: torrent.hash})
				// send info about cleaning takes
				send('cleanTorrent', i++, 0, 'check');
			}
		}, done)
	})

	const getVotes = async (hash) => {
		const votes = await p2pStore.find(`vote:${hash}`)
		let good = 0
		let bad = 0
		let selfVote = false
		if(votes)
		{
			votes.forEach(({vote, _peerId}) => {
				if(_peerId === p2p.peerId)
					selfVote = true

				if(vote == 'bad')
					bad++
				else
					good++
			})
		}

		return {good, bad, selfVote}
	}

	recive('vote', async ({hash, isGood}, callback) =>
	{
		if(hash.length != 40)
			return;

		if(typeof callback != 'function')
			return;

		isGood = !!isGood;

		const action = isGood ? 'good' : 'bad';
		let {good, bad, selfVote} = await getVotes(hash)

		if(!selfVote)
		{
			setTimeout(async () => p2pStore.store({
				type: 'vote',
				torrentHash: hash,
				vote: action,
				_index: `vote:${hash}`,
				_temp: {
					torrent: setupTorrentRecord(await getTorrent(sphinx, hash))
				}
			}), 0)
			good += isGood ? 1 : 0
			bad += !isGood ? 1 : 0
		}

		send('votes', {
			hash, good, bad
		});
		callback(true)

	});

	// store torrent to feed
	await feed.load()
	Object.defineProperty(p2p.info, 'feed', { 
		enumerable: true,
		get: () => feed.size()
	});
	Object.defineProperty(p2p.info, 'feedDate', { 
		enumerable: true,
		get: () => feed.feedDate
	});
	p2pStore.on('store', async ({data: record, temp, myself}) => {
		if(record.type !== 'vote')
			return

		if(!temp || !temp.torrent)
			return
    
		const { torrent } = temp
        
		if(torrent.hash !== record.torrentHash)
			return

		if(!myself)
		{
			logT('store', 'replicate torrent from store record', torrent.hash)
			await insertTorrentToDB(torrent)
		}

		let {good, bad} = await getVotes(torrent.hash)
		torrent.good = good
		torrent.bad = bad
		if(torrent.good > 0 || torrent.bad > 0)
			updateTorrentToDB(torrent)

		// update feed only on some good info
		if(torrent.good < 1)
			return
        
		feed.add(torrent)
		send('feedUpdate', {
			feed: feed.feed
		});
	})

	const feedCall = ({index}, callback) =>
	{
		callback(feed.feed.slice(index || 0, (index || 0) + 20));
	}
	recive('feed', mergeTorrentsWithDownloadsFn(feedCall, true)); // don't overwrite feed value

	p2p.on('feed', (nil, callback) => {
		callback({
			feed: feed.feed,
			feedDate: feed.feedDate
		})
	})

	// call once to get bigest feed
	p2p.events.on('peer', (peer) => {
		if(peer.info && peer.info.feed)
		{
			if(peer.info.feed > feed.size() // list bigger than our
            || (peer.info.feed == feed.size() && peer.info.feedDate > feed.feedDate)) // or same but more new
			{
				peer.emit('feed', null, (remoteFeed) => {
					if(!remoteFeed)
						return
            
					if(Array.isArray(remoteFeed) || !remoteFeed.feed)
						return // old version call

					if(remoteFeed.feed.length > feed.max) {
						logT('feed', 'remote feed have more torrent that needed: ', remoteFeed.feed.length, ' > ', feed.max);
						remoteFeed.feed = remoteFeed.feed.slice(0, feed.max);
					}
            
					if(remoteFeed.feed.length > feed.size() || (remoteFeed.feed.length == feed.size() && remoteFeed.feedDate > feed.feedDate))
					{
						logT('feed', 'replace our feed with remote feed')
						feed.feed = remoteFeed.feed
						feed.feedDate = remoteFeed.feedDate || 0
						// it can be new torrents replicate all
						feed.feed.forEach(torrent => insertTorrentToDB(torrent, true))
						send('feedUpdate', {
							feed: feed.feed
						});
					}
				});
			}
		}
	})
    
}