const fetch = require('node-fetch')
const cheerio = require('cheerio')

module.exports = class Nyaa
{
	get name() { return 'nyaa' }

	async findHash(hash)
	{
		this.hash = hash
		return await this.parse()
	}

	async parse()
	{
		if(this.promise)
			await this.promise

		this.promise = new Promise(async (resolve) => {
			let html;
			try {
				html = await fetch('https://nyaa.si/' + (this.threadId ? `view/${this.threadId}` : (this.hash ? `?q=${this.hash}` : '')))
			} catch(err) {
				resolve()
				return
			}
			if(!html) {
				resolve()
				return
			}
			html = await html.textConverted()
			const $ = cheerio.load(html)
			let topicTitle = $('.panel-title').first().text()
			if(!topicTitle) {
				resolve()
				return
			}

			topicTitle = topicTitle.replace(/\t/g, '').replace(/\n/g, '')

			resolve({
				name: topicTitle,
				description: $('#torrent-description').text(),
			})
		})

		return await this.promise
	}
}