const fs = require('fs')

function directoryFilesRecursive (directory, filesList = []) {
	let files;
	try {
		files = fs.readdirSync(directory)
	} catch(err) {
		if(err.code !== 'ENOTDIR')
			throw err
		else
			return [directory] // if file, return file
	}
	for (const file of files) {
		const filePath = `${directory}/${file}`
		if (fs.statSync(filePath).isDirectory()) {
			directoryFilesRecursive(filePath, filesList)
		} else {
			filesList.push(filePath)
		}
	}
	return filesList
}

module.exports = directoryFilesRecursive
