import React, { Component } from 'react';
import './app.css';
import './router';
import PagesPie from './pages-pie.js';
//import registerServiceWorker from './registerServiceWorker';
import MuiThemeProvider from 'material-ui/styles/MuiThemeProvider';
import __, { changeLanguage } from './translation'

import {Header} from './header'
import Footer from './footer'

window.__ = __


if(typeof WEB !== 'undefined')
{
	const io = require("socket.io-client");
	window.torrentSocket = io(document.location.protocol + '//' + document.location.hostname + (process.env.NODE_ENV === 'production' ? '/' : ':8095/'));
	const emit = window.torrentSocket.emit.bind(window.torrentSocket);
	window.torrentSocket.emit = (...data) => {
		let id;
		if(typeof data[data.length - 1] === 'function')
		{
			id = Math.random().toString(36).substring(5)
		}
		data.splice(1,0,id);
		emit(...data)
		return id
	}
}
else
{
	const { ipcRenderer } = require('electron');
	const remote = require('@electron/remote')
	window.currentWindow = remote.getCurrentWindow()

	window.torrentSocket = {}
	window.torrentSocket.callbacks = {}
	window.torrentSocket.listeners = {}
	window.torrentSocket.on = (name, func) => {
		const id = Math.random().toString(36).substring(2) + Math.random().toString(36).substring(2) + Math.random().toString(36).substring(2)
		const newListener = (event, ...data) => {
			func(...data)
		}
		window.torrentSocket.listeners[id] = newListener
		func._eventId = id

		ipcRenderer.on(name, newListener);
	}
	window.torrentSocket.off = (name, func) => {
		if(!func)
			ipcRenderer.removeAllListeners(name);
		else
		{
			const realListener = window.torrentSocket.listeners[func._eventId]
			if(realListener)
			{
				ipcRenderer.removeListener(name, realListener);
				delete window.torrentSocket.listeners[func._eventId]
			}
			delete func._eventId
		}
	}
	window.torrentSocket.emit = (name, ...data) => {
		let id;
		if(typeof data[data.length - 1] === 'function')
		{
			id = Math.random().toString(36).substring(5)
			window.torrentSocket.callbacks[id] = data[data.length - 1];
			data[data.length - 1] = {callback: id}
		}
		ipcRenderer.send(name, data)
		return id
	}
	ipcRenderer.on('callback', (event, id, data) => {
		const callback = window.torrentSocket.callbacks[id]
		if(callback) {
			if(data)
				callback(JSON.parse(data))
			else
				callback(data)
		}
		delete window.torrentSocket.callbacks[id]
	});

	ipcRenderer.on('url', (event, url) => {
		console.log('url', url)
		router(url)    
	});

}

//registerServiceWorker();

// override log to main process
const consoleLog = console.log
console.log = (...log) => {
	window.torrentSocket.emit('log', ...log)
	consoleLog(...log)
}
const consoleError = console.error
console.error = (...log) => {
	let type = 'logE'

	if(log[0] && log[0].startsWith('Warning:'))
		type = 'log'
	if(log[0] && log[0].includes('MaxListenersExceededWarning:'))
		type = 'log'

	window.torrentSocket.emit(type, ...log)
	consoleError(...log)
}

window.onerror = function (msg, url, line, col, error) {
	if(!error || !error.stack)
		console.error(msg, 'at', url + ':' + line)
	else
		console.error(error.stack)

	return true; // same as preventDefault
};

let loadersCount = 0;
let appReady = false;
window.customLoader = (func, onLoading, onLoaded) => {
	loadersCount++;
	if(onLoading) {
		onLoading();
	}
	return (...args) => {
		func(...args);
		if(onLoaded) {
			onLoaded();
		}
		loadersCount--;
	}
};

window.isReady = () => {
	return (appReady && loadersCount === 0)
}

window.peers = 0;
window.peersTorrents = 0;

class App extends Component {
	constructor(props)
	{
		super(props)
		window.torrentSocket.emit('config', (config) => {
			window.initConfig = config
			window.darkMode = config.darkMode;
			changeLanguage(config.language, () => {
				if(appReady)
					this.forceUpdate()
			})
		});
	}

	componentDidMount() {
		window.torrentSocket.on('peer', (peer) => {
			if(peer.size > window.peers)
				window.peersTorrents = (window.peersTorrents || 0) + peer.torrents
			else
				window.peersTorrents = (window.peersTorrents || 0) - peer.torrents
			window.peers = peer.size
			this.forceUpdate()
		})

		window.torrentSocket.emit('peers', (peers) => {
			if(peers.size > 0 || window.peers == 1)
			{
				window.peers = peers.size
				window.peersTorrents = peers.torrents
				this.forceUpdate()
			}
		})

		window.torrentSocket.emit('p2pStatus', (status) => {
			if(status == 0)
				return

			window.p2pStatus = status
			this.forceUpdate()
		})
		window.torrentSocket.on('p2pStatus', (status) => {
			window.p2pStatus = status
			this.forceUpdate()
		})

		window.torrentSocket.on('changeLanguage', (lang) => {
			changeLanguage(lang, () => this.forceUpdate())
		})

		window.torrentSocket.on('configChanged', ({darkMode}) => {
			if (typeof darkMode != 'undefined') {
				console.log('changed darkMode to ' + darkMode)
				window.darkMode = darkMode;
				this.forceUpdate()
			}
		})

		const processTorrents = async (files) => {
			if(!files || files.length == 0)
				return

			files = await Promise.all(Array.from(files).filter(file => (file.type == 'application/x-bittorrent' || file.type == '')).map(file => {
				if(file.path)
					return {path: file.path};

				return new Promise(resolve => {
					const reader = new FileReader();
					reader.onload = () => resolve({data: reader.result});
					reader.readAsArrayBuffer(file);
				})
			}));
			torrentSocket.emit('dropTorrents', {pathTorrents: files});
		}

		document.addEventListener('dragover', (event) => {
			event.stopPropagation();
			event.preventDefault();
			event.dataTransfer.dropEffect = 'copy'; // Explicitly show this is a copy.
		}, false);
		document.addEventListener('drop', (event) => {
			event.stopPropagation();
			event.preventDefault();
			processTorrents(event.dataTransfer.files); // FileList object.
		}, false);

		window.router()
		appReady = true;
	}
	componentWillUnmount() {
		appReady = false;
	}
	render() {
		const checkNotModal = (window.currentWindow && !window.currentWindow.isModal()) || typeof WEB !== 'undefined'

		if(checkNotModal && !window.initConfig)
			return null // nothing to do yet

		return (
			<MuiThemeProvider>
				<div className={window.darkMode ? 'darkMode' : ''}>
					{
						checkNotModal
                        &&
                        <Header />
					}
					<PagesPie />
					<Footer />
				</div>
			</MuiThemeProvider>
		);
	}
}

export default App;
