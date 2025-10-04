import { app, BrowserWindow, shell } from "electron";
import path from "path";
import url from "url";
import __ from '../../app/translation'

export const manageMenuTemplateFunc = () => ({
	label: __("Manage"),
	submenu: [
		{
			label: __("Downloads"),
			accelerator: "CmdOrCtrl+d",
			click: () => {
				BrowserWindow.getFocusedWindow().webContents.send('url', '/downloads')
			},
		},
		{
			label: __("Feed"),
			accelerator: "CmdOrCtrl+n",
			click: () => {
				BrowserWindow.getFocusedWindow().webContents.send('url', '/')
			},
		},
		{
			label: __("Top"),
			accelerator: "CmdOrCtrl+t",
			click: () => {
				BrowserWindow.getFocusedWindow().webContents.send('url', '/top')
			},
		},
		{
			label: __("Activity"),
			accelerator: "CmdOrCtrl+m",
			click: () => {
				BrowserWindow.getFocusedWindow().webContents.send('url', '/activity')
			},
		}
	]
});
