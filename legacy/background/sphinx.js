const path = require("path");
let env
try{
	env = require("env");
} catch(e){}
const appPath = require('./electronAppPath')
const fs = require('fs')
const iconv = require('iconv-lite')
const { spawn, exec } = require('child_process')
const appConfig = require('./config')
const findFiles = require('./findFiles')
const _ = require('lodash')
const isRunning = require('is-running')
const portCheck = require('./portCheck')
const detectOnebyteEncoding = require('detect-onebyte-encoding')
const isOneByteEncoding = require('./detectOneByte')
const {promisify} = require('util');
const mkdirp = require('mkdirp')
const mysql = require('./mysql')
const asyncWait = require('./asyncWait')

const findGoodPort = async (port, host) => {
	while (!(await portCheck(port, host))) {
		port++
		logT('sphinx', 'port is busy, listen on', port)
	}
	return port
}

const writeSphinxConfig = async (rootPath, dbPath, params = {}) => {
	appConfig.sphinx.port = await findGoodPort(appConfig.sphinx.port)
	appConfig.sphinx = appConfig.sphinx

	let generateConfig = () => (`
  index torrents
  {
    type = rt
    path = ${dbPath}/database/torrents
    
    min_prefix_len = 3
    expand_keywords = 1
    
    rt_attr_string = hash
    rt_attr_string = name
    rt_field = nameIndex
    rt_attr_bigint = size
    rt_attr_uint = files
    rt_attr_uint = piecelength
    rt_attr_timestamp = added
    rt_field = ipv4
    rt_attr_uint = port
    rt_attr_uint = contentType
    rt_attr_uint = contentCategory
    rt_attr_uint = seeders
    rt_attr_uint = leechers
    rt_attr_uint = completed
    rt_attr_timestamp = trackersChecked
    rt_attr_uint = good
    rt_attr_uint = bad
    rt_attr_json = info
    
	stored_only_fields = ipv4
  }

  index files
  {
    type = rt
    path = ${dbPath}/database/files
      
    rt_field = path
    rt_attr_string = hash
    rt_field = size

    stored_fields = path
	stored_only_fields = size
  }

  index version
  {
      type = rt
      path = ${dbPath}/database/version
      
      rt_attr_uint = version
      rt_field = versionIndex
  }

  index store
  {
      type = rt
      path = ${dbPath}/database/store
      
      rt_field = storeIndex
      rt_attr_json = data
      rt_attr_string = hash
      rt_attr_string = peerId
  }

  index feed
  {
      type = rt
      path = ${dbPath}/database/feed

      rt_field = feedIndex
      rt_attr_json = data
  }

  searchd
  {
    listen      = 127.0.0.1:${appConfig.sphinx.port}:mysql41
    seamless_rotate   = 1
    preopen_indexes   = 1
    unlink_old    = 1
    pid_file    = ${rootPath}/searchd.pid
    log     = ${rootPath}/searchd.log
    query_log   = ${rootPath}/query.log
    binlog_path = ${rootPath}
  }
  `);

  	let config = generateConfig()

	// fix db path under windows platform (one-byte path)
	let windowsEncodingFix = false;
	if(/^win/.test(process.platform) && (!isOneByteEncoding(dbPath) || !isOneByteEncoding(rootPath)))
	{
		logT('sphinx', 'detected non-one byte encoding, trying to fix config for db path', dbPath, params.noWindowsReEncoding)
		let encoding = detectOnebyteEncoding(rootPath + dbPath)
		if (encoding !== 'utf8' && !params.noWindowsReEncoding) {
			config = iconv.encode(config, encoding)
			logT('sphinx', 'config encoded to', encoding)
			windowsEncodingFix = true
		} else {
			logT('sphinx', 'config encoded with utf8, moving config to some root directory')
			while(!isOneByteEncoding(dbPath) || !(fs.statSync(dbPath).mode & 0x92))
				dbPath = path.dirname(dbPath)
			while(!isOneByteEncoding(rootPath) || !(fs.statSync(rootPath).mode & 0x92))
				rootPath = path.dirname(rootPath)
			dbPath += "/RatsConfig"
			rootPath += "/RatsConfig"
			await mkdirp(dbPath)
			await mkdirp(rootPath)
			logT('sphinx', 'changed root directory', rootPath)
			logT('sphinx', 'changed db directory', dbPath)
			config = generateConfig()
		}
	}

	// clear dir in test env
	if(env && env.name === 'test')
	{
		if (fs.existsSync(`${dbPath}/database`)) {
			fs.readdirSync(`${dbPath}/database`).forEach(function(file, index){
				const curPath = `${dbPath}/database` + "/" + file;
				if (!fs.lstatSync(curPath).isDirectory()) {
					fs.unlinkSync(curPath);
				}
			});

			fs.readdirSync(rootPath).forEach(function(file, index){
				if(!file.startsWith('binlog'))
					return;
				const curPath = rootPath + "/" + file;
				if (!fs.lstatSync(curPath).isDirectory()) {
					fs.unlinkSync(curPath);
				}
			});
		}
	}

	// clean query.log because it too large and don't consist any good info
	if(fs.existsSync(`${rootPath}/query.log`))
	{
		fs.unlinkSync(`${rootPath}/query.log`)
	}

	let isInitDb = false

	if (!fs.existsSync(`${dbPath}/database`)){
		fs.mkdirSync(`${dbPath}/database`);
		isInitDb = true
	}

	fs.writeFileSync(`${rootPath}/sphinx.conf`, config)
	logT('sphinx', `writed sphinx config to ${rootPath}`)
	logT('sphinx', 'db path:', dbPath)

	return {isInitDb, rootPath, dbPath, windowsEncodingFix}
}

module.exports = async (callback, dataDirectory, onClose, params = {}) => {
	const start = async (callback) => {

		const sphinxPath = path.resolve(appPath('searchd'))
		logT('sphinx', 'Sphinx Path:', sphinxPath)

		let sphinxConfigDirectory = dataDirectory
		appConfig['dbPath'] = appConfig.dbPath && appConfig.dbPath.length > 0 ? appConfig.dbPath : sphinxConfigDirectory;
		// on portable dir can move database directory
		if(!fs.existsSync(appConfig.dbPath) && fs.existsSync(sphinxConfigDirectory))
		{
			appConfig['dbPath'] = sphinxConfigDirectory
		}

		// check external sphinx instance for using
		const sphinxPid = `${sphinxConfigDirectory}/searchd.pid`
		const isSphinxExternal = fs.existsSync(sphinxPid) && isRunning(parseInt(fs.readFileSync(sphinxPid)))
		if(isSphinxExternal)
			logT('sphinx', `founded running sphinx instance in ${sphinxPid}, using it`)

		const { isInitDb, rootPath, dbPath, windowsEncodingFix } = isSphinxExternal ? {isInitDb: false} : await writeSphinxConfig(sphinxConfigDirectory, appConfig.dbPath, params)
		// on windows directory can be changed during sphinx bug with one byte path
		if (rootPath != sphinxConfigDirectory || dbPath != appConfig.dbPath)
		{
			sphinxConfigDirectory = rootPath;
			appConfig.dbPath = dbPath;
		}

		const config = `${sphinxConfigDirectory}/sphinx.conf`
		const options = ['--config', config]
		if(!(/^win/.test(process.platform)))
		{
			options.push('--nodetach')
		}

		const sphinx = !isSphinxExternal ? spawn(sphinxPath, options) :
			{isExternal: true, on: (d,f) => {}, stdout: {on : (d,f)=>{} }}; // running stub

		// remeber initizalizing of db
		sphinx.start = start
		sphinx.isInitDb = isInitDb
		sphinx.directoryPath = appConfig.dbPath
		sphinx.directoryPathDb = appConfig.dbPath + '/database'

		const optimizeResolvers = {}

		let needConvertation = false;
		sphinx.stdout.on('data', (data) => {
			logT('sphinx', `sphinx: ${data}`)

			// don't listen if we are in fixing mode
			if(sphinx.fixing)
				return

			if (data.includes('accepting connections')) {
				logT('sphinx', 'catched sphinx start');
				// convertation for linux after start
				if(needConvertation) {
					sphinx.convertDatabase();
					return;
				}
				if(callback)
					callback()
			}

			if(data.includes('invalid meta file'))
			{
				sphinx.fixDatabase()
			}

			if(data.includes('indexes with meta prior to v.14 are no longer supported'))
			{
				needConvertation = true;
			}

			let manticoreVersion;
			if (!sphinx.version && (manticoreVersion = /Manticore ([0-9\.]+)/.exec(data))) {
				sphinx.version = manticoreVersion[1];
				logT('sphinx', 'sphinx version', sphinx.version);
			}
	
			if(windowsEncodingFix && data.includes('failed to parse config file'))
			{
				logT('sphinx', 'encoding rewrite failed, forcing restart of application to fix that problem')
				sphinx.windowsEncodingFix = true;
			}

			const checkOptimized = String(data).match(/index ([\w]+): optimized/)
			if(checkOptimized)
			{
				if(optimizeResolvers[checkOptimized[1]])
				{
					logT('sphinx', 'resolve optimizer', checkOptimized[1])
					optimizeResolvers[checkOptimized[1]]()
				}
			}
		})

		const close = () => {
			if(onClose && !sphinx.replaceOnClose) // sometime we don't want to call default callback
				onClose()
			if(sphinx.onClose)
				sphinx.onClose()
		}

		sphinx.on('close', (code, signal) => {
			logT('sphinx', `sphinx closed with code ${code} and signal ${signal}`)
			close()
		})

		sphinx.stop = (onFinish, replaceFinish) => {
			logT('sphinx', 'sphinx closing...')
			if(onFinish)
				sphinx.onClose = onFinish
			if(replaceFinish)
				sphinx.replaceOnClose = true // sometime we don't want to call default callback
            
			if (!sphinx.isExternal) 
			{
				logT('sphinx', `stoping with sphinx stopwait`);
				exec(`"${sphinxPath}" --config "${config}" --stopwait`)
			} 
			else
			{
				logT('sphinx', `ignoring sphinx closing because external sphinx instance`)
				close()
			}
		}

		sphinx.waitOptimized = (table) => new Promise((resolve) => {
			optimizeResolvers[table] = () => {
				delete optimizeResolvers[table];
				resolve()
			}
		})

		sphinx.convertDatabase = async () => {
			logT('sphinx', 'found old database, starting convertiong process...');
			if(sphinx.isExternal)
				return

			if(sphinx.fixing)
				return
			sphinx.fixing = true

			logT('sphinx', 'run database convertation...');

			// close db
			await new Promise((resolve) => {
				sphinx.stop(resolve, true)
				logT('sphinx', 'revent start')
			})

			logT('sphinx', 'sphinx stoped')

			const converterPath = path.resolve(appPath('index_converter'))
			logT('dbconverter', 'Convert Path:', converterPath)

			const binLogs = await findFiles(`${rootPath}/binlog.*`)
			logT('dbconverter', 'remove ', binLogs)
			if(binLogs)
				binLogs.forEach(file => fs.unlinkSync(file));

			logT('dbconverter', 'fixing ramchunks')
			await new Promise((resolve) => {
				const oldSphinxPath = path.resolve(appPath('searchd.v2'))
				logT('dbconverter', 'old sphinx', oldSphinxPath);
				let options = ['--config', config];
				if(!(/^win/.test(process.platform)))
				{
					options.push('--nodetach')
				}
				const oldSphinxEXE = spawn(oldSphinxPath, options);

				const tables = [];
				oldSphinxEXE.stdout.on('data', async (data) => {
					data = data.toString();
					logT('sphinx', data);

					const table = /precaching index '(\w+)'/.exec(data);
					if(table)
						tables.push(table[1]);

					if (data.includes('accepting connections')) {
						logT('sphinx', 'catched sphinx start')
						const mydb = mysql.single();
						for(const table of tables)
							await mydb.query(`FLUSH RAMCHUNK ${table}`);
						await mydb.end()
						exec(`"${oldSphinxPath}" --config "${config}" --stopwait`)
					}

					if(data.includes('shutdown complete')) {
						await asyncWait(200);
						resolve();
					}
				});
			});

			logT('dbconverter', 'fixing ramchunks ok, start converting...')

			await new Promise((resolve) => {
				const converterEXE = spawn(converterPath, ['--config', config, '--all']);
				converterEXE.stdout.on('data', (data) => {
					data = data.toString();
					logT('dbconverter', data);

					if(data.includes('converted indexes')) {
						resolve();
					}
				});
			})

			logT('dbconverter', 'database conveted');

			// cleanup
			const oldFiles = await findFiles(`${dbPath}/database/*.old.*`)
			logT('dbconverter', 'remove ', oldFiles)
			if(oldFiles)
				oldFiles.forEach(file => fs.unlinkSync(file));

			logT('dbconverter', 'cleanup finish');

			// restart
			sphinx.fixing = false
			_.merge(sphinx, await sphinx.start(callback));
		}

		sphinx.fixDatabase = async () => {
			if(sphinx.isExternal)
				return

			if(sphinx.fixing)
				return
			sphinx.fixing = true

			// close db
			await new Promise((resolve) => {
				sphinx.stop(resolve, true)
				logT('sphinx', 'revent start')
			})

			const checkNullFile = (file) => new Promise((resolve) => {
				let f = fs.createReadStream(file)
				f.on('data', (chunk) => {  
					for(const byte of chunk)
						if(byte != 0)
						{
							resolve(true)
							f.destroy()
							return
						}
				}).on('end', () => {
					resolve(false)
				});
			})

			// check meta files
			const probablyCoruptedFiles = await findFiles(`${sphinx.directoryPath}/**/*.+(meta|ram)`)
			let brokenFiles = await Promise.all(probablyCoruptedFiles.map(file => checkNullFile(file)))
			brokenFiles = probablyCoruptedFiles.filter((file, index) => !brokenFiles[index])
            
			brokenFiles.forEach(file => {
				logT('sphinx', 'FIXDB: clean file because of broken', file)
				fs.unlinkSync(file)
			})

			sphinx.fixing = false

			_.merge(sphinx, await sphinx.start(callback));
		}

		if (isSphinxExternal && callback) setTimeout(()=>{logT('sphinx', 'external sphinx signalled');callback()}, 0);

		return {sphinx, rootPath: sphinxConfigDirectory, dbPath}
	}

	return await start(callback)
}