import React from 'react';
import Page from './page';

import TorrentLine from './torrent'
import {List, ListItem} from 'material-ui/List';
import Divider from 'material-ui/Divider';
import Subheader from 'material-ui/Subheader';
import RaisedButton from 'material-ui/RaisedButton';
import LinearProgress from './LinearProgress';

import {Tabs, Tab} from 'material-ui/Tabs';
import _ from 'lodash'

export default class TopPage extends Page {
	constructor(props) {
		super(props)
		this.setTitle('Rats On The Boat - Torrents top');
		this.topTorrents = {};
		this.types = ['main', 'video', 'audio', 'books', 'pictures', 'application', 'archive']
		this.descriptions = {
			main: __('All'),
			video: __('Video'),
			audio: __('Audio/Music'),
			books: __('Books'),
			pictures: __('Pictures/Images'),
			application: __('Apps/Games'),
			archive: __('Archives')
		}
		this.times = {
			overall: __('Overall'),
			hours: __('Last hour'),
			week: __('Last week'),
			month: __('Last month')
		}
		this.state = {type: 'main', time: 'overall'}
	}
	loadMoreTorrents(type, time)
	{
		this.firstUpdate = false
		time = time ? time : this.state.time
		const page = (this.topTorrents[type] && this.topTorrents[type][time] && this.topTorrents[type][time].page) || 0
		window.torrentSocket.emit('topTorrents', {
				type: (type == 'main' ? null : type), 
				navigation: {index: page * 20, limit: 20, time}
			},
			window.customLoader((torrents) => {
				this.mergeTorrents(torrents, type, time)
				this.topTorrents[type][time].page = page + 1
			})
		)
	}
	mergeTorrents(torrents, type, time)
	{
		let isNewTab = false
		if(!this.topTorrents[type])
		{
			this.topTorrents[type] = {}
			isNewTab = true
		}
		if(!this.topTorrents[type][time])
			this.topTorrents[type][time] = {torrents: [], page: 0}

		if(!torrents || torrents.length == 0)
			return

		this.topTorrents[type][time].torrents = _.orderBy(_.unionBy(this.topTorrents[type][time].torrents, torrents, 'hash'), ['seeders'], ['desc'])
        
		if((this.state.type == type && this.state.time == time) || isNewTab)
		{
			this._update();
		}
	}
	_update()
	{
		if(!this.firstUpdate)
		{
			this.firstUpdate = true
			this.forceUpdate()
			return
		}

		if(this.timeForce)
			return
		this.timeForce = setTimeout(() => {
			delete this.timeForce
			this.forceUpdate()
		}, 700)
	}
	componentDidMount()
	{
		super.componentDidMount();
		for(const type of this.types)
		{
			this.loadMoreTorrents(type)
		}
		this.remoteTopTorrents = ({torrents, type, time}) => {
			time = time ? time : 'overall'
			type = type ? type : 'main'
			this.mergeTorrents(torrents, type, time)
		}
		window.torrentSocket.on('remoteTopTorrents', this.remoteTopTorrents);
	}
	componentWillUnmount()
	{
		if(this.remoteTopTorrents)
			window.torrentSocket.off('remoteTopTorrents', this.remoteTopTorrents);
	}
	render() {
		return (
			<div>
				<div className='column center w100p'>
					{
						Object.keys(this.topTorrents).length == 0
            &&
            <div className='pad0-75 w100p '>
            	<LinearProgress mode="indeterminate" />
            </div>
					}
					<Tabs
						className='w100p'
						value={this.state.type}
						onChange={(type) => {
							this.setState({type});
							// lost other content
							if(!this.topTorrents[type][this.state.time])
							{
								this.loadMoreTorrents(type, this.state.time)
							}
						}}
						tabItemContainerStyle={{flexWrap: 'wrap', alignItems: 'stretch', whiteSpace: 'normal'}}
						inkBarStyle={{display: 'none'}}
					>
						{
							this.types.map((type, index) => {
								if(!this.topTorrents[type])
									return null;

								return (
									<Tab buttonStyle={type === this.state.type ? {fontWeight: 'bold'} : undefined} style={{minWidth: 150}} key={index} label={this.descriptions[type]} value={type}>
										<Tabs
											className='w100p'
											value={this.state.time}
											onChange={(time) => {
												this.setState({time})
												// lost other content
												if(!this.topTorrents[type][time])
												{
													this.loadMoreTorrents(type, time)
												}
											}}
											tabItemContainerStyle={{flexWrap: 'wrap', alignItems: 'stretch', whiteSpace: 'normal'}}
											inkBarStyle={{display: 'none'}}
										>
											{
												Object.keys(this.times).map((time, index) => {
													const {torrents} = this.topTorrents[type][time] || {torrents: undefined};

													// dont draw top on other page rather than focused
													if(this.state.type !== type || this.state.time !== time)
													{
														return <Tab buttonStyle={time === this.state.time ? {fontWeight: 'bold'} : undefined} style={{minWidth: 150}} key={index} label={this.times[time]} value={time}></Tab>
													}

													if(!torrents)
														return (
															<Tab buttonStyle={time === this.state.time ? {fontWeight: 'bold'} : undefined} style={{minWidth: 150}} key={index} label={this.times[time]} value={time}>
																<div className='pad0-75 w100p '>
																	<LinearProgress mode="indeterminate" />
																</div>
															</Tab>
														)

													return (
														<Tab buttonStyle={time === this.state.time ? {fontWeight: 'bold'} : undefined} style={{minWidth: 150}} key={index} label={this.times[time]} value={time}>
															<List style={{minWidth: '20em'}}>
																{
																	torrents.map((torrent, index) => {
																		return <TorrentLine key={index} torrent={torrent} />
																	})
																}
																{
																	torrents.length > 0
                          &&
                          <div>
                          	<ListItem innerDivStyle={{textAlign: 'center', padding: '1em'}} primaryText={<span>{__('More Torrents')}</span>} onClick={() => {
                          		this.loadMoreTorrents(type)
                          	}} />
                          	<Divider />
                          </div>
																}
															</List>
														</Tab>)
												})
											}
                    
										</Tabs>
									</Tab>
								)
              
							})
						}
					</Tabs>
				</div>
			</div>
		);
	}
}
