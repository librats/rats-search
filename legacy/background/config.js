const { app } = require('electron')
const os = require('os')
const _ = require('lodash')

let config = {
	indexer: true,

	domain: 'ratsontheboat.org',
	httpPort: 8095,
	spiderPort: 4445,
	udpTrackersPort: 4446,
	udpTrackersTimeout: 3 * 60 * 1000,
	peerId: undefined, 
	language: 'en',
	trackers: true,
	restApi: false,
    
	p2p: true,
	p2pConnections: 10,
	p2pBootstrap: true,
	p2pReplication: true,
	p2pReplicationServer: true,

	upnp: true,

	trayOnClose: false,
	trayOnMinimize: true,
	startMinimized: false,
	darkMode: false,

	sitemapMaxSize: 25000,

	sphinx: {
		host     : '127.0.0.1',
		port     : 9306,
		connectionLimit: 10
	},

	spider: {
		walkInterval: 5,
		nodesUsage: 100,
		packagesLimit: 500
	},

	downloader: {
		maxConnections: 200,
		timeout: 5000
	},

	filters: {
		maxFiles: 0,
		namingRegExp: '',
		namingRegExpNegative: false,
		adultFilter: false,

		size: {max: 0, min: 0},
		maxSize: 1024 * 1024 * 1024 * 1024,
		sizeEnabled: false,

		contentType: null 
	},

	cleanup: true,
	cleanupDiscLimit: 7 * 1024 * 1024 * 1024,
	spaceQuota: false,
	spaceDiskLimit: 7 * 1024 * 1024 * 1024,
	recheckFilesOnAdding: true,

	dbPath: '',

	client: {
		downloadPath: os.homedir() + '/Downloads'
	},

	feedDate: 0,
}

const fs = require('fs');
const debug = require('debug')('config')

let configPath = 'rats.json'
if(app && app.getPath("userData") && app.getPath("userData").length > 0)
{
	configPath = app.getPath("userData") + '/rats.json'
}

const configProxy = new Proxy(config, {
	set: (target, prop, value, receiver) => {
		// some values check (important!)
		if(prop == 'p2pConnections' && value < 10)
			value = 10
		if(prop == 'p2pConnections' && value > 300)
			value = 300
		if(prop == 'p2pReplication' && value)
			target['p2pReplicationServer'] = true
		if(prop == 'p2pReplicationServer' && !value)
			target['p2pReplication'] = false

		if (!_.isEqual(target[prop],value)) {
			logT('config', prop, '=', value)
		}
		target[prop] = value
        
		if(!fs.existsSync(configPath))
			fs.writeFileSync(configPath, '{}')

		const data = fs.readFileSync(configPath)
		let obj = JSON.parse(data)
		obj[prop] = value;
		fs.writeFileSync(configPath, JSON.stringify(obj, null, 4), 'utf8');
		debug('saving rats.json:', prop, '=', value)
		return true
	}
})

config.load = () => {
	debug('loading configuration', configPath)
	if(fs.existsSync(configPath))
	{
		debug('finded configuration', configPath)
		const data = fs.readFileSync(configPath, 'utf8')
		const obj = JSON.parse(data);
		for(let prop in obj) 
		{
			// объединяем объекты
			if(typeof config[prop] === 'object' && typeof obj[prop] === 'object')
			{
				for(const subProp in obj[prop])
				{
					config[prop][subProp] = obj[prop][subProp]
				}
			}
			else
			{
				config[prop] = obj[prop]
			}
			debug('rats.json:', prop, '=', obj[prop])
		}
	}
	return configProxy
}

config.reload = (path) => {
	configPath = path  + '/rats.json'
	return config.load()
}

module.exports = configProxy.load()