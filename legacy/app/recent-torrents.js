import React, { Component } from 'react';
import TorrentLine from './torrent'
import {List} from 'material-ui/List';
import Divider from 'material-ui/Divider';
import Subheader from 'material-ui/Subheader';
import Paper from 'material-ui/Paper';
import FlatButton from 'material-ui/FlatButton';
import RefreshIndicator from 'material-ui/RefreshIndicator';

export default class RecentTorrents extends Component {
	constructor() {
		super()
		this.torrents = [];
		this.torrentsAssoc = {};
		this.displayQueue = [];
		this.displayQueueAssoc = {};
		this.maxQueueSize = 1000;
		this.maxDisplaySize = 10;
		this.state = { 
			pause: false,
			searchingIndicator: false
		}
	}
	componentDidMount() {
		window.torrentSocket.emit('recentTorrents', window.customLoader((data) => {
			if(data) {
				this.torrents = data;
				//this.forceUpdate(); // вызывается через searchingIndicator
			}

			this.displayNewTorrent = () => {
				if(!this.displayNewTorrent) {
					return;
				}

				if(this.displayQueue.length == 0) {
					setTimeout(this.displayNewTorrent, 1000);
					return;
				}

				const speed = 850;

				if(this.state.pause) {
					setTimeout(this.displayNewTorrent, speed);
					return;
				}

				let torrent = this.displayQueue.shift();
				this.torrents.unshift(torrent);
				this.torrentsAssoc[torrent.hash] = torrent;
				if(this.torrents.length > this.maxDisplaySize) {
					let toDelete = this.torrents.pop()
					delete this.torrentsAssoc[toDelete.hash];
					delete this.displayQueueAssoc[toDelete.hash];
				}
				this.displayTorrentCounterValue = this.displayQueue.length;

				this.forceUpdate();
				setTimeout(this.displayNewTorrent, speed);
			}
			this.displayNewTorrent();

			this.displayTorrentCounterValue = 0;
			this.displayTorrentCounter = setInterval(() => {
				if(this.displayTorrentCounterValue != this.displayQueue.length) {
					this.displayTorrentCounterValue = this.displayQueue.length;
					this.forceUpdate();
				}
			}, 40);
		}, () => {
			this.setState({
				searchingIndicator: true
			});
		}, () => {
			this.setState({
				searchingIndicator: false
			});
		}));
		this.newTorrentFunc = (torrent) => {
			if(this.displayQueue.length < this.maxQueueSize) {
				this.displayQueue.push(torrent);
				this.displayQueueAssoc[torrent.hash] = torrent;
			}
		};
		window.torrentSocket.on('newTorrent', this.newTorrentFunc);
    
		this.tracketUpdate = (statistic) => {
			if(statistic.hash in this.displayQueueAssoc)
			{
				Object.assign(this.displayQueueAssoc[statistic.hash], statistic);
				if(statistic.hash in this.torrentsAssoc) {
					this.forceUpdate();
				}
			}
		}
		window.torrentSocket.on('trackerTorrentUpdate', this.tracketUpdate);
	}
	pauseAndContinue() {
		this.setState({
			pause: !this.state.pause
		});
	}
	componentWillUnmount() {
		if(this.newTorrentFunc)
			window.torrentSocket.off('newTorrent', this.newTorrentFunc);
		if(this.tracketUpdate)
			window.torrentSocket.off('trackerTorrentUpdate', this.tracketUpdate);
		if(this.displayNewTorrent)
			delete this.displayNewTorrent;
		if(this.displayTorrentCounter)
			clearInterval(this.displayTorrentCounter);
	}
	render() {
		const style = {
			refresh: {
				display: 'inline-block',
				position: 'relative',
			},
		};

		if(this.state.searchingIndicator) {
			return (
				<div className='pad1'>
					<RefreshIndicator
						size={50}
						left={0}
						top={0}
						loadingColor="#FF9800"
						status="loading"
						style={style.refresh}
					/>
				</div>
			);
		}

		if(!this.torrents || this.torrents.length == 0)
			return null;

		return (
			<List className='animated recent-torrents torrents-container'>
				<Subheader className='recent-title' inset={true}>
					<FlatButton style={{marginRight: '8px'}} primary={true} label={__('top')} labelStyle={{color: "#a4c639"}} onClick={() =>{
						window.router('/top');
					}} />
					<FlatButton style={{marginRight: '8px'}} label={!this.state.pause ? __('running') : __('stoped')} secondary={this.state.pause} primary={!this.state.pause} onClick={() =>{
						this.pauseAndContinue()
					}} />
					{__('Most recent torrents')}{this.displayQueue.length > 0 ? ` (${__('and')} ${this.displayQueue.length} ${__('more')})` : null}
				</Subheader>
				<Divider />
				{
					this.torrents.map((torrent, index) =>{
						return <TorrentLine key={index} torrent={torrent} />;
					})
				}
			</List>
		);
	}
}
