const glob = require("glob")

module.exports = (path) => new Promise((resolve) => glob(path, (error, files) => resolve(files)))