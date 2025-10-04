import React, { Component } from 'react';
import AutoScrollable from './auto-scrollable'
import TorrentLine from './torrent'
import {List} from 'material-ui/List';
import Divider from 'material-ui/Divider';
import Subheader from 'material-ui/Subheader';

export default class RecentTorrents extends AutoScrollable {
	constructor() {
		super()
		this.torrents = [];
	}
	componentDidMount() {
		super.componentDidMount();
		this.loadMoreFeed();
		this.feedFunc = ({feed}) => {
			this.torrents = feed.slice(0, this.torrents.length || 20)
			this.forceUpdate()
		};
		window.torrentSocket.on('feedUpdate', this.feedFunc);
	}
	componentWillUnmount() {
		super.componentWillUnmount();
		if(this.feedFunc)
			window.torrentSocket.off('feedUpdate', this.feedFunc);
	}
	onBottomScroll() {
		this.loadMoreFeed();
	}
	loadMoreFeed() {
		window.torrentSocket.emit('feed', {index: this.torrents.length}, window.customLoader((data) => {
			if(data) {
				this.torrents = this.torrents.concat(data);
				this.forceUpdate();
			}
		}))
	}
	render() {
		return (
			<List className='animated torrents-container'>
				<Subheader className='recent-title' inset={true}>
					{__('Feed')}
				</Subheader>
				<Divider />
				{
					this.torrents.map((torrent, index) =>{
						if(torrent.contentCategory === 'xxx')
							return

						return <TorrentLine key={index} torrent={torrent} />;
					})
				}
			</List>
		);
	}
}
