const objectHash = require('object-hash');
const EventEmitter = require('events');
const forBigTable = require('./forBigTable')

module.exports = class P2PStore extends EventEmitter {
	constructor(p2p, sphinx)
	{
		super()
		this.id = 0
		Object.defineProperty(p2p.info, 'store', { 
			enumerable: true,
			get: () => this.id
		});
		this.synchronized = false

		logT('store', 'connect p2p store...')
		this.p2p = p2p
		this.sphinx = sphinx

		this.sphinx.query("SELECT MAX(`id`) as mx from store", (err, rows) => {
			if(err)
				return

			if(rows[0] && rows[0].mx >= 1)
				this.id = rows[0].mx;
                
			logT('store', 'store db index', this.id)

			this.p2p.events.on('peer', (peer) => {
				if(peer.info && peer.info.store)
				{
					if(peer.info.store > this.id)
						this.sync(peer) // sync db
					else if(peer.info.store === this.id)
						this.synchronized = true
				}
			})
		})

		this.p2p.on('dbStore', (record) => { 
			if(!record || record.id - 1 !== this.id)
			{
				logT('store', 'out of range peerdb store', record.id)
				return
			}

			this._syncRecord(record, () => {
				// redirect other peers that record are stored
				this.p2p.emit('dbStore', record)
			})
		})

		this.p2p.on('dbSync', ({id} = {}, callback) => {
			logT('store', 'ask to sync db from', id, 'version')
			if(typeof id === 'undefined' || id >= this.id)
			{
				callback(false)
				return
			}

			// back 
			this.sphinx.query(`select * from store where id > ${id}`, (err, records) => {
				if(err)
				{
					logT('store', err)
					return
				}
    
				if(records.length > 0)
					callback({
						records,
						index: this.id
					})
			})
		})
	}

	sync(peer)
	{
		logT('store', 'sync db on version', this.id, peer ? `from peer ${peer.peerId}` : '')
		const processSync = (data, nil, peer) => {
			if(!data || !data.records)
				return

			const oldIndex = this.id
			data.records.forEach(record => this._syncRecord(record))

			// peer also can contain another part of store records, try to sync them all
			if(data.index >= 0 
                && oldIndex < this.id // last sync update of store must be successful, otherwise no point to try sync db from this peer
                && this.id < data.index)
			{
				logT('store', 'continue sync store from', this.id, 'index', 'peer', peer.peerId)
				peer.emit('dbSync', {id: this.id}, processSync)
			}
		}
		if(peer)
			peer.emit('dbSync', {id: this.id}, processSync)
		else
			this.p2p.emit('dbSync', {id: this.id}, processSync)

		this.synchronized = true
	}

	_syncRecord(record, callback)
	{
		if(!record)
			return

		if(!record.id)
			return

		if(record.id <= this.id)
			return
            
		if(typeof record.data !== 'object')
			record.data = JSON.parse(record.data)
            
		// check hash
		if(objectHash(record.data) !== record.hash)
		{
			logT('store', 'wrong hash for sync peerdb')
			return
		}

		// set myself to false
		record.myself = false

		// push to db
		logT('store', 'sync peerdb record', record.id)
		this._pushToDb(record)
		this.id = record.id

		// redirect to next
		if(callback)
			callback()
	}

	_pushToDb(value, callback)
	{
		const data = this.sphinx.escape(JSON.stringify(value.data))
		this.sphinx.query(
			`insert into store(id, hash, peerId, data` + (value.index || value.data._index ? ', storeIndex' : '') + `) 
            values('${value.id}', '${value.hash}', '${value.peerId || value.peerid}', ${data}` + (value.index || value.data._index ? ',' + this.sphinx.escape(value.index || value.data._index) : '') + ')', 
			(err) => {
				if(err)
				{
					logTE('store', err)
					return
				}

				this.emit('store', value)
				if(callback)
					callback()
			})
	}

	store(obj)
	{
		if(!this.synchronized)
		{
			logT('store', 'cant store item on unsync db')
			return false
		}

		// clean temp from object
		const temp = obj._temp
		delete obj._temp

		const value = {
			id: ++this.id,
			hash: objectHash(obj),
			data: obj,
			index: obj._index,
			peerId: this.p2p.peerId,
			myself: true,
			temp
		}

		logT('store', 'store object', value.id)

		this._pushToDb(value, () => {
			// store record
			this.p2p.emit('dbStore', value) 
		})

		return true
	}

	async find(index)
	{
		const records = []
		await forBigTable(this.sphinx, 'store', (record) => records.push(record), null, 1000, `and match(${this.sphinx.escape(index)})`)
		return records.map( ({data, peerid}) => Object.assign(JSON.parse(data), { _peerId: peerid }) )
	}
}