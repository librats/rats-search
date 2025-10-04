const {single} = require('./mysql')
const forBigTable  = require('./forBigTable')
const { BrowserWindow }  = require("electron");
const url  = require('url')
const path  = require('path')
const fs  = require('fs')
const glob = require("glob")
const asyncForEach = require('./asyncForEach')

const {torrentTypeDetect, torrentTypeId, torrentIdToType, torrentCategoryId, torrentIdToCategory} = require('../app/content');
const startSphinx = require('./sphinx')


const currentVersion = 8


module.exports = async (callback, mainWindow, sphinxApp) => {
	let sphinx = await single().waitConnection()

	const setVersion = async (version) => {
		await sphinx.query(`delete from version where id = 1`)
		await sphinx.query(`insert into version(id, version) values(1, ${version})`)
		if(sphinxApp)
			fs.writeFileSync(`${sphinxApp.directoryPath}/version.vrs`, `${version}`)
	}
    
	let patchWindow;
	const openPatchWindow = (closable = false) => {
		if(patchWindow)
			return

		if(!BrowserWindow)
			return

		if(mainWindow)
			mainWindow.hide()

		patchWindow = new BrowserWindow({width: 800, height: 400, closable})

		patchWindow.setMenu(null)

		patchWindow.on('close', () => mainWindow.appClose())

		patchWindow.loadURL("data:text/html;charset=utf-8," + encodeURI(`
            <html>
                <head><title>Database patching...</title></head>
                <style>
                html, body {
                    height: 100%;
                }

                html {
                    display: table;
                    margin: auto;
                }

                body {
                    background: -webkit-gradient(linear, left top, left bottom, from(#fff), to(#cbccc8)) fixed;
                    display: table-cell;
                    vertical-align: middle;
                    color: #939393;
                    font-size: 1.5em; 
                    text-shadow: 1px 1px 2px #c6c6c6, 0 0 0.2em #e2e2e2;
                    overflow: hidden;
                }
                svg {
                    position: absolute;
                    width: 100%;
                    top: 0;
                    left: 0;
                    z-index: -1;
                    opacity: 0.5;
                    max-height: 120%;
                }
                #one {
                    padding: 20px;
                }
                #long {
                    font-size: 0.8em;
                    padding: 10px;
                }
                #canBreak {
                    font-size: 0.8em;
                    padding: 10px;
                }
                </style>
                <script>
                    const {ipcRenderer} = require('electron')
                    ipcRenderer.on('reindex', (e, data) =>{
                        document.getElementById('one').innerHTML = \`Updating \${data.torrent ? 'torrent': 'file'} \${data.index} of \${data.all} [\${data.field} index]\`
                        if(data.longTime)
                            document.getElementById('long').innerHTML = 'This patch is very long, may be some hours. So you can take some cup of tea, while we perform db patch.'
                        if(data.canBreak)
                            document.getElementById('canBreak').innerHTML = 'You can break this patch, and continue when you will have time to patch, it will be resumed.'
                    })
                    ipcRenderer.on('optimize', (e, data) =>{
                        document.getElementById('one').innerHTML = \`Optimization for \${data.field}...\`
                    })
                </script>
                <body>
                    <svg fill='white' viewBox='0 0 264.725 264.725'><path d="M220.195,71.427c-0.589-7.654-9.135-15.619-17.979-16.209c-8.844-0.584-17.398,0.301-12.087,6.483
                    c5.308,6.188,7.074,12.091,4.423,11.212c-2.66-0.896-13.267-7.08-45.104-2.066c-4.126,1.17-21.221-12.682-44.513-12.977
                    c-23.283-0.295-40.381,6.346-64.85,72.296c-2.356,5.828-18.866,19.386-27.71,25.865C3.536,162.529,0.007,169.787,0,182.763
                    c-0.018,18.158,25.934,27.187,81.648,26.889c55.715-0.292,85.195-9.388,85.195-9.388c-62.789,6.773-158.907,10.52-158.907-18.687
                    c0-20.641,28.321-28.47,36.281-28.184c7.958,0.3,13.562,12.673,33.307,5.603c3.247-0.295,1.48,4.423-1.18,7.369
                    c-2.651,2.942-0.586,6.487,9.73,6.487c10.315,0,41.183,0.295,47.707,0c6.531-0.299,11.839-11.792-9.384-12.68
                    c-18.548,0.311,12.023-5.773,15.915-21.813c0.709-3.927,8.84-4.139,15.918-4.119c20.777,0.029,34.485,38.193,38.912,38.338
                    c4.416,0.15,17.979,1.621,17.683-4.273c-0.292-5.897-11.491-3.241-13.854-6.487c-2.359-3.234-10.023-15.504-7.366-21.104
                    c2.65-5.59,12.674-21.229,24.463-22.988c11.789-1.777,42.451,7.361,47.459,0c5.012-7.372-6.783-11.512-15.918-28.611
                    C243.779,80.572,238.768,71.728,220.195,71.427z"/>
                    <div id="one"></div>
                    <div id="long"></div>
                    <div id="canBreak"></div>
                </svg>
                </body>
            </html>
        `))
	}

	const patch = async (version) => {
		logT('patcher', 'db version', version)

		const rebuildFull = async (table = 'torrents', patchFunction = (torrent) => torrent.nameIndex = torrent.name) => {

			if(sphinxApp.isExternal)
			{
				logTE('patcher', 'this patch avaiable only not on external db')
				throw new Error('this patch avaiable only not on external db')
			}

			let i = 1
			const torrents = (await sphinx.query(`SELECT COUNT(*) AS c FROM ${table}`))[0].c

			let torrentsArray = []

			let patch = 1
			await forBigTable(sphinx, table, async (torrent) => {
				logT('patcher', 'remember index', torrent.id, torrent.name || torrent.hash, '[', i, 'of', torrents, ']')
				if(patchWindow)
					patchWindow.webContents.send('reindex', {field: torrent.name || torrent.hash, index: i++, all: torrents, torrent: true})

				torrentsArray.push(torrent)
				// keep memory safe
				if(torrentsArray.length >= 20000)
				{
					fs.writeFileSync(`${sphinxApp.directoryPath}/${table}.patch.${patch++}`, JSON.stringify(torrentsArray, null, 4), 'utf8');
					logT('patcher', `write ${table} dump`, `${sphinxApp.directoryPath}/${table}.patch.${patch - 1}`)
					torrentsArray = []
				}
			})
			// keep last elemets
			if(torrentsArray.length > 0)
			{
				fs.writeFileSync(`${sphinxApp.directoryPath}/${table}.patch.${patch}`, JSON.stringify(torrentsArray, null, 4), 'utf8');
				logT('patcher', 'write torrents dump', `${sphinxApp.directoryPath}/${table}.patch.${patch}`)
				torrentsArray = []
			}
			else
			{
				patch-- //no last patch
			}

			// stop sphinx
			await new Promise((resolve) => {
				// reopen sphinx
				sphinx.destroy() // destory connection
				sphinxApp.stop(resolve, true)
			})

			logT('patcher', 'sphinx stoped for patching')

			await new Promise((resolve) => {
				glob(`${sphinxApp.directoryPathDb}/${table}.*`, function (er, files) {
					files.forEach(file => {
						logT('patcher', `clear ${table} file`, file)
						fs.unlinkSync(path.resolve(file))
					})
					resolve()
				})
			})

			logT('patcher', `cleaned ${table} db structure, rectreating again`)
			i = 1
			await new Promise(async (resolve) => {
				// reopen sphinx
				const spRun = await sphinxApp.start(async () => {
					sphinx = await single().waitConnection()
					resolve()
				}) // same args
				sphinxApp = spRun.sphinx;
			})

			logT('patcher', 'sphinx restarted, patch db now')

			for(let k = 1; k <= patch; k++)
			{
				torrentsArray = JSON.parse(fs.readFileSync(`${sphinxApp.directoryPath}/${table}.patch.${k}`, 'utf8'))
				logT('patcher', 'read torrents dump', `${sphinxApp.directoryPath}/${table}.patch.${k}`)
				await asyncForEach(torrentsArray, async (torrent) => {
					logT('patcher', 'update index', torrent.id, torrent.name || torrent.hash, '[', i, 'of', torrents, ']')
					if(patchWindow)
						patchWindow.webContents.send('reindex', {field: torrent.name, index: i++, all: torrents, torrent: true})

					patchFunction(torrent);
					//await sphinx.query(`DELETE FROM ${table} WHERE id = ${torrent.id}`)
					await sphinx.insertValues(table, torrent)
				})
			}

			await new Promise((resolve) => {
				glob(`${sphinxApp.directoryPath}/${table}.patch.*`, function (er, files) {
					files.forEach(file => {
						logT('patcher', 'clear dump file', file)
						fs.unlinkSync(path.resolve(file))
					})
					resolve()
				})
			})

			torrentsArray = null

			logT('patcher', `optimizing ${table}`)
			if(patchWindow)
				patchWindow.webContents.send('optimize', {field: table})
			sphinx.query(`OPTIMIZE INDEX ${table}`)
			await sphinxApp.waitOptimized(table)
		}

		switch(version)
		{
		case 1:
		{
			logT('patcher', 'patch db to version 2')
			openPatchWindow()
			let i = 1

			const torrents = (await sphinx.query("SELECT COUNT(*) AS c FROM torrents"))[0].c
			const files = (await sphinx.query("SELECT COUNT(*) AS c FROM files"))[0].c

			await forBigTable(sphinx, 'torrents', async (torrent) => {
				logT('patcher', 'update index', torrent.id, torrent.name, '[', i, 'of', torrents, ']')
				if(patchWindow)
					patchWindow.webContents.send('reindex', {field: torrent.name, index: i++, all: torrents, torrent: true})

				torrent.nameIndex = torrent.name
				await sphinx.query(`DELETE FROM torrents WHERE id = ${torrent.id}`)
				await sphinx.insertValues('torrents', torrent)
			})
			i = 1
			await forBigTable(sphinx, 'files', async (file) => {
				logT('patcher', 'update index', file.id, file.path, '[', i, 'of', files, ']')
				if(patchWindow)
					patchWindow.webContents.send('reindex', {field: file.path, index: i++, all: files})

				file.pathIndex = file.path
				await sphinx.query(`DELETE FROM files WHERE id = ${file.id}`)
				await sphinx.insertValues('files', file)
			})

			await setVersion(2)
		}
		case 2:
		{
			openPatchWindow()

			logT('patcher', 'optimizing torrents')
			if(patchWindow)
				patchWindow.webContents.send('optimize', {field: 'torrents'})
			sphinx.query(`OPTIMIZE INDEX torrents`)
			await sphinxApp.waitOptimized('torrents')

			logT('patcher', 'optimizing files')
			if(patchWindow)
				patchWindow.webContents.send('optimize', {field: 'files'})
			sphinx.query(`OPTIMIZE INDEX files`)
			await sphinxApp.waitOptimized('files')

			await setVersion(3)
		}
		case 3:
		{
			openPatchWindow()

			// block xxx
			let bad = 0

			let i = 1
			const torrents = (await sphinx.query("SELECT COUNT(*) AS c FROM torrents"))[0].c
			await forBigTable(sphinx, 'torrents', async (torrent) => {
				logT('patcher', 'update index', torrent.id, torrent.name, '[', i, 'of', torrents, '] - delete:', bad)
				if(patchWindow)
					patchWindow.webContents.send('reindex', {field: torrent.name, index: i++, all: torrents, torrent: true})

				if(torrent.contentcategory == 'xxx')
				{
					delete torrent.contentcategory
					delete torrent.contenttype
					torrent.filesList = (await sphinx.query(`SELECT * FROM files WHERE hash = '${torrent.hash}'`)) || []
					torrentTypeDetect(torrent, torrent.filesList)
					if(torrent.contentType == 'bad')
					{
						logT('patcher', 'remove bad torrent', torrent.name)
						bad++
						await sphinx.query(`DELETE FROM torrents WHERE hash = '${torrent.hash}'`)
						await sphinx.query(`DELETE FROM files WHERE hash = '${torrent.hash}'`)
					}
				}
			})

			logT('patcher', 'removed', bad, 'torrents')

			await setVersion(4)
		}
		case 4:
		{
			openPatchWindow()
			await rebuildFull()
			await setVersion(5)
		}
		case 5:
		{
			openPatchWindow()
			await rebuildFull()
			await setVersion(6)
		}
		case 6:
		{
			openPatchWindow(true)
			logT('patcher', 'merge all files in db patch');

			let filesMap = {}
			let newId = 0;
			let fileIndex = 0;
			let fileIndexChecked = 0;
			let count = (await sphinx.query("select count(*) as cnt from files where size > 0"))[0].cnt;

			if(patchWindow)
				patchWindow.webContents.send('reindex', {field: 'calculate', index: 'calculate', all: count, longTime: true, canBreak: true})

			// found new id
			try {
				const maxNotPatched = (await sphinx.query("select min(id) as cnt from files where size > 0"))[0].cnt;
				newId = (await sphinx.query(`select max(id) as cnt from files where id < ${maxNotPatched}`))[0].cnt | 0;
				if(newId <= 0) {
					logTE('patcher', 'not founded old if');
					newId = 0;
				}
			} catch(e) {
				newId = 0;
			}
			newId++;
			logT('patcher', 'founded newId', newId);
            
			logT('patcher', 'perform optimization');
			sphinx.query(`OPTIMIZE INDEX files`)
			await sphinxApp.waitOptimized('files')

			const descFiles = await sphinx.query(`desc files`);
			let isSizeNewExists = false;
			let isSizeAlreadyPatched = false;
			descFiles.forEach(({Field, Type}) => {
				if(Field == 'size_new')
					isSizeNewExists = true;
				if(Field == 'size' && Type == 'string')
					isSizeAlreadyPatched = true;
			});
            
			if(!isSizeNewExists)
				await sphinx.query("alter table files add column `size_new` string");
			else
				logT('patcher', 'size_new already exists, skip');

			const fileMapWorker = async (keys) => {
				let hashCount = 0;
				for(let hash of keys)
				{
					if(filesMap[hash].length == 0)
						continue;

					fileIndex++;
					for(let i = 1; i < filesMap[hash].length; i++)
					{
						fileIndex++;
						filesMap[hash][0].path += '\n' + filesMap[hash][i].path;
						filesMap[hash][0].size += '\n' + filesMap[hash][i].size;
					}

					await sphinx.query(`DELETE FROM files WHERE hash = '${hash}'`);
					await sphinx.insertValues('files', { 
						id: newId++,
						hash,
						path: filesMap[hash][0].path,
						pathIndex: filesMap[hash][0].path,
						size_new: filesMap[hash][0].size.toString()
					});
					logT('patcher', 'patched file', fileIndex, 'from', count, 'hash', hash, 'cIndex', ++hashCount);
					if(patchWindow)
						patchWindow.webContents.send('reindex', {field: hash, index: fileIndex, all: count, longTime: true, canBreak: true})

					delete filesMap[hash];
				}
			}

			if(!isSizeAlreadyPatched)
			{
				await forBigTable(sphinx, 'files', (file) => {
					if(!filesMap[file.hash])
					{
						filesMap[file.hash] = []
					}
					filesMap[file.hash].push(file);
				}, null, 1000, 'and size > 0', async (lastTorrent) => {
					if(fileIndex > 0 && fileIndex - fileIndexChecked > 500000) {
						fileIndexChecked = fileIndex;
						logT('patcher', 'perform optimization');
						sphinx.query(`OPTIMIZE INDEX files`)
						await sphinxApp.waitOptimized('files')
					}

					let keys = Object.keys(filesMap);
					if(keys.length > 2000) {
						await fileMapWorker(keys.filter(key => key !== lastTorrent.hash));
					}
				})
				let keys = Object.keys(filesMap);
				if(keys.length > 0)
					await fileMapWorker(keys);
				filesMap = null;
			}

			await sphinx.query("alter table files drop column `size`");
			await sphinx.query("alter table files add column `size` string");
			fileIndex = 1;
			count = (await sphinx.query("select count(*) as cnt from files where size is null"))[0].cnt;
			logT('patcher', 'restore files', count);
			await forBigTable(sphinx, 'files', async (file) => {
				if(!file.size_new)
					return
				file.size = file.size_new.toString();
				delete file.size_new;
				await sphinx.replaceValues('files', file, {particial: false, sphinxIndex: {pathIndex: 'path'}});
				if(patchWindow)
					patchWindow.webContents.send('reindex', {field: file.id, index: fileIndex, all: count, longTime: false, canBreak: true})
				logT('patcher', 'restore patched file', fileIndex++, 'from', count, 'hash', file.hash);
			}, null, 1000, 'and size is null');
			await sphinx.query("alter table files drop column `size_new`");

			await setVersion(7)

			sphinx.query(`OPTIMIZE INDEX files`)
			await sphinxApp.waitOptimized('files')
		}
		case 7:
		{
			openPatchWindow()
			await rebuildFull('torrents', (torrent) => {
				torrent.nameIndex = torrent.name
				torrent.contenttype = torrentTypeId(torrent.contenttype);
				torrent.contentcategory = torrentCategoryId(torrent.contentcategory);
			});
			await rebuildFull('files', (file) => {});
			await setVersion(8)
		}
		}
		logT('patcher', 'db patch done')
		sphinx.destroy()
		if(patchWindow)
		{
			patchWindow.destroy()
			if(mainWindow)
				mainWindow.show()
		}
		callback()
	}

	// init of db, we can set version to last
	if(sphinxApp && sphinxApp.isInitDb)
	{
		logT('patcher', 'new db, set version to last version', currentVersion)
		await setVersion(currentVersion)
	}

	sphinx.query('select * from version', async (err, version) => {
		if(err)
		{
			logTE('patcher', 'error on version get on db patch')
			return
		}

		if(!version || !version[0] || !version[0].version)
		{
			if(sphinxApp && fs.existsSync(`${sphinxApp.directoryPath}/version.vrs`))
			{
				const ver = parseInt(fs.readFileSync(`${sphinxApp.directoryPath}/version.vrs`))
				if(ver > 0)
				{
					logT('patcher', 'readed version from version.vrs', ver)
					patch(ver)
				}
				else
				{
					logT('patcher', 'error: bad version in version.vrs')
				}
			}
			else
			{
				logT('patcher', 'version not founded, set db version to 1')
				await setVersion(1)
				patch(1)
			}
		}
		else
		{
			patch(version[0].version)
		}
	})
}