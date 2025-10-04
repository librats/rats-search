module.exports = (filesData) => {
	if(Array.isArray(filesData))
		filesData = filesData[0]

	let path = filesData.path.split('\n');
	let size = filesData.size.split('\n');
    
	return path.map((pathString, index) => Object.assign({}, filesData, {
		path: pathString,
		size: parseInt(size[index])
	}))
}