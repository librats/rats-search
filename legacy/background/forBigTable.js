module.exports = (sphinx, table, callback, doneCallback, max = 1000, where = '', intermediateCallback = null) => new Promise((done) => {
	const checker = async (index = 0) => {
		const finish = () => {
			if(doneCallback)
				doneCallback(true)
			done(true)
		}
		const data = await sphinx.query(`SELECT * FROM ${table} WHERE id > ${index} ${where} ORDER BY id ASC LIMIT ${max}`);
		if(data.length == 0) {
			finish()
			return;
		}

		await Promise.all(data.map(callback));
		if(intermediateCallback)
			await intermediateCallback(data[data.length - 1]);

		if(data.length === max)
			checker(data[data.length - 1].id)
		else
			finish()
	}
	checker()
})