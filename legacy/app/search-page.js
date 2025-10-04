import React from 'react';
import Page from './page';

import SearchResults from './search-results'
import Search from './search'
import SelectField from 'material-ui/SelectField';
import MenuItem from 'material-ui/MenuItem';

import _ from 'lodash'

export default class SearchPage extends Page {
	constructor(props) {
		super(props)
		this.setTitle('Rats On The Boat - Content Search Engine');
	}
	componentDidMount()
	{
		Search.instance().onSearchUpdate = () => this.forceUpdate()
		window.torrentSocket.emit('downloads', (downloads) => {
			for(const torrent of downloads) {
				const hash = torrent.torrentObject.hash;
				delete torrent.torrentObject;
				let needUpdate = false;
				const updateValues = (target) => {
					let index = _.findIndex(target, {hash});
					if(index >= 0) {
						target[index].download = torrent
						needUpdate = true;
					}
				}
				updateValues(Search.instance().searchTorrents);
				updateValues(Search.instance().searchFiles);
				if (needUpdate)
					this.forceUpdate()
			}
		})
	}
	componentWillUnmount()
	{
		Search.instance().onSearchUpdate = () => {}
		for(const torrent of Search.instance().searchTorrents)
			delete torrent.download
		for(const torrent of Search.instance().searchFiles)
			delete torrent.download
	}
	render() {
		const orderText = (text, field) => {
			if(field !== Search.instance().state.orderBy)
				return text;

			if(Search.instance().state.orderDesc)
				return text + ' ⇩'
			else
				return text + ' ⇧'
		}

		return (
			<div className='column center'>
				<div className='torrents-container search-list'>
					<SearchResults 
						torrentsSearchResults={Search.instance().searchTorrents} 
						filesSearchResults={Search.instance().searchFiles}
						currentSearching={Search.instance().state.searchingIndicator}
						searchText={Search.instance().currentSearch}
						resultSelector={
							<SelectField
								floatingLabelText={__("Sort by")}
								floatingLabelFixed={true}
								value={Search.instance().state.orderBy}
								onChange={(event, index, value) => {
									event.preventDefault(); // fix overclick on torrent
									if(value === 'none') {
										Search.instance().setState({orderBy: null}, () => {
											Search.instance().search(true)
										})
										return;
									}

									if(value === Search.instance().state.orderBy)
									{
										Search.instance().setState({orderDesc: !Search.instance().state.orderDesc}, () => {
											Search.instance().search(true)
										})
										return;
									}

									Search.instance().setState({
										orderBy: value, 
										orderDesc: (value === 'seeders' || value === 'completed' || value === 'added') ? true : Search.instance().state.orderDesc
									}, () => {
										Search.instance().search(true)
									})
								}}
							>
								<MenuItem value='none' primaryText={__('None')} />
								<MenuItem value='seeders' primaryText={orderText(__('Seeders'), 'seeders')} />
								<MenuItem value='name' primaryText={orderText(__('Name'), 'name')} />
								<MenuItem value='files' primaryText={orderText(__('Files'), 'files')} />
								<MenuItem value='size' primaryText={orderText(__('Size'), 'size')} />
								<MenuItem value='added' primaryText={orderText(__('Added date'), 'added')} />
								<MenuItem value='completed' primaryText={orderText(__('Completed'), 'completed')} />
							</SelectField>
						}

						moreTorrentsEnabled={Search.instance().moreSearchTorrents && !Search.instance().state.searchingIndicator}
						moreFilesEnabled={Search.instance().moreSearchFiles && !Search.instance().state.searchingIndicator}
						onMoreTorrents={() => Search.instance().moreTorrents()}
						onMoreFiles={() => Search.instance().moreFiles()}
						moreTorrentsIndicator={Search.instance().state.moreTorrentsIndicator}
						moreFilesIndicator={Search.instance().state.moreFilesIndicator}
					/>
				</div>
			</div>
		);
	}
}
