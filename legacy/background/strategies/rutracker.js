
const fetch = require('node-fetch')
const cheerio = require('cheerio')

module.exports = class Rutracker
{
	get name() { return 'rutracker' }

	async findHash(hash)
	{
		this.hash = hash
		return await this.parse()
	}

	async parse()
	{
		let html; 
		try {
			html = await fetch('https://rutracker.org/forum/viewtopic.php?' + (this.threadId ? `t=${this.threadId}` : (this.hash ? `h=${this.hash}` : '')))
		} catch(err) {
			return
		}
		if(!html)
			return
		html = await html.textConverted()
		html = html.replace(/\<span class\="post-br"\>/g, '\n<span class="post-br">')
		html = html.replace(/\><span class\="post-b"\>/g, '>\n<span class="post-b">')
		const $ = cheerio.load(html)
		const topicTitle = $('#topic-title').text()
		if(!topicTitle)
			return

		let contentCategory;
		try {
			contentCategory = $('.vBottom .nav').text().replace(/[\t]+/g, '').replace(/[\n]+/g, ' ').trim()
		} catch(er) {}

		return {
			name: topicTitle,
			poster: $('.post_body .postImgAligned').attr('title') || $('.post_body .postImg').attr('title'),
			description: $('.post_body').first().text(),
			rutrackerThreadId: parseInt($('a.magnet-link').attr('data-topic_id')) || parseInt($('a.magnet-link-1').attr('data-topic_id')),
			contentCategory
		}
	}
}