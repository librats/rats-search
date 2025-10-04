module.exports = (magnet) => {
	const match = /magnet:\?xt=urn:btih:([0-9a-f]+)/i.exec(magnet)
	if(!match)
		return
	if(match[1].length === 40)
		return match[1].toLowerCase()
	return
}