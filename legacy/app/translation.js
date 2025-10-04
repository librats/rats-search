const fs = require('fs')
const env = typeof WEB !== 'undefined' ? false : require('env')
let dictionary = {}

const translationsDir = () => {
	if(env.name == 'production')
		return process.resourcesPath + '/translations'
	else
		return 'translations'
}

function loadJSON(file, callback) {
	if(fs && env)
	{
		callback(JSON.parse(fs.readFileSync(`${translationsDir()}/${file}`, 'utf8')))
	}
	else
	{
		const xobj = new XMLHttpRequest();
		xobj.overrideMimeType("application/json");
		xobj.open('GET', `translations/${file}`, true);
		xobj.onreadystatechange = function() {
			if (xobj.readyState == 4 && xobj.status == 200) {
				// .open will NOT return a value but simply returns undefined in async mode so use a callback
				callback(JSON.parse(xobj.responseText));
			}
		}
		xobj.send(null);
	}
}

const changeLanguage = (lang, callback) => {
	loadJSON(`${lang}.json`, (data) => {
		dictionary = data.translations
		if(callback)
			callback()
	})
}

export { changeLanguage, translationsDir }

export default (word) => {
	const translation = dictionary[word]
	if(translation === undefined)
	{
		if(fs && env && env.name === "development")
		{
			dictionary[word] = word
			fs.readdirSync('translations').forEach(translation => {
				console.log('update translation in file', translation)
				const translationJson = JSON.parse(fs.readFileSync(`translations/${translation}`, 'utf8'))
				translationJson.translations[word] = word
				fs.writeFileSync(`translations/${translation}`, JSON.stringify(translationJson, null, 4), 'utf8');
			})
		}
		return word
	}
	return translation
}
