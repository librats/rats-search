import React from 'react';
import Page from './page';
import formatBytes from './format-bytes'

import {List, ListItem} from 'material-ui/List';
import Subheader from 'material-ui/Subheader';
import {Tabs, Tab} from 'material-ui/Tabs';
import ActionInfo from 'material-ui/svg-icons/action/info';
import RaisedButton from 'material-ui/RaisedButton';
import Toggle from 'material-ui/Toggle';

import FileFolder from 'material-ui/svg-icons/file/folder';
import NoImage from './images/no-image-icon.png'

var moment = require('moment');
import RefreshIndicator from 'material-ui/RefreshIndicator';
let rating = require('./rating');
import LinearProgress from './LinearProgress';
import FlatButton from 'material-ui/FlatButton';
import {fileTypeDetect} from './content'
import {contentIcon} from './torrent'
import TrackersImages from './trackers-images'
import DownloadTorrentMenu from './download-torrent-menu'
import {torrentTypeDetect, niceTypeColor} from './content';
import { PieChart } from 'react-minimal-pie-chart';

let parseDescriptionText = (text) => {
	return text.split("\n").map(function(item) {
		const text = /(.+?:)(.*)/.exec(item)

		return (
			<span>
				{
					text ? <span><b>{`${text[1]} `}</b>{text[2]}</span> : item
				}
				<br/>
			</span>
		)
	})
}

let buildFilesTree = (filesList) => {
	let rootTree = {
		__sizeBT: 0
	};
	filesList.forEach((file) => {
		let pathTree = file.path.split('/');
		let currentItem = rootTree;
		pathTree.forEach((pathItem, index) => {
			if(!(pathItem in currentItem)) 
			{
				// крайний индекс, значит это объект файла, объединяем объекты
				if(index === pathTree.length - 1)
				{
					file.__sizeBT = 0
					file.__fileBT = true
					currentItem[pathItem] = file
				}
				else
				{
					currentItem[pathItem] = {
						__sizeBT: 0
					}
				}
			}
			currentItem = currentItem[pathItem]
			currentItem.__sizeBT += file.size;
		})
		rootTree.__sizeBT += file.size;
	});
	return rootTree;
}

const treeToTorrentFiles = (tree, torrent, toggles) => {
	// toggles for button disable/enable torrent/directory in torrent client
	if(toggles)
	{
		if(tree.__fileBT && typeof tree.downloadIndex !== 'undefined')
		{
			toggles.push({ 
				downloadIndex: tree.downloadIndex,
				selected: typeof tree.downloadSelected === 'undefined' || tree.downloadSelected 
			})
		}
	}

	// this is already file, return
	if(tree.__fileBT)
		return

	let arr = [];
	for(let file in tree)
	{
		if(file == '__sizeBT')
			continue;

		const newToggles = []

		arr.push(<ListItem
			key={file}
			primaryText={file}
			secondaryText={formatBytes(tree[file].__sizeBT)}
			nestedItems={treeToTorrentFiles(tree[file], torrent, newToggles)}
			primaryTogglesNestedList={true}
			innerDivStyle={{wordBreak: 'break-word'}}
			leftIcon={!tree[file].__fileBT ? <FileFolder /> : contentIcon(fileTypeDetect({path: file}))}
			rightToggle={
				newToggles.length > 0
					?
					<Toggle
						toggled={newToggles.every( ({selected}) => selected )}
						onToggle={(e, checked) => {
							e.preventDefault()
							e.stopPropagation()
							let toggleValues = {}
							newToggles.forEach(({downloadIndex}) => toggleValues[downloadIndex] = checked)
							window.torrentSocket.emit('downloadSelectFiles', {hash: torrent.hash, files: toggleValues})
						}}
					/>
					:
					null
			}
		/>);

		if(toggles)
		{
			for(const newToggle of newToggles)
				toggles.push(newToggle)
		}
	}
	return arr;
}

const TorrentFiles = (props) => {
	let filesList = props.torrent.filesList;
	let tree = buildFilesTree(filesList);
	return (
		<List className='w100p'>
			{
				filesList.length > 0
					?
					<div className='w100p'>
						<Subheader inset={true}>{__('Content of the torrent')}:</Subheader>
						{treeToTorrentFiles(tree, {hash: props.torrent.hash})}
					</div>
					:
					<div className='column center'>
						<span className='pad0-75'>{__('Processing files')}...</span>
						<LinearProgress mode="indeterminate" />
					</div>
			}
		</List>
	);
};

const TorrentInformation = (props) => {
	let torrent = props.torrent;
	return (
		<List className='w100p'>
			<Subheader inset={true}>{__('Information about torrent')}</Subheader>
			<ListItem
				//leftAvatar={<Avatar icon={<ActionAssignment />} backgroundColor={blue500} />}
				rightIcon={<ActionInfo />}
				primaryText={__('Torrent Name')}
				secondaryText={<span className='break-word' style={{whiteSpace: 'normal'}}>{(torrent.info && torrent.info.name) || torrent.name}</span>}
			/>
			<ListItem
				// leftAvatar={<Avatar icon={<EditorInsertChart />} backgroundColor={yellow600} />}
				rightIcon={<ActionInfo />}
				id="torrentSizeId"
				primaryText={__('Torrent Size')}
				secondaryText={formatBytes(torrent.size)}
				onClick={() => {
					if(!props.parent)
						return

					props.parent.setState({
						value: 'files'
					})
				}}
			/>
			<ListItem
				// leftAvatar={<Avatar icon={<EditorInsertChart />} backgroundColor={yellow600} />}
				rightIcon={<ActionInfo />}
				primaryText={__('Torrent contains files')}
				id="torrentFilesId"
				secondaryText={torrent.files}
				onClick={() => {
					if(!props.parent)
						return

					props.parent.setState({
						value: 'files'
					})
				}}
			/>
			<ListItem
				// leftAvatar={<Avatar icon={<EditorInsertChart />} backgroundColor={yellow600} />}
				rightIcon={<ActionInfo />}
				primaryText={__('Indexed/Added torrent date')}
				secondaryText={moment(torrent.added * 1000).format('MMMM Do YYYY, hh:mm')}
			/>
			<ListItem
				// leftAvatar={<Avatar icon={<EditorInsertChart />} backgroundColor={yellow600} />}
				rightIcon={<ActionInfo />}
				primaryText={__('Content type')}
				secondaryText={torrent.contentType || 'unknown'}
			/>
			<ListItem
				// leftAvatar={<Avatar icon={<EditorInsertChart />} backgroundColor={yellow600} />}
				rightIcon={<ActionInfo />}
				primaryText={__('Category')}
				secondaryText={(torrent.info && torrent.info.contentCategory) || torrent.contentCategory || 'unknown'}
			/>
		</List>
	);
}

export default class TorrentPage extends Page {
	constructor(props) {
		super(props);
		this.state = {
			value: 'info',
			searchingIndicator: false,
			voting: false,
			voted: false,
			downloading: false,
			downloaded: false,
			startingDownloading: false,
			downloadProgress: {}
		};
		this.setTitle('Information about torrent');
	}

	changeTab(tab) {
		if(this.state.value != tab) {
			this.setState({
				value: tab
			});
			console.log('change');
		}
	}
	onSwipeRight() {
		this.changeTab('files');
	}
	onSwipeLeft() {
		this.changeTab('info');
	}

  handleChange = (value) => {
  	if(value == 'main') {
  		window.routerOpenPrev();
  		return;
  	}

  	this.setState({
  		value: value,
  	});
  };
  getTorrentInfo() {
  	window.torrentSocket.emit('torrent', {hash: this.props.hash, options: {files: true, peer: this.props.peer}}, window.customLoader((data) => {
  		if(data) {
  			this.torrent = data
  			this.setTitle(this.torrent.name + ' - Rats On The Boat');
  			if(this.torrent.contentCategory == 'xxx') {
  				this.setMetaTag('robots', 'noindex');
  			}
			this.torrent.filesTypesPriority = torrentTypeDetect(this.torrent, this.torrent.filesList);
			//this.forceUpdate(); // вызывается через searchingIndicator

  			// Получаем более новую статистику пира
  			if((Date.now() / 1000) - this.torrent.trackersChecked > 10 * 60) {
  				window.torrentSocket.emit('checkTrackers', {hash: this.torrent.hash});
  			}
  		}
  	}, () => {
  		this.setState({
  			searchingIndicator: true
  		});
  	}, () => {
  		this.setState({
  			searchingIndicator: false
  		});
  	}));
  }
  componentDidMount() {
  	super.componentDidMount();

  	this.filesUpdated = (hash, filesList) => {
  		if(this.props.hash != hash)
  			return;

  		if(filesList)
  		{
  			if(this.torrent)
  			{
  				this.torrent.filesList = filesList
				this.torrent.filesTypesPriority = torrentTypeDetect(this.torrent, this.torrent.filesList);
  				this.forceUpdate()
  			}
  		}
  		else
  			this.getTorrentInfo();
  	}
  	window.torrentSocket.on('filesReady', this.filesUpdated);

  	this.trackerUpdate = (info) => {
  		if(this.props.hash != info.hash)
  			return;

  		if(!this.torrent)
  			return;

  		this.torrent = Object.assign(this.torrent, info);
  		this.forceUpdate();
  	}
  	window.torrentSocket.on('trackerTorrentUpdate', this.trackerUpdate);

  	this.onVotes = async ({hash, good, bad, selfVote}) => {
  		if(this.props.hash != hash)
  			return;

  		if(!this.torrent)
  			return

  		this.torrent.good = good;
  		this.torrent.bad = bad;
  		this.state.voted = selfVote;
  		this.forceUpdate();
  	}
  	window.torrentSocket.on('votes', this.onVotes);

  	this.downloading = (hash) => {
  		if(this.props.hash != hash)
  			return;

  		this.setState({
  			downloading: true,
  			startingDownloading: false
  		})
  	}
  	window.torrentSocket.on('downloading', this.downloading);
    
  	this.downloadDone = (hash, canceled) => {
  		if(this.props.hash != hash)
  			return;

  		this.setState({
  			downloading: false,
  			startingDownloading: false,
  			downloaded: !canceled
  		})
  	}
  	window.torrentSocket.on('downloadDone', this.downloadDone);

  	this.downloadProgress = (hash, progress) => {
  		if(this.props.hash != hash)
  			return;

  		this.setState({
  			downloading: true,
  			startingDownloading: false,
  			downloadProgress: progress
  		})
  	}
  	window.torrentSocket.on('downloadProgress', this.downloadProgress);

  	this.getTorrentInfo();
  }
  componentWillUnmount() {
  	if(this.filesUpdated)
  		window.torrentSocket.off('filesReady', this.filesUpdated);
  	if(this.trackerUpdate)
  		window.torrentSocket.off('trackerTorrentUpdate', this.trackerUpdate);
  	if(this.onVotes)
  		window.torrentSocket.off('votes', this.onVotes);
  	if(this.torrent && this.torrent.contentCategory == 'xxx') {
  		this.removeMetaTag('robots');
  	}
  	if(this.downloading)
  		window.torrentSocket.off('downloading', this.downloading);
  	if(this.downloadDone)
  		window.torrentSocket.off('downloadDone', this.downloadDone);
  	if(this.downloadProgress)
  		window.torrentSocket.off('downloadProgress', this.downloadProgress);
  }
  vote(good) {
  	if(!this.torrent)
  		return;

  	this.setState({
  		voting: true
  	});
  	window.torrentSocket.emit('vote', {hash: this.torrent.hash, isGood: !!good}, window.customLoader((success) => {
  		this.setState({
  			voted: true,
  			voting: false
  		});
  	}));
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
  			<div className='pad1 w100p column center'>
  				<div className='row center pad0-75'>
  					<RaisedButton label={__('Back to previus')} primary={true} onClick={() => {
  						window.routerOpenPrev();
  					}} />
  				</div>
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

  	let torrentRating;
  	if(this.torrent) {
  		torrentRating = Math.round(rating(this.torrent.good, this.torrent.bad) * 100);
  	}

  	return (
  		<div className="w100p column center">
  			{
  				this.torrent
  					?
  					<Tabs
  						className='w100p'
  						value={this.state.value}
  						onChange={this.handleChange}
  					>
  						<Tab label={__('Back to previus')} value="main" />
  						<Tab label={__('Information')} value="info" >
  							<div className='column w100p'>
  								<div className='row w100p torrent-information-row'>
  									<div className='info-table'>
  										<TorrentInformation torrent={this.torrent} parent={this} />
  									</div>
  									<div style={{flexBasis: '40%'}} className='info-table-right column center w100p'>
  										<img src={(this.torrent && this.torrent.info && this.torrent.info.poster) ? this.torrent.info.poster : NoImage} className='pad0-75' style={{height: '200px'}} />
  										<TrackersImages info={this.torrent && this.torrent.info} className='column' />
										  {
										   this.torrent.filesTypesPriority && Object.keys(this.torrent.filesTypesPriority).length > 0 && Object.keys(this.torrent.filesTypesPriority).filter((key) => this.torrent.filesTypesPriority[key] > 0.0002).length > 1
										   &&
										  <PieChart
										    style={{height: 128, margin: 5}}
											data={
												Object.keys(this.torrent.filesTypesPriority).map(type => ({
													title: type,
													value: this.torrent.filesTypesPriority[type],
													color: niceTypeColor(type)
												}))
											}
											label={({dataEntry}) => dataEntry.title }
											labelStyle={{
												fontSize: '0.55em',
												fill: '#fff'
											}}
											/>
  										}
  										<RaisedButton
  											href={`magnet:?xt=urn:btih:${this.torrent.hash}`}
  											target="_self"
  											label={__('Magnet')}
  											secondary={true}
  											onClick={(e) => {
  												e.preventDefault();
  												window.open(`magnet:?xt=urn:btih:${this.torrent.hash}`, '_self')
  											}}
  											icon={<svg fill='white' viewBox="0 0 24 24"><path d="M17.374 20.235c2.444-2.981 6.626-8.157 6.626-8.157l-3.846-3.092s-2.857 3.523-6.571 8.097c-4.312 5.312-11.881-2.41-6.671-6.671 4.561-3.729 8.097-6.57 8.097-6.57l-3.092-3.842s-5.173 4.181-8.157 6.621c-2.662 2.175-3.76 4.749-3.76 7.24 0 5.254 4.867 10.139 10.121 10.139 2.487 0 5.064-1.095 7.253-3.765zm4.724-7.953l-1.699 2.111-1.74-1.397 1.701-2.114 1.738 1.4zm-10.386-10.385l1.4 1.738-2.113 1.701-1.397-1.74 2.11-1.699z"/></svg>}
  										/>
  										{
  											!this.state.downloaded && !this.state.downloading && !this.state.startingDownloading
                      &&
                      <DownloadTorrentMenu torrent={this.torrent}>
                      	<RaisedButton
                      		href={`magnet:?xt=urn:btih:${this.torrent.hash}`}
                      		target="_self"
                      		label={__('Download')}
                      		backgroundColor='#00C853'
                      		labelColor='white'
                      		style={{marginTop: 8}}
                      		onClick={(e) => {
                      			e.preventDefault();
                      		}}
                      		icon={
                      			<svg viewBox="0 0 56 56" fill='white' id="downloadSvg">
                      				<g>
                      					<path d="M35.586,41.586L31,46.172V28c0-1.104-0.896-2-2-2s-2,0.896-2,2v18.172l-4.586-4.586c-0.781-0.781-2.047-0.781-2.828,0
                                s-0.781,2.047,0,2.828l7.999,7.999c0.093,0.094,0.196,0.177,0.307,0.251c0.047,0.032,0.099,0.053,0.148,0.081
                                c0.065,0.036,0.127,0.075,0.196,0.103c0.065,0.027,0.133,0.042,0.2,0.062c0.058,0.017,0.113,0.04,0.173,0.051
                                C28.738,52.986,28.869,53,29,53s0.262-0.014,0.392-0.04c0.06-0.012,0.115-0.034,0.173-0.051c0.067-0.02,0.135-0.035,0.2-0.062
                                c0.069-0.028,0.131-0.067,0.196-0.103c0.05-0.027,0.101-0.049,0.148-0.081c0.11-0.074,0.213-0.157,0.307-0.251l7.999-7.999
                                c0.781-0.781,0.781-2.047,0-2.828S36.367,40.805,35.586,41.586z"/>
                      					<path d="M47.835,18.986c-0.137-0.019-2.457-0.335-4.684,0.002C43.1,18.996,43.049,19,42.999,19c-0.486,0-0.912-0.354-0.987-0.85
                                c-0.083-0.546,0.292-1.056,0.838-1.139c1.531-0.233,3.062-0.196,4.083-0.124C46.262,9.135,39.83,3,32.085,3
                                C27.388,3,22.667,5.379,19.8,9.129C21.754,10.781,23,13.246,23,16c0,0.553-0.447,1-1,1s-1-0.447-1-1
                                c0-2.462-1.281-4.627-3.209-5.876c-0.227-0.147-0.462-0.277-0.702-0.396c-0.069-0.034-0.139-0.069-0.21-0.101
                                c-0.272-0.124-0.55-0.234-0.835-0.321c-0.035-0.01-0.071-0.017-0.106-0.027c-0.259-0.075-0.522-0.132-0.789-0.177
                                c-0.078-0.013-0.155-0.025-0.233-0.036C14.614,9.027,14.309,9,14,9c-3.859,0-7,3.141-7,7c0,0.082,0.006,0.163,0.012,0.244
                                l0.012,0.21l-0.009,0.16C7.008,16.744,7,16.873,7,17v0.63l-0.567,0.271C2.705,19.688,0,24,0,28.154C0,34.135,4.865,39,10.845,39H25
                                V28c0-2.209,1.791-4,4-4s4,1.791,4,4v11h2.353c0.059,0,0.116-0.005,0.174-0.009l0.198-0.011l0.271,0.011
                                C36.053,38.995,36.11,39,36.169,39h9.803C51.501,39,56,34.501,56,28.972C56,24.161,52.49,19.872,47.835,18.986z"/>
                      				</g>
                      			</svg>
                      		}
                      	/>
                      </DownloadTorrentMenu>
  										}
  										{
  											this.state.downloading
                      &&
                      <div className='column center pad0-75' style={{width: '300px'}}>
                      	<div className='fs0-75' style={{color: 'rgb(0, 188, 212)'}}>{__('downloading')} {this.state.downloadProgress && (this.state.downloadProgress.progress * 100).toFixed(1)}%</div>
                      	<LinearProgress 
                      		style={{marginTop: 3}}
                      		mode="determinate" 
                      		value={this.state.downloadProgress && (this.state.downloadProgress.progress ? this.state.downloadProgress.progress : 0) * 100}
                      	/>
                      	<FlatButton
                      		onClick={() => {
                      			window.torrentSocket.emit('downloadCancel', {hash: this.torrent.hash})
                      		}}
                      		label={__('Cancel download')}
                      		secondary={true}
                      		icon={<svg fill='rgb(255, 64, 129)' viewBox="0 0 18 18"><path d="M9 1C4.58 1 1 4.58 1 9s3.58 8 8 8 8-3.58 8-8-3.58-8-8-8zm4 10.87L11.87 13 9 10.13 6.13 13 5 11.87 7.87 9 5 6.13 6.13 5 9 7.87 11.87 5 13 6.13 10.13 9 13 11.87z"/></svg>}
                      	/>
                      </div>
  										}
  										<div className='fs0-75 pad0-75 center column' style={{color: 'rgba(0, 0, 0, 0.541176)'}}><div>BTIH:</div><div>{this.torrent.hash}</div></div>
  										{
  											this.torrent.seeders || this.torrent.leechers || this.torrent.completed
  												?
  												<div className='fs0-85 pad0-75 center column'>
  													<div className='pad0-25' style={{color: '#00C853'}}>{__('seeders')}: {this.torrent.seeders}</div>
  													<div className='pad0-25' style={{color: '#AA00FF'}}>{__('leechers')}: {this.torrent.leechers}</div>
  													<div className='pad0-25' style={{color: '#FF6D00'}}>{__('completed')}: {this.torrent.completed}</div>
  												</div>
  												:
  												null
  										}
  										{
  											!this.state.voted && !this.state.voting
  												?
  												<div className='row pad0-25'>
  													<RaisedButton
  														label={__('Good') + ` (${this.torrent.good})`}
  														labelColor="white"
  														backgroundColor="#00C853"
  														icon={
  															<svg viewBox="0 0 489.543 489.543" fill="white">
  																<g>
  																	<path d="M270.024,0c-22.6,0-15,48.3-15,48.3s-48.3,133.2-94.5,168.7c-9.9,10.4-16.1,21.9-20,31.3l0,0l0,0
                                c-0.9,2.3-1.7,4.5-2.4,6.5c-3.1,6.3-9.7,16-23.8,24.5l46.2,200.9c0,0,71.5,9.3,143.2,7.8c28.7,2.3,59.1,2.5,83.3-2.7
                                c82.2-17.5,61.6-74.8,61.6-74.8c44.3-33.3,19.1-74.9,19.1-74.9c39.4-41.1,0.7-75.6,0.7-75.6s21.3-33.2-6.2-58.3
                                c-34.3-31.4-127.4-10.5-127.4-10.5l0,0c-6.5,1.1-13.4,2.5-20.8,4.3c0,0-32.2,15,0-82.7C346.324,15.1,292.624,0,270.024,0z"/>
  																	<path d="M127.324,465.7l-35-166.3c-2-9.5-11.6-17.3-21.3-17.3h-66.8l-0.1,200.8h109.1C123.024,483,129.324,475.2,127.324,465.7z"
  																	/>
  																</g>
  															</svg>
  														}
  														onClick={() => this.vote(true)}
  													/>
  													<RaisedButton
  														style={{marginLeft: '9px'}}
  														label={__('Bad') + ` (${this.torrent.bad})`}
  														labelColor="white"
  														backgroundColor="#D50000"
  														icon={
  															<svg viewBox="0 0 487.643 487.643" fill="white">
  																<g>
  																	<path d="M113.869,209.443l46-200.1c0,0,71.2-9.3,142.6-7.8c28.5-2.3,58.9-2.5,83,2.7c81.9,17.4,61.4,74.5,61.4,74.5
                              c44.2,33.2,19,74.6,19,74.6c39.2,41,0.7,75.3,0.7,75.3s21.2,33-6.1,58c-34.2,31.2-126.9,10.5-126.9,10.5l0,0
                              c-6.4-1.1-13.3-2.5-20.7-4.2c0,0-32.1-15,0,82.4s-21.4,112.3-43.9,112.3s-15-48.1-15-48.1s-48.1-132.7-94.1-168
                              c-9.9-10.4-16.1-21.8-19.9-31.2l0,0l0,0c-0.9-2.3-1.7-4.5-2.4-6.5C134.469,227.543,127.869,217.843,113.869,209.443z
                               M70.869,206.643c9.7,0,19.2-7.7,21.2-17.2l34.8-165.6c2-9.5-4.3-17.2-14-17.2H4.169l0.1,200H70.869z"/>
  																</g>
  															</svg>
  														}
  														onClick={() => this.vote(false)}
  													/>
  												</div>
  												:
  												this.state.voting
  													?
  													<div>voting...</div>
  													:
  													<div>Thank you for voting!</div>
  										}
  										{
  											this.torrent.good > 0 || this.torrent.bad > 0
  												?
  												<div className='w100p' style={{padding: '7px 35px', marginTop: '10px'}}>
  													<LinearProgress 
  														mode="determinate" 
  														value={torrentRating}
  														color={torrentRating >= 50 ? '#00E676' : '#FF3D00'}
  														style={{
  															height: '5px',
  														}}
  													/>
  													<div className='row center pad0-75 fs0-85' style={{color: torrentRating >= 50 ? '#00E676' : '#FF3D00'}}>{__('Torrent rating')}: {torrentRating}%</div>
  												</div>
  												:
  												null
  										}
  									</div>
  								</div>
  								{
  									this.torrent && this.torrent.info && this.torrent.info.description
                                    &&
                                    <div className='fs0-85' style={{width: '95%', padding: 15, margin: 20, boxShadow: 'rgba(0, 0, 0, 0.12) 0px 1px 6px, rgba(0, 0, 0, 0.12) 0px 1px 4px'}}>
                                    	<div>{parseDescriptionText(this.torrent.info.description)}</div>
                                    </div>
  								}
  							</div>
  						</Tab>
  						<Tab label={__('Files')} value="files" >
  							<TorrentFiles torrent={this.torrent} />
  						</Tab>
  					</Tabs>
  					:
  					null
  			}
  		</div>
  	);
  }
}
