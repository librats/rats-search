
const fetch = require('node-fetch')
const cheerio = require('cheerio')

module.exports = class Stragegie
{
	get name() { return '1337x' }

	async findHash(hash)
	{
		let html; 
		try {
			html = await fetch(`https://1337x.to/srch?search=${hash}`)
		} catch(err) {
			return
		}
		if(!html)
			return
		html = await html.textConverted()
		const $ = cheerio.load(html)

		this.href = $($('.table-list tr td a').get(1)).attr('href')
		if(this.href)
			this.id = this.href.match(/\/torrent\/([0-9]+)\//)[1];

		return await this.parse();
	}

	async parse()
	{
		let html; 
		try {
			html = await fetch('https://1337x.to' + this.href)
		} catch(err) {
			return
		}
		if(!html)
			return
		html = await html.textConverted()
		const $ = cheerio.load(html)
		const topicTitle = $('h1').text()
		if(!topicTitle)
			return

		let contentCategory;
		try {
			contentCategory = $('.torrent-category-detail .list li').first().find('span').text()
		} catch(er) {}

		let poster = $('.torrent-image img').attr('src') || $('#description img').attr('data-original');
		if(poster)
			poster = poster.replace(/^\/\//, 'https://');

		return {
			name: topicTitle.trim(),
			poster,
			description: $('#description').text(),
			x1337ThreadId: parseInt(this.id),
			x1337Href: this.href,
			contentCategory
		}
	}
}