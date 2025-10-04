const google = require('google')

module.exports = (search) => new Promise((resolve) => {
	google(search, (err, res) => {
		resolve(res.links)
	})
})