const config = require('./config');
const rating = require('../app/rating');

module.exports = class Feed {
	constructor({sphinx})
	{
		this.feed = []
		this.sphinx = sphinx
		this.loaded = false
		this.max = 1000
		this.feedDate = 0
	}

	size()
	{
		return this.feed.length
	}

	async save() {
		if(!this.loaded)
			return // feed not loaded on begining, ignore saving

		logT('feed', 'saving feed')
		config.feedDate = this.feedDate
		await this.sphinx.query('delete from feed where id > 0')
		let id = 0
		await Promise.all(
			this.feed.map(
				async record => await this.sphinx.query('insert into feed(id, data) values(?, ?)', [++id, JSON.stringify(record)])
			)
		)
	}
    
	async load() {
		this.feed = await this.sphinx.query('select * from feed limit 1000')
		if(this.feed && this.feed.length > 0)
			this.feed = this.feed.map(f => JSON.parse(f.data))
		else
			this.feed = []
            
		this._order()
		this.feedDate = config.feedDate
		this.loaded = true
		logT('feed', 'lodead feed')
	}

	clear()
	{
		logT('feed', 'clearing feed')
		this.feed = []
	}

	add(data) {
		let index = -1
		if(data.hash)
			index = this.feed.findIndex(element => element.hash === data.hash)

		if(index >= 0)
			this.feed[index] = Object.assign(this.feed[index], data) // just push up element
		else
		{
			if(typeof data == 'object')
			{
				data.feedDate = Math.floor(Date.now() / 1000)
			}

			if(this.feed.length >= this.max)
			{
				//cleanup
				for(let i = this.feed.length - 1; i >= 0; i--)
					if(this._compare(this.feed[i]) <= 0)
						this.feed.pop()
					else
						break

				if(this.feed.length >= this.max)
					this.feed[this.feed.length - 1] = data // replace last one
				else
					this.feed.push(data) // insert
			}
			else
			{
				this.feed.push(data) // insert
			}
		}

		this._order()
		this.feedDate = Math.floor(Date.now() / 1000)
	}

	_order() {
		this.feed.sort((a, b) => this._compare(b) - this._compare(a))
	}

	_compare(x)
	{
		const good = (x && x.good) || 0
		const bad = (x && x.bad) || 0
		const comments = 0
		let time = Math.floor(Date.now() / 1000) - ((x && x.feedDate) || 0)

		const maxTime = 600000
		if(time > maxTime)
			time = maxTime
		const relativeTime = (maxTime - time) / maxTime
		return (
			relativeTime * relativeTime 
            + good * 1.5 * relativeTime 
            + comments * 4 * relativeTime 
            - bad * 0.6 * relativeTime
            + rating(good, bad)
		)
	}
}