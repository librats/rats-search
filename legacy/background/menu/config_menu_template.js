import { app, BrowserWindow } from "electron";
import fs from 'fs'
import path from 'path'
import __, { translationsDir } from '../../app/translation'

export const settingsMenuTemplateFunc = (config, onLanguageChange) => ({
	label: __("Settings"),
	submenu: [
		{
			label: __("Main Settings"),
			accelerator: "CmdOrCtrl+O",
			click: () => {
				BrowserWindow.getFocusedWindow().webContents.send('url', '/config')
			}
		},
		{
			label: __("Torrents Filters"),
			accelerator: "CmdOrCtrl+\\",
			click: () => {
				BrowserWindow.getFocusedWindow().webContents.send('url', '/filters')
			}
		},
		{
			label: __("Language"),
			submenu: (() => {
				const translations = []
				const translationsDirectory = translationsDir()
				fs.readdirSync(translationsDirectory).forEach(translation => {
					const translationJson = JSON.parse(fs.readFileSync(`${translationsDirectory}/${translation}`, 'utf8'))
					const lang = path.basename(translation, '.json')
					translations.push({
						label: translationJson.nameOriginal,
						type: 'checkbox',
						checked: config.language === lang,
						click: () => {
							BrowserWindow.getFocusedWindow().webContents.send('changeLanguage', lang)
							config.language = lang
							if(onLanguageChange)
								onLanguageChange(lang)
							console.log('changed translation to:', lang)
						}
					})
				})
				return translations
			})()
		}
	]
});
