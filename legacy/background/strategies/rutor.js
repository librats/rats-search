
const fetch = require('node-fetch')
const cheerio = require('cheerio')
const {promisify} = require('util');
const fs = require('fs')
const fileRead = promisify(fs.readFile)
const exist = promisify(fs.exists)
const magnetParse = require('../magnetParse')
const mkdirp = require('mkdirp')

module.exports = class Rutor
{
	constructor(props)
	{
		this.p2p = props.p2p
		this.dataDirectory = props.dataDirectory
		let t = setTimeout(() => this.recheck(), 30000)
		t.unref()
	}

	get name() { return 'rutor' }

	async findHash(hash)
	{
		if(!this.dataDirectory)
			return

		let id;
		const checkInDatabase = async (file, noCheck) => {
			const fileName = this.dataDirectory + `/${file}`
			if(this.p2p && !noCheck && !(await exist(fileName)))
			{
				logT('rutor', 'download hash database', file)
				await this.p2p.file('rutor')
				logT('rutor', 'downloaded hash database', file)
			}

			if(id)
				return

			let idMapFile = await fileRead(fileName)
			idMapFile = JSON.parse(idMapFile).hashes
			id = idMapFile[hash]
		}

		try
		{
			await checkInDatabase(`rutor/rutor.${hash[0]}.json`)
			await checkInDatabase(`rutor/rutor.x.json`, true)
			if(id)
				return this.parse(id)
		} catch(error)
		{
			return
		}
	}

	async recheck(page = 0) {
		if(!this.dataDirectory)
			return

		if(!this.rutorMap && fs.existsSync(this.dataDirectory + '/rutor/rutor.x.json'))
		{
			let data = JSON.parse(fs.readFileSync(this.dataDirectory + '/rutor/rutor.x.json'))
			this.rutorMap = data.hashes
			logT('rutor', 'add records to', Object.keys(this.rutorMap).length)
		}
		else if(!this.rutorMap) 
			this.rutorMap = {}

		if(page > 10)
		{
			delete this.rutorMap
			return
		}

		let html
		try {
			html = await fetch(`http://rutor.is/browse/${page}/0/0/0`)
		} catch(err) {
			return
		}
		if(!html)
			return
		html = await html.textConverted()
		const $ = cheerio.load(html)
    
		const rutorMap = this.rutorMap
		$('#index tr').each(function(i, elem) {
			const row = $(this)
			const nameField = row.find('td').next()
			if(!nameField)
				return
			let id = nameField.find('a').attr('href')
			if(!id)
				return
			id = id.match(/download\/([0-9]+)/)[1]
			id = parseInt(id)
			const hash = magnetParse(nameField.find('a').next().attr('href'))
    
			rutorMap[hash] = id
		});
    
		await mkdirp(`${this.dataDirectory}/rutor`)
		fs.writeFileSync(`${this.dataDirectory}/rutor/rutor.x.json`, JSON.stringify({
			date: Date.now(),
			hashes: this.rutorMap
		}, null, 4), 'utf8');
    
		logT('rutor', 'parse new links page', page)
		setTimeout(() => this.recheck(page + 1), 30)
	}

	async parse(id)
	{
		let html; 
		try {
			html = await fetch(`http://www.rutor.is/torrent/${id}`)
		} catch(err) {
			return
		}
		if(!html)
			return
		html = await html.textConverted()
		html = html.replace(/textarea/g, 'div')
		const $ = cheerio.load(html)
		const topicTitle = $('h1').text()
		if(!topicTitle)
			return

		let contentCategory
		$('#details tr').each(function(i, elem) {
			const row = $(this)
			const field = row.find('td.header').text()
			if(field == 'Категория')
			{
				contentCategory = row.find('td').next().text().trim()
			}
		});
    

		return {
			name: topicTitle,
			poster: $('#details tbody img').attr('src'),
			description: $('#details tbody').first().text(),
			rutorThreadId: parseInt(id),
			contentCategory
		}
    
	}

}