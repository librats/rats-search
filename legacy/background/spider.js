const config = require('./config');
const client = new (require('./bt/client'))
const spider = new (require('./bt/spider'))(client)
const fs = require('fs');
const parseTorrent = require('parse-torrent')
const {single, pool} = require('./mysql')
const getPeersStatisticUDP = require('./bt/udp-tracker-request')
const crypto = require('crypto')
const EventEmitter = require('events');
const P2PServer = require('./p2p')
const P2PStore = require('./store')
const stun = require('stun')
const natUpnp = require('nat-upnp');
const http = require('https')
const API = require('./api')
const Feed = require('./feed')
//var sm = require('sitemap');
//var phantomjs = require('phantomjs-prebuilt')
//const disk = require('diskusage');
const encryptor = require('simple-encryptor')('rats-on-the-boat-enc-v0');
const os = require('os');
let rootPath = os.platform() === 'win32' ? 'c:' : '/';

const _debug = require('debug')
const cleanupDebug = _debug('main:cleanup');
const balanceDebug = _debug('main:balance');
const quotaDebug = _debug('main:quota');

const checkInternet = require('./checkInternet')

const {torrentTypeDetect, torrentTypeId, torrentIdToType, torrentCategoryId, torrentIdToCategory} = require('../app/content');

const torrentClient = require('./torrentClient')
const directoryFilesRecursive = require('./directoryFilesRecursive')
const _ = require('lodash')
const mime = require('mime');

// Start server
//server.listen(config.httpPort);
//console.log('Listening web server on', config.httpPort, 'port')

module.exports = function (send, recive, dataDirectory, version, env, {version: manticoreVersion})
{
	this.initialized = (async () =>
	{
		recive('log', (...log) => {
			logT('gui', ...log)
		})

		recive('logE', (...log) => {
			logTE('gui', ...log)
		})

		let torrentsId = 1;
		let filesId = 1;

		const events = new EventEmitter
		let sphinx = await pool();

		// initialize p2p
		const p2p = new P2PServer()
		p2p.version = version
		p2p.dataDirectory = dataDirectory // make file transfer work
		p2p.filesBlacklist = [
			'rats.json',
			'rats.log',
			'downloads.json',
			'Cookies',
			'Cookies-journal',
			'searchd',
			'Preferences',
			'Local Storage',
			'IndexedDB',
			'GPUCache',
			'Cache',
			'blob_storage',
			'peers.p2p',
			'query.log',
			'sphinx.conf'
		]
		let p2pStatus = 0;
		p2p.events.on('peer', ({peer, size}) => {
			send('peer', {
				torrents: peer.info ? peer.info.torrents || 0 : 0,
				size: size
			})
			if (p2pStatus != 2) {
				p2pStatus = 2;
				send('p2pStatus', p2pStatus);
			}
		})
		p2p.events.on('peerDisconnect', ({peer, size}) => {
			send('peer', {
				torrents: peer.info ? peer.info.torrents || 0 : 0,
				size: size
			})
			if (p2pStatus != 0) {
				p2pStatus = 0;
				send('p2pStatus', p2pStatus);
			}
		})
		// Start p2p
		await p2p.listen()
		const p2pStore = new P2PStore(p2p, sphinx)

		const udpTrackers = [
			{
				host: 'tracker.coppersurfer.tk',
				port: 6969
			},
			{
				host: 'tracker.leechers-paradise.org',
				port: 6969
			},
			{
				host: 'tracker.opentrackr.org',
				port: 1337
			},
			{
				host: '9.rarbg.me',
				port: 2710
			},
			{
				host: 'open.stealth.si',
				port: 80
			}
		]

		const sphinxSingle = await single().waitConnection()
		let torrentsInfo = await sphinxSingle.query(`
            SELECT 
                MAX(id) as maxid,
                COUNT(*) as torrentscount,
                SUM(files) as numfiles,
                SUM(size) as filessize
            FROM torrents
        `);
		let filesInfo = await sphinxSingle.query(`
            SELECT 
                MAX(id) as maxid
            FROM files
        `);
		if(torrentsInfo && torrentsInfo[0]) 
		{
			torrentsInfo = torrentsInfo[0]
			torrentsId = (torrentsInfo.maxid || 0) + 1
			p2p.info.torrents = Number(torrentsInfo.torrentscount) || 0
			p2p.info.files = Number(torrentsInfo.numfiles) || 0
			p2p.info.filesSize = Number(torrentsInfo.filessize) || 0
		}
		else
		{
			torrentsId = 1;
			p2p.info.torrents = 0;
			p2p.info.files = 0;
			p2p.info.filesSize = 0;
		}

		p2p.info.manticoreVersion = manticoreVersion;

		if(filesInfo && filesInfo[0])
		{
			filesInfo = filesInfo[0]
			filesId = (filesInfo.maxid || 0) + 1
		}
		else
		{
			filesId = 1;
		}
		const sphinxSingleAlternative = await single().waitConnection()
        
        
		class RemoteTrackers
		{
			constructor(args)
			{
				this.sphinx = args.sphinx
				if(!config.trackers)
				{
					logT('tracker', 'trackers disabled')
					this.trackers = []
					return
				}

				this.trackers = []
				if(process.versions.electron) {
					let strategies = require.context('./strategies', false, /\.js$/);
					strategies.keys().forEach(strategie => {
						this.trackers.push(new (strategies(strategie))(args))
						logT('tracker', 'loaded strategie', strategie)
					})
				} else {
					fs.readdirSync(__dirname + '/strategies').forEach((strategie) => {
						this.trackers.push(new (require('./strategies/' + strategie))(args))
						logT('tracker', 'loaded strategie', strategie)
					})
				}
			}

			findHash(hash, callback)
			{
				for(const tracker of this.trackers)
					if(tracker.findHash)
						tracker.findHash(hash).then(data => callback(tracker.name, data))
			}

			async close()
			{
				for(const tracker of this.trackers)
					if(tracker.close)
						await tracker.close()
			}

			update({hash, name})
			{
				this.findHash(hash, (tracker, data) => {
					if(!data)
						return

					logT('tracker', 'found', name, 'on', tracker)
					let info;
					this.sphinx.replaceValues('torrents', {hash, info: data}, {
						particial: true,
						key: 'hash', 
						sphinxIndex: {nameIndex: (obj) => buildTorrentIndex(obj)},
						merge: ['info'], 
						mergeCallback: (n, obj) => {
							if(n != 'info')
								return

							if(!obj.trackers)
								obj.trackers = []
							obj.trackers.push(tracker)
							obj.trackers = [...new Set(obj.trackers)]

							info = obj
						} }).then(() => {
						send('trackerTorrentUpdate', {
							hash,
							info
						});
					})          
				})
			}
		}
		const remoteTrackers = new RemoteTrackers({
			sphinx: sphinxSingle,
			p2p,
			dataDirectory
		})

		// start
		function baseRowData(row)
		{
			return {
				hash: row.hash,
				name: row.name,
				size: row.size,
				files: row.files,
				filesList: row.filesList,
				piecelength: row.piecelength,
				added: row.added ? (typeof row.added === 'object' ? row.added.getTime() : row.added) : (new Date()).getTime(),
				contentType: torrentIdToType(row.contentType || row.contenttype),
				contentCategory: torrentIdToCategory(row.contentCategory || row.contentcategory),
				seeders: row.seeders,
				completed: row.completed,
				leechers: row.leechers,
				trackersChecked: row.trackerschecked || row.trackersChecked,
				good: row.good,
				bad: row.bad,
				info: typeof row.info == 'string' && row.info.length > 0 ? JSON.parse(row.info) : undefined 
			}
		}

		// load initial peers
		if(dataDirectory && fs.existsSync(dataDirectory + '/peers.p2p'))
		{
			const peersEncrypted = fs.readFileSync(dataDirectory + '/peers.p2p', 'utf8')
			const peers = encryptor.decrypt(peersEncrypted)
			if(peers && peers.length > 0)
			{
				peers.forEach(peer => p2p.add(peer))
				logT('p2p', 'loaded', peers.length, 'peers')
			}
		}

		const getServiceJson = (url) => new Promise((resolve) => {
			let timeout;
			const req = http.get(url, (resp) => {
				let data = '';

				resp.on('data', (chunk) => {
					data += chunk;
				});

				resp.on('end', () => {
					clearTimeout(timeout);
					try {
						resolve(data.length > 0 && JSON.parse(data))
					} catch(e) {
						logTE('p2p', 'loading bootstrap failed', e)
						resolve(false)
					}
				});
			});
			req.on("error", (err) => {
				clearTimeout(timeout);
				logTE('http', `${url} error: ` + err.message)
				resolve(false)
			});
			timeout = setTimeout(() => {
				logTE('http', `${url} abort by time`)
				req.destroy();
			}, 3000) 
		})

		let p2pBootstrapLoop = null
		if(config.p2p && config.p2pBootstrap)
		{
			const loadBootstrapPeers = async (url) => {
				const json = await getServiceJson(url)
				if(json.bootstrap)
				{
					const peers = encryptor.decrypt(json.bootstrap)
					if(peers && peers.length > 0)
					{
						peers.forEach(peer => p2p.add(peer))
						logT('p2p', 'loaded', peers.length, 'peers from bootstrap from host', url)
					}
				}
				if(json.bootstrapMap)
				{
					const peersMap = encryptor.decrypt(json.bootstrapMap)
					if(typeof peersMap === 'object')
					{
						for(const map in peersMap)
						{
							if(parseInt(map) <= 0)
								continue // break if this is not number

							const peers = peersMap[map]
							if(peers.length > 0)
							{
								peers.forEach(peer => p2p.add(peer))
							}
						}
					}
					logT('p2p', 'loaded peers map from bootstrap from host', url)
				}
				if(json.relays)
				{
					const relays = encryptor.decrypt(json.relays)
					if(Array.isArray(relays) && relays.length > 0)
					{
						relays.forEach(peer => p2p.add(peer, true))
					}
					logT('relay', 'loaded relays', relays.length, 'from bootstrap from host', url)
				}
			}

			const loadBootstrap = () => {
				checkInternet((connected) => {
					if(!connected)
						return

					// loadBootstrapPeers('https://api.myjson.com/bins/1e5rmh')
					// loadBootstrapPeers('https://jsonblob.com/api/jsonBlob/4d22c8ba-5046-11eb-b13f-81fd0496c154')
					// loadBootstrapPeers('https://getpantry.cloud/apiv1/pantry/2f760eeb-2e76-4ea0-804b-6032223086e1/basket/testBasket')  
				})
			}

			// first bootstrap load
			loadBootstrap()
			p2pBootstrapLoop = setInterval(() => {
				if(p2p.size === 0)
				{
					logT('p2p', 'load peers from bootstap again because no peers at this moment')
					loadBootstrap()
				}
			}, 90000) // try to load new peers if there is no one found
		}

		const updateTorrentTrackers = (hash) => {
			let maxSeeders = 0, maxLeechers = 0, maxCompleted = 0;
			sphinxSingle.query('UPDATE torrents SET trackersChecked = ? WHERE hash = ?', [Math.floor(Date.now() / 1000), hash], (err, result) => {
				if(!result) {
					console.error(err);
					return
				}

				udpTrackers.forEach((tracker) => {
					getPeersStatisticUDP(tracker.host, tracker.port, hash, ({seeders, completed, leechers}) => {
						if(this.closing) // ignore trackers response if app is closing
							return

						if(seeders == 0 && completed == 0 && leechers == 0)
							return;

						if(seeders < maxSeeders)
						{
							return;
						}
						if(seeders == maxSeeders && leechers < maxLeechers)
						{
							return;
						}
						if(seeders == maxSeeders && leechers == maxLeechers && completed <= maxCompleted)
						{
							return;
						}
						maxSeeders = seeders;
						maxLeechers = leechers;
						maxCompleted = completed;
						let checkTime = new Date();

						sphinxSingle.query('UPDATE torrents SET seeders = ?, completed = ?, leechers = ?, trackersChecked = ? WHERE hash = ?', [seeders, completed, leechers, Math.floor(checkTime.getTime() / 1000), hash], function(err, result) {
							if(!result) {
								logTE('udp-tracker', err);
								return
							}

							send('trackerTorrentUpdate', {
								hash,
								seeders,
								completed,
								leechers,
								trackersChecked: checkTime.getTime()
							});
						});
					});
				});
			});
		}

		const cleanupTorrents = (cleanTorrents = 1) => {
			if(!config.cleanup)
				return;

			/*
    disk.check(rootPath, function(err, info) {
        if (err) {
            console.log(err);
        } else {
            const {available, free, total} = info;
            
            if(free < config.cleanupDiscLimit)
            {
                sphinxSingle.query(`SELECT * FROM torrents WHERE added < DATE_SUB(NOW(), INTERVAL 6 hour) ORDER BY seeders ASC, files DESC, leechers ASC, completed ASC LIMIT ${cleanTorrents}`, function(err, torrents) {
                    if(!torrents)
                        return;

                    torrents.forEach((torrent) => {
                        if(torrent.seeders > 0){
                            cleanupDebug('this torrent is ok', torrent.name);
                            return
                        }

                        cleanupDebug('cleanup torrent', torrent.name, '[seeders', torrent.seeders, ', files', torrent.files, ']', 'free', (free / (1024 * 1024)) + "mb");
                        
                        sphinxSingle.query('DELETE FROM files WHERE hash = ?', torrent.hash);
                        sphinxSingle.query('DELETE FROM torrents WHERE hash = ?', torrent.hash);
                    })
                });
            }
            else
                cleanupDebug('enough free space', (free / (1024 * 1024)) + "mb");
        }
    });
    */
		}

		const checkTorrent = (torrent) => {
			if(config.filters.maxFiles > 0 && torrent.files > config.filters.maxFiles)
			{
				logT('check', 'ignore', torrent.name, 'because files', torrent.files, '>', config.filters.maxFiles)
				return false
			}

			const nameRX = config.filters.namingRegExp && config.filters.namingRegExp.trim()
			if(nameRX && nameRX.length > 0)
			{
				const rx = new RegExp(nameRX, 'i')
				if(!config.filters.namingRegExpNegative && !rx.test(torrent.name))
				{
					logT('check', 'ignore', torrent.name, 'by naming rx')
					return false
				}
				else if(config.filters.namingRegExpNegative && rx.test(torrent.name))
				{
					logT('check', 'ignore', torrent.name, 'by naming rx negative')
					return false
				}
			}

			if(torrentIdToType(torrent.contentType) === 'bad')
			{
				logT('check', 'ignore torrent', torrent.name, 'because this is a bad thing')
				return false
			}

			if(config.filters.adultFilter && torrentIdToCategory(torrent.contentCategory) === 'xxx')
			{
				logT('check', 'ignore torrent', torrent.name, 'because adult filter')
				return false
			}

			if(config.filters.sizeEnabled && (torrent.size < config.filters.size.min || torrent.size > config.filters.size.max))
			{
				logT('check', 'ignore torrent', torrent.name, 'because size bounds of', torrent.size, ':', config.filters.size)
				return false
			}

			if(config.filters.contentType && Array.isArray(config.filters.contentType) && !config.filters.contentType.includes(torrent.contentType))
			{
				logT('check', 'ignore torrent', torrent.name, 'because type', torrent.contentType, 'not in:', config.filters.contentType)
				return false
			}

			return true
		}

		const setupTorrentRecord = (torrent) => {
			// fix cases for low cases letters
			if(typeof torrent.contentcategory != 'undefined')
			{
				torrent.contentCategory = torrent.contentcategory;
				delete torrent.contentcategory;
			}
			if(typeof torrent.contenttype != 'undefined')
			{
				torrent.contentType = torrent.contenttype;
				delete torrent.contenttype;
			}

			torrent.contentType = torrentTypeId(torrent.contentType);
			torrent.contentCategory = torrentCategoryId(torrent.contentCategory);

			if(torrent.info && typeof torrent.info == 'string')
			{
				try {
					torrent.info = JSON.parse(torrent.info)
				} catch(err) {
					logT('add', 'problem with info torrent parse for torrent', torrent.name, 'just ignore')
					delete torrent.info
				}
			}

			// clean download info if added
			if(torrent.download)
				delete torrent.download

			// feed date clean
			if(typeof torrent.feedDate !== 'undefined')
				delete torrent.feedDate

			return torrent
		}

		const buildTorrentIndex = (torrent) => {
			let index = torrent.name
			if(torrent.info && typeof torrent.info.name === 'string' && torrent.info.name.length > 0)
			{
				if(torrent.info.name.length < 800)
					index += ' ' + torrent.info.name
			}
			return index
		}

		const insertTorrentToDB = (torrent, silent) => new Promise((resolve) => {
			if(!torrent)
			{
				resolve()
				return
			}

			// duplicate object because we will do object modification
			torrent = Object.assign({}, torrent)

			// setup torrent record if it from db
			setupTorrentRecord(torrent)

			if(!checkTorrent(torrent))
			{
				resolve()
				return
			}

			const { filesList } = torrent
			delete torrent.filesList;

			if(!filesList || filesList.length == 0)
			{
				logT('add', 'skip torrent', torrent.name, '- no filesList')
				resolve()
				return
			}

			torrent.id = torrentsId++;

			const recheckFiles = (callback) => {
				sphinxSingle.query('SELECT id FROM files WHERE hash = ? limit 1', [torrent.hash], function(err, filesRecords) {
					if(err) {
						logTE('add', 'cannot check files in recheckFiles')
						return
					}
    
					if(!filesRecords || filesRecords.length == 0)
					{
						callback()
					}
				})
			}

			const addFilesToDatabase = () => {
				sphinxSingle.query('DELETE FROM files WHERE hash = ?', torrent.hash, function (err, result) {
					if(err)
					{
						return;
					}

					let path = '';
					let size = '';
					for(const file of filesList)
					{
						path += file.path + '\n';
						size += file.size + '\n';
					}
					path = path.slice(0, -1);
					size = size.slice(0, -1);

					sphinxSingle.insertValues('files', { 
						id: torrent.id,
						hash: torrent.hash,
						path,
						size
					}, function(err, result) {
						if(!result) {
							console.error(err);
							return
						}
						if(!silent)
							send('filesReady', torrent.hash);
					});
				})
			}

			sphinxSingle.query("SELECT id FROM torrents WHERE hash = ?", torrent.hash, (err, single) => {
				if(!single)
				{
					logTE('add', err)
					resolve()
					return
				}

				// torrent already probably in db
				if(single.length > 0)
				{
					if(config.recheckFilesOnAdding)
					{
						// recheck files and if they not ok add their to database
						recheckFiles(addFilesToDatabase)
					}
					resolve()
					return
				}
				else
				{
					addFilesToDatabase()
				}

				delete torrent.nameindex;
				torrent.nameIndex = buildTorrentIndex(torrent)

				sphinxSingle.insertValues('torrents', torrent, function(err, result) {
					if(result) {
						if(!silent)
							send('newTorrent', {
								hash: torrent.hash,
								name: torrent.name,
								size: torrent.size,
								files: torrent.files,
								piecelength: torrent.piecelength,
								contentType: torrentIdToType(torrent.contentType),
								contentCategory: torrentIdToCategory(torrent.contentCategory),
								info: torrent.info,
							});
						updateTorrentTrackers(torrent.hash);
						remoteTrackers.update(torrent)
						p2p.info.torrents++;
						p2p.info.files += Number(torrent.files);
						p2p.info.filesSize += Number(torrent.size);
					}
					else
					{
						logTE('add', err);
					}
					resolve()
					events.emit('insert', torrent)
				});
			})
		})

		const removeTorrentFromDB = async (torrent) => {
			const {hash} = torrent
			await sphinxSingle.query('DELETE FROM torrents WHERE hash = ?', hash)
			await sphinxSingle.query('DELETE FROM files WHERE hash = ?', hash)
			logT('remove', 'removed torrent', torrent.name || torrent.hash)
		}

		const updateTorrentToDB = async (torrent) => {
			if(typeof torrent !== 'object')
				return

			// duplicate object because we will do object modification
			torrent = Object.assign({}, torrent)

			// setup torrent record if it from db
			setupTorrentRecord(torrent)

			delete torrent.id
			delete torrent.filesList

			await sphinxSingle.updateValues('torrents', torrent, {hash: torrent.hash})
			logT('update', 'updated torrent', torrent.name)
		}

		const insertMetadata = (metadata, infohash, rinfo) => {
			const bufferToString = (buffer) => Buffer.isBuffer(buffer) ? buffer.toString() : buffer

			logT('spider', 'finded torrent', bufferToString(metadata.info.name), 'and add to database');

			const hash = infohash.toString('hex');
			let size = metadata.info.length ? metadata.info.length : 0;
			let filesCount = 1;
			let filesArray = [];

			const filesAdd = (path, size) => filesArray.push({
				hash,
				path,
				size,
			})

			if(metadata.info.files && metadata.info.files.length > 0)
			{
				filesCount = metadata.info.files.length;
				size = 0;

				for(let i = 0; i < metadata.info.files.length; i++)
				{
					let file = metadata.info.files[i];
					let filePath = bufferToString(file['path.utf-8'] || file.path).join('/');
					filesAdd(filePath, file.length);
					size += file.length;
				}
			}
			else
			{
				filesAdd(bufferToString(metadata.info['name.utf-8'] || metadata.info.name), size)
			}

			const torrentQ = {
				hash: hash,
				name: bufferToString(metadata.info['name.utf-8'] || metadata.info.name),
				size: size,
				files: filesCount,
				piecelength: metadata.info['piece length'],
				ipv4: rinfo.address,
				port: rinfo.port,
				added: Math.floor(Date.now() / 1000)
			};

			torrentTypeDetect(torrentQ, filesArray);
			torrentQ.filesList = filesArray;
			insertTorrentToDB(torrentQ)
		}

		client.on('complete', function (metadata, infohash, rinfo) {

			cleanupTorrents(1); // clean old torrents before writing new

			if(config.spaceQuota && config.spaceDiskLimit > 0)
			{
				disk.check(rootPath, function(err, info) {
					if (err) {
						logTE('quota', err);
					} else {
						const {available, free, total} = info;

						if(free >= config.spaceDiskLimit)
						{
							hideFakeTorrents(); // also enable fake torrents;
							insertMetadata(metadata, infohash, rinfo);
						}
						else
						{
							quotaDebug('ignore torrent', metadata.info.name, 'free space', (free / (1024 * 1024)) + "mb");
							showFakeTorrents(); // also enable fake torrents;
						}
					}
				});
			}
			else
			{
				insertMetadata(metadata, infohash, rinfo);
			}
		});

        
		let downloadersCallbacks = {}
		events.on('insert', (torrent) => {
			const { hash } = torrent
			const callback = downloadersCallbacks[hash]
			if(!callback)
				return

			delete downloadersCallbacks[hash]
			callback(torrent)
		})

		torrentClient._downloader = (peer, infoHash, callback) => {
			const hash = infoHash.toString('hex')
			downloadersCallbacks[hash] = callback
			setTimeout(() => delete downloadersCallbacks[hash], 8000)
			client._download(peer, infoHash)
		}

		recive('dropTorrents', ({pathTorrents}) => {
			logT('drop', 'drop torrents and replicate from original torrent files')
			const addTorrents = (torrents) => {
				torrents.map(({data, path}) => { 
					try {
						return ({ 
							torrent: parseTorrent(data), 
							path 
						})
					} catch(err) {
						logT('drop', 'error on parse torrent:', path)
					}
				})
				.filter(torrent => torrent)
				.forEach(({torrent, path}) => {
					insertMetadata(torrent, torrent.infoHashBuffer, {address: '127.0.0.1', port: 666})
					logT('drop', 'copied torrent to db:', path)
				})
				logT('drop', 'torrent finish adding to db')
			}

			if(pathTorrents[0].data) {
				addTorrents(pathTorrents.map(({data}) => ({data, path: '.'})));
			} else {
				const torrents = _.flatten(pathTorrents.map(({path}) => directoryFilesRecursive(path)))
				.filter(path => mime.getType(path) == 'application/x-bittorrent')
				.map(path => ({data: fs.readFileSync(path), path}));
				addTorrents(torrents);
			}
		})

		checkInternet((connected) => {
			if(!connected)
				return

			const { STUN_BINDING_REQUEST, STUN_ATTR_XOR_MAPPED_ADDRESS } = stun.constants
			const stunServer = stun.createServer()
			const stunRequest = stun.createMessage(STUN_BINDING_REQUEST)
			stunServer.once('bindingResponse', stunMsg => {
				const {address, port} = stunMsg.getAttribute(STUN_ATTR_XOR_MAPPED_ADDRESS).value
				stunServer.close()
    
				logT('stun', 'p2p stun ignore my address', address)
				p2p.ignore(address)
			})
			stunServer.send(stunRequest, 19302, 'stun.l.google.com')    
		})

		let upnp
		if(config.upnp)
		{
			upnp = natUpnp.createClient();
			upnp.ratsMap = () => {
				upnp.portMapping({
					public: config.spiderPort,
					private: config.spiderPort,
					protocol: 'UDP',
					description: 'Rats',
					ttl: 0
				}, function(err) {
					if(err)
						logT('upnp', 'upnp server dont respond')
				});
				upnp.portMapping({
					public: config.spiderPort,
					private: config.spiderPort,
					protocol: 'TCP',
					description: 'Rats',
					ttl: 0
				}, function(err) {
					if(err)
						logT('upnp', 'upnp server dont respond')
				});
				upnp.portMapping({
					public: config.udpTrackersPort,
					private: config.udpTrackersPort,
					protocol: 'UDP',
					description: 'Rats',
					ttl: 0
				}, function(err) {
					if(err)
						logT('upnp', 'upnp server dont respond')
				});
			}

			upnp.ratsUnmap = () => {
				upnp.portUnmapping({
					public: config.spiderPort,
					protocol: 'UDP'
				});
				upnp.portUnmapping({
					public: config.spiderPort,
					protocol: 'TCP'
				});
				upnp.portUnmapping({
					public: config.udpTrackersPort,
					protocol: 'UDP'
				});
			}

			upnp.ratsMap();

			upnp.externalIp(function(err, ip) {
				if(err)
					return

				logT('upnp', 'p2p upnp ignore my address', ip)
				p2p.ignore(ip)
			});
		}

		// feed
		const feed = new Feed({sphinx})
		// load inside api

		// setup api
		await API({
			sphinx,
			sphinxSingle: sphinxSingleAlternative,
			recive,
			send,
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
		})

		if(config.indexer) {
			spider.listen(config.spiderPort)
			if(config.p2p)
			{
				spider.announceHashes = [crypto.createHash('sha1').update('degrats-v1').digest()]
			}
		}

		if(config.cleanup && config.indexer)
		{
			cleanupDebug('cleanup enabled');
			cleanupDebug('cleanup disc limit', (config.cleanupDiscLimit / (1024 * 1024)) + 'mb');
		}

		if(config.spaceQuota)
		{
			quotaDebug('disk quota enabled');
		}

		// load torrents sessions
		logT('downloader', 'restore downloading sessions')
		torrentClient.loadSession(dataDirectory + '/downloads.json')

		this.stop = async (callback) => {
			this.closing = true
			logT('close', 'spider closing...')
			if(upnp)
				upnp.ratsUnmap()

			logT('close', 'closing alternative db interface')
			await sphinxSingleAlternative.end()

			// save torrents sessions
			logT('close', 'save torrents downloads sessions')
			torrentClient.saveSession(dataDirectory + '/downloads.json')

			// save feed
			await feed.save()
    
			// close trackers if needed
			logT('close', 'closing trackers')
			await remoteTrackers.close()

			// stop bootstrap interval
			if(config.p2pBootstrap && p2pBootstrapLoop)
			{
				clearInterval(p2pBootstrapLoop)
				logT('close', 'bootstrap loop stoped')
			}

			// safe future peers
			if(dataDirectory)
			{
				const addresses = p2p.addresses(p2p.peersList())
				const peersEncripted = encryptor.encrypt(addresses)
				if(addresses.length > 0)
				{
					fs.writeFileSync(dataDirectory + '/peers.p2p', peersEncripted, 'utf8');
					logT('close', 'local peers saved')
				}

				if(config.p2pBootstrap)
				{
					const saveBootstrapPeers = (host, path) => new Promise(async (resolve) => {
						if(env === 'test')
						{
							resolve()
							return
						}

						if(addresses.length <= 0)
						{
							resolve()
							return
						}

						// check bootstrap map
						const json = await getServiceJson(`https://${host}${path}`)
						let bootstrapMap = {}
						if(json.bootstrapMap)
						{
							const bootstrapMapCandidate = encryptor.decrypt(json.bootstrapMap)
							if(typeof bootstrapMapCandidate === 'object')
								bootstrapMap = bootstrapMapCandidate
						}
						bootstrapMap[addresses.length] = addresses
            
						const options = {
							port: 443,
							host,
							method: 'PUT',
							path,
							headers: { 
								'Content-Type' : "application/json",
							}
						};
						let timeout;
						const req = http.request(options, () => {
							logT('close', 'bootstrap peers saved to', host)
							clearTimeout(timeout)
							resolve()
						});
						req.on('error', () => {
							logTE('close', 'cant save bootstrap pears to', host)
							clearTimeout(timeout)
							resolve()
						})
						req.end(JSON.stringify({
							bootstrap: peersEncripted,
							bootstrapMap: encryptor.encrypt(bootstrapMap)
						}))
						timeout = setTimeout(() => {
							logTE('close', 'abort by time', host)
							req.destroy();
						}, 4000)
					})

					// await Promise.all([
					// 	saveBootstrapPeers('api.myjson.com', '/bins/1e5rmh'),
					// 	saveBootstrapPeers('jsonblob.com', '/api/jsonBlob/4d22c8ba-5046-11eb-b13f-81fd0496c154'),
					// 	saveBootstrapPeers('getpantry.cloud', '/apiv1/pantry/2f760eeb-2e76-4ea0-804b-6032223086e1/basket/testBasket')
					// ])
				}
			}

			logT('close', 'closing p2p...')
			await p2p.close()

			// don't listen complete torrent responses
			client.removeAllListeners('complete')

			logT('close', 'closing torrent client')
			torrentClient.destroy(() => spider.close(async () => {
				await sphinx.end()
				logT('close', 'pool closed')
				await sphinxSingle.end()
				logT('close', 'single closed')
				logT('close', 'spider closed')
				callback()
			}))
		}
    
	})()
}
