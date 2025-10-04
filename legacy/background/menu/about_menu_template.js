import { app, BrowserWindow, shell } from "electron";
import path from "path";
import url from "url";
import __ from '../../app/translation'

export const aboutMenuTemplateFunc = () => ({
	label: __("About"),
	submenu: [
		{
			label: __("Changelog"),
			accelerator: "CmdOrCtrl+]",
			click: () => {
				const win = new BrowserWindow({
					parent: BrowserWindow.getFocusedWindow(),
					modal: true,
					webPreferences: {
						nodeIntegration: true,
						enableRemoteModule: true,
						contextIsolation: false,
					}
				})
				win.setMenu(null)
				win.loadURL(url.format({
					pathname: path.join(__dirname, "app.html"),
					protocol: "file:",
					slashes: true
				}))
				require("@electron/remote/main").enable(win.webContents);
				win.webContents.on('did-finish-load', () => {
					logT('changelog', "finish load page, open changlog")
					setTimeout(() => win.send('url', '/changelog'), 0)
				});

				const handleRedirect = (e, url) => {
					if(url != win.webContents.getURL()) {
						e.preventDefault()
						shell.openExternal(url)
					}
				}
        
				win.webContents.on('will-navigate', handleRedirect)
				win.webContents.on('new-window', handleRedirect)
			},
		},
		{
			label: __("Bug Report"),
			accelerator: "CmdOrCtrl+[",
			click: () => {
				shell.openExternal('https://github.com/DEgITx/rats-search/issues')
			},
		},
		{
			label: __("Donate"),
			accelerator: "CmdOrCtrl+*",
			click: () => {
				const win = new BrowserWindow({
					parent: BrowserWindow.getFocusedWindow(),
					modal: true,
					width: 1000
				})
				win.setMenu(null)
				win.loadURL(url.format({
					pathname: path.join(__dirname, "donate.html"),
					protocol: "file:",
					slashes: true
				}))

				const handleRedirect = (e, url) => {
					if(url != win.webContents.getURL()) {
						if(!url.includes('patreon'))
							return

						e.preventDefault()
						shell.openExternal(url)
					}
				}
        
				win.webContents.on('will-navigate', handleRedirect)
				win.webContents.on('new-window', handleRedirect)
			},
		},
		{
			label: __("Help (Documentation)"),
			accelerator: "CmdOrCtrl+?",
			click: () => {
				shell.openExternal('https://github.com/DEgITx/rats-search/blob/master/docs/MANUAL.md')
			},
		},
		{
			label: __("Support (Discussion)"),
			accelerator: "CmdOrCtrl+>",
			click: () => {
				shell.openExternal('https://discord.gg/t9GQtxA')
			},
		},
		{
			label: __("About (GitHub)"),
			accelerator: "CmdOrCtrl+<",
			click: () => {
				shell.openExternal('https://github.com/DEgITx/rats-search')
			},
		}
	]
});
