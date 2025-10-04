import React, { Component } from 'react';

import AdvancedSearch from './search-advanced-controls'
import TorrentsStatistic from './torrent-statistic'
import Tooltip from './tooltip'

import TextField from 'material-ui/TextField';
import RaisedButton from 'material-ui/RaisedButton';
import RefreshIndicator from 'material-ui/RefreshIndicator';

import Checkbox from 'material-ui/Checkbox';
import Visibility from 'material-ui/svg-icons/action/visibility';
import VisibilityOff from 'material-ui/svg-icons/action/visibility-off';
import AddIcon from 'material-ui/svg-icons/content/add';
import RemoveIcon from 'material-ui/svg-icons/content/remove';

import _ from 'lodash'
import singleton from './singleton';

class Search extends Component {
	constructor(props)
	{
		super(props)
		this.onSearchUpdate = () => {}

		this.state = { 
			searchingIndicator: false,
			safeSearchText: __('safe search enabled'),
			safeSearchColor: 'rgb(0, 188, 212)',
			moreTorrentsIndicator: false,
			moreFilesIndicator: false,
			orderBy: null,
			orderDesc: false,
			advancedSearch: false,
		}
		this.searchLimit = 10
		this.advanced = {}
		this.searchError = undefined;
	}

	search(oldSearch) {
		window.router('/search')
		this.setState({
			searchingIndicator: true
		});
		this.onSearchUpdate('indicator')
    
		this.searchTorrents = [];
		this.moreSearchTorrents = true;
		this.searchFiles = [];
		this.moreSearchFiles = true;
		this.currentSearch = this.searchValue;
		let queries = 2;
		let searchTorrentsParams = {
			limit: this.searchLimit, 
			safeSearch: !this.notSafeSearch,
			orderBy: this.state.orderBy,
			orderDesc: this.state.orderDesc,
		};
		if(this.state.advancedSearch && this.advanced)
			searchTorrentsParams = Object.assign(searchTorrentsParams, this.advanced);

		this.searchTorrentId = window.torrentSocket.emit('searchTorrent', {text: oldSearch ? this.currentSearch : this.searchValue, navigation: searchTorrentsParams}, window.customLoader((torrents) => {
			if(torrents) {
				this.searchTorrents = torrents;
				if(torrents.length != this.searchLimit)
					this.moreSearchTorrents = false;
			}
			else
			{
				this.moreSearchTorrents = false;
			}
			if(--queries == 0) {
				this.setState({
					searchingIndicator: false
				});
			}
			this.onSearchUpdate('torrents')
		}));
		let searchFilesParams = {
			limit: this.searchLimit, 
			safeSearch: !this.notSafeSearch,
			orderBy: this.state.orderBy,
			orderDesc: this.state.orderDesc,
		};
		if(this.state.advancedSearch && this.advanced)
			searchFilesParams = Object.assign(searchFilesParams, this.advanced);
    
		this.searchFilesId = window.torrentSocket.emit('searchFiles', {text: oldSearch ? this.currentSearch : this.searchValue, navigation: searchFilesParams}, window.customLoader((torrents) => {
			if(torrents) {
				this.searchFiles = torrents;
				let files = 0;
				torrents.forEach((torrent) => {
					if(torrent.path && torrent.path.length > 0)
						files += torrent.path.length
				});
				if(files != this.searchLimit)
					this.moreSearchFiles = false;
			}
			else
			{
				this.moreSearchFiles = false;
			}
			if(--queries == 0) {
				this.setState({
					searchingIndicator: false
				});
			}
			this.onSearchUpdate('files')
		}));
	}
	moreTorrents() {
		this.setState({moreTorrentsIndicator: true});
		this.onSearchUpdate('indicator')

		this.searchTorrentId = window.torrentSocket.emit('searchTorrent', {text: this.currentSearch, navigation: {
			index: this.searchTorrents.length,
			limit: this.searchLimit, 
			safeSearch: !this.notSafeSearch,
			orderBy: this.state.orderBy,
			orderDesc: this.state.orderDesc,
		}}, window.customLoader((torrents) => {
			if(torrents) {
				this.searchTorrents = this.searchTorrents.concat(torrents);
				if(torrents.length != this.searchLimit)
					this.moreSearchTorrents = false;

				this.setState({moreTorrentsIndicator: false});
				this.onSearchUpdate('more torrents')
			}
		}));
	}
	calcTorrentsFiles(torrents)
	{
		let files = 0;
		torrents.forEach((torrent) => {
			if(torrent.path && torrent.path.length > 0)
				files += torrent.path.length
		});
		return files
	}
	moreFiles() {
		let index = 0;
		this.searchFiles.forEach((torrent) => {
			if(torrent.path && torrent.path.length > 0)
				index += torrent.path.length;
		});

		this.setState({moreFilesIndicator: true});
		this.onSearchUpdate('indicator')

		this.searchFilesId = window.torrentSocket.emit('searchFiles', {text: this.currentSearch, navigation: {
			index: index,
			limit: this.searchLimit, 
			safeSearch: !this.notSafeSearch,
			orderBy: this.state.orderBy,
			orderDesc: this.state.orderDesc,
		}}, window.customLoader((torrents) => {
			if(torrents) {
				this.searchFiles = this.searchFiles.concat(torrents);

				if(this.calcTorrentsFiles(torrents) != this.searchLimit)
					this.moreSearchFiles = false;

				this.mergeFiles()

				this.setState({moreFilesIndicator: false});
				this.onSearchUpdate('more files')
			}
		}));
	}
	mergeFiles()
	{
		for(let i = 0; i < this.searchFiles.length; i++)
		{
			for(let j = i + 1; j < this.searchFiles.length; j++)
			{
				if(this.searchFiles[i].hash != this.searchFiles[j].hash)
					continue

				if(!this.searchFiles[i].remove)
				{
					this.searchFiles[i].path = this.searchFiles[i].path.concat(this.searchFiles[j].path)
				}
        
				this.searchFiles[j].remove = true
			}
		}

		this.searchFiles = this.searchFiles.filter(torrent => !torrent.remove)
	}

	componentDidMount() {
		this.newStatisticFunc = (statistic) => {
			if(statistic) {
				this.stats = statistic;
				this.forceUpdate();
			}
		};
		window.torrentSocket.emit('statistic', window.customLoader(this.newStatisticFunc));
		window.torrentSocket.on('newStatistic', this.newStatisticFunc);

		this.remoteSearchTorrent = (torrents) => {
			if(!torrents || !torrents.torrents)
				return
	  
			if (this.searchTorrentId != torrents.id)
				return

			torrents = torrents.torrents

			if(torrents.length === this.searchLimit)
				this.moreSearchTorrents = true;

			this.searchTorrents = _.unionBy(this.searchTorrents, torrents, 'hash')
			this.onSearchUpdate('remote torrents')
		}
		window.torrentSocket.on('remoteSearchTorrent', this.remoteSearchTorrent);

		this.remoteSearchFiles = (torrents) => {
			if(!torrents || !torrents.torrents)
				return
			
			if (this.searchFilesId != torrents.id)
				return

			torrents = torrents.torrents

			if(torrents.length > 0 && this.calcTorrentsFiles(torrents) === this.searchLimit)
				this.moreSearchFiles = true;

			this.searchFiles = _.unionBy(this.searchFiles, torrents, 'hash')
			this.mergeFiles()
			this.onSearchUpdate('remote files')
		}
		window.torrentSocket.on('remoteSearchFiles', this.remoteSearchFiles);
	}
	componentWillUnmount()
	{
		if(this.newStatisticFunc)
			window.torrentSocket.off('newStatistic', this.newStatisticFunc);

		if(this.remoteSearchTorrent)
			window.torrentSocket.off('remoteSearchTorrent', this.remoteSearchTorrent);

		if(this.remoteSearchFiles)
			window.torrentSocket.off('remoteSearchFiles', this.remoteSearchFiles);
	}
	setSafeSearch(ch) {
		this.notSafeSearch = ch;
		if(ch)
		{
			return {safeSearchText: __('safe search disabled'), safeSearchColor: '#EC407A'}
		}
		else
		{
			return {safeSearchText: __('safe search enabled'), safeSearchColor: 'rgb(0, 188, 212)'}
		}
	}
	render() {
		const style = {
			refresh: {
				display: 'inline-block',
				position: 'relative',
			},
		};

		return (
			<div className="column w100p center" style={{position: 'relative'}}>
				<div className='row inline w100p pad0-75 search-row' style={{minWidth: '35em', backgroundColor: 'white', paddingTop: 0, paddingBottom: this.searchError ? 17 : 0, margin: 5, borderRadius: 3}}>
					{
						((this.searchTorrents && this.searchTorrents.length > 0) || (this.searchFiles && this.searchFiles.length > 0))
                        &&
                        <div style={{width: 25, height: 25, margin: 2, marginRight: 8}}>
                        	<Tooltip hint={__('display search results for current search request')}>
                        		<Checkbox
                        			id='open-recent-search'
                        			checked={false}
                        			uncheckedIcon={<svg viewBox="0 0 459 459">
                        				<g>
                        					<path d="M178.5,382.5h102v-51h-102V382.5z M0,76.5v51h459v-51H0z M76.5,255h306v-51h-306V255z"/>
                        				</g>
                        			</svg>
                        			}
                        			iconStyle={{fill: '#27ce74'}}
                        			onCheck={() => {
                        				if(window.routerCurrent() !== '/search')
                        					window.router('/search')
                        			}}
                        			style={{paddingTop: '0.6em', paddingLeft: '0.2em'}}
                        		/>
                        	</Tooltip>
                        </div>
					}
					<TextField
						style={{marginTop: -12}}
						hintText={__('Search torrent or file') + ' ' + __('or with hash')}
						floatingLabelText={__('What to search?')}
						fullWidth={true}
						ref='searchInput'
						id='searchInput'
						defaultValue={this.searchValue}
						errorText={this.searchError}
						onKeyPress={(e) => {
							if (e.key === 'Enter') {
								this.search();
							}
						}}
						onChange={e => {
							this.searchValue = e.target.value
							if(this.searchValue.length < 3 && this.searchValue.length > 0)
								this.searchError = __('too short string for search');
							else
								this.searchError = undefined;
							this.forceUpdate()
						}}
					/>

					<div style={{width: 25, height: 25, margin: 2}}>
						<Tooltip hint={__('enable/disable safe search (adult filter)')}>
							<Checkbox
								ref='safeSearch'
								checked={this.notSafeSearch ? true : false}
								checkedIcon={<Visibility />}
								uncheckedIcon={<VisibilityOff />}
								iconStyle={{fill: this.state.safeSearchColor}}
								onCheck={(ev, ch) => {
									this.setState(this.setSafeSearch(ch));
								}}
								style={{paddingBottom: '0.8em'}}
							/>
						</Tooltip>
					</div>
					<div style={{width: 25, height: 25, margin: 2}}>
						<Tooltip hint={__('advanced search')}>
							<Checkbox
								ref='advancedSearch'
								checked={this.state.advancedSearch}
								checkedIcon={<RemoveIcon />}
								uncheckedIcon={<AddIcon />}
								iconStyle={{fill: 'black'}}
								onCheck={(ev, ch) => {
									this.setState({advancedSearch: ch});
								}}
								style={{paddingBottom: '0.8em'}}
							/>
						</Tooltip>
					</div>

					<RaisedButton style={{marginLeft: '10px'}} id='search' label={__('Search')} primary={true} onClick={() =>{
						this.search()
					}} />
				</div>
				{
					this.state.advancedSearch
          &&
            <AdvancedSearch onChange={(state) => {
            	this.advanced = state;
            }} state={this.advanced} />

				}
				{
					this.stats
            &&
            <TorrentsStatistic stats={this.stats} />
				}
				{
					this.state.searchingIndicator
						?
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
						:
						null
				}
			</div>
		);
	}
}

export default singleton(Search)