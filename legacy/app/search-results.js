import React, { Component } from 'react';

import TorrentLine from './torrent'
import {List, ListItem} from 'material-ui/List';
import Subheader from 'material-ui/Subheader';
import Divider from 'material-ui/Divider';
import LinearProgress from './LinearProgress';

export default class SearchResults extends Component {
	render() {
		return (
			<List style={{minWidth: '20em'}}>
				{
					(this.props.torrentsSearchResults && this.props.torrentsSearchResults.length > 0)
            || (this.props.filesSearchResults && this.props.filesSearchResults.length > 0)
						?
						<div>
							<Subheader className='row center w100p' style={{paddingLeft: 0}} inset={true}><span>{__('Search results for')}</span> <span style={{marginLeft: '0.4em'}}><b>{this.props.searchText}</b></span></Subheader>
							<div className='w100p row center' style={{marginTop: '-16px'}}>{this.props.resultSelector}</div>
						</div>
						:
						null
				}
				{
					this.props.torrentsSearchResults && this.props.torrentsSearchResults.length > 0
						?
						this.props.torrentsSearchResults.map((torrent, index) =>{
							return(
								<TorrentLine torrent={torrent} key={index} />
							);
						})
						:
						null
				}
				{
					this.props.moreTorrentsEnabled && !this.props.moreTorrentsIndicator
						?
						<div>
							<ListItem innerDivStyle={{textAlign: 'center', padding: '1em'}} primaryText={<span>{__('More Torrents')}</span>} onClick={() => {
								if(this.props.onMoreTorrents)
									this.props.onMoreTorrents();
							}} />
							<Divider />
						</div>
						:
						null
				}
				{
					this.props.moreTorrentsIndicator
						?
						<div style={{padding: '0.8em'}}>
							<LinearProgress mode="indeterminate" />
						</div>
						:
						null
				}
				{
					this.props.filesSearchResults && this.props.filesSearchResults.length > 0
						?
						this.props.filesSearchResults.map((torrent, index) =>{
							return(
								<TorrentLine torrent={torrent} key={index} />
							);
						})
						:
						null
				}
				{
					this.props.moreFilesEnabled && !this.props.moreFilesIndicator
						?
						<div>
							<ListItem innerDivStyle={{textAlign: 'center', padding: '1em'}}  primaryText={__('More Files')} onClick={() => {
								if(this.props.onMoreFiles)
									this.props.onMoreFiles();
							}} />
							<Divider />
						</div>
						:
						null
				}
				{
					this.props.moreFilesIndicator
						?
						<div style={{padding: '0.8em'}}>
							<LinearProgress mode="indeterminate" />
						</div>
						:
						null
				}
				{
					this.props.torrentsSearchResults && this.props.torrentsSearchResults.length == 0
            && this.props.filesSearchResults && this.props.filesSearchResults.length == 0
            && !this.props.currentSearching
						?
						<div className='row inline center w100p pad0-75'>
							<svg style={{fill: 'grey', height: '30px'}} viewBox="0 0 264.695 264.695">
								<g>
									<path d="M219.171,216.785c-4.762,0-10.322,2.3-16.672,6.881l-35.211-12.968l35.734-12.978
                            c6.003,3.888,11.558,5.833,16.682,5.833c5.639,0,9.347-2.917,11.117-8.733c0.351-1.235,0.527-2.57,0.527-3.981
                            c0-7.397-4.766-11.378-14.295-11.9c3.876-3.882,5.828-7.687,5.828-11.392c0-3.871-2.039-7.149-6.092-9.797
                            c-2.118-1.049-4.325-1.584-6.615-1.584c-7.769,0-13.064,6.258-15.887,18.797l-61.941,23.039l-61.94-22.504
                            c-2.823-12.885-8.125-19.332-15.885-19.332c-2.293,0-4.501,0.535-6.62,1.584c-3.876,2.647-5.82,5.926-5.82,9.797
                            c0,3.705,1.944,7.51,5.82,11.392c-9.701,0.522-14.555,4.503-14.555,11.901c0,1.41,0.179,2.746,0.526,3.98
                            c1.946,5.816,5.651,8.733,11.122,8.733c5.113,0,10.671-1.945,16.677-5.832l35.998,12.977l-35.476,12.698
                            c-6.175-4.406-11.637-6.611-16.402-6.611c-5.654,0-9.623,2.918-11.919,8.733c-0.348,1.235-0.526,2.553-0.526,3.975
                            c0,7.405,4.853,11.385,14.555,11.907c-3.876,3.883-5.82,7.688-5.82,11.393c0,3.869,1.944,7.134,5.82,9.797
                            c2.477,1.412,4.854,2.105,7.153,2.105c7.227,0,12.443-6.176,15.619-18.525l61.673-22.504l61.678,22.504
                            c3.178,12.35,8.475,18.525,15.882,18.525c2.121,0,4.407-0.693,6.884-2.105c4.052-2.663,6.092-5.928,6.092-9.797
                            c0-3.705-1.953-7.51-5.828-11.393c9.528-0.522,14.295-4.502,14.295-11.907c0-1.422-0.177-2.739-0.527-3.975
                            C228.702,219.702,224.82,216.785,219.171,216.785z"/>
									<path d="M48.436,128.904c9.703,11.114,23.379,19.242,41.035,24.346v3.986c0,4.936,1.672,9.086,5.025,12.433
                            c3.35,3.358,7.498,5.211,12.441,5.563c5.116,0.357,8.905-0.528,11.378-2.646c3.879,2.817,8.204,4.229,12.974,4.229
                            c4.41,0,8.474-1.316,12.175-3.963c2.471,1.934,6.087,2.738,10.856,2.381c4.937-0.528,9.089-2.426,12.44-5.689
                            c3.35-3.281,5.025-7.371,5.025-12.307v-2.91c19.057-4.945,33.795-13.237,44.21-24.898c10.059-11.109,15.087-24.253,15.087-39.435
                            c0-3.359-0.355-6.886-1.063-10.597c-3.525-22.571-13.938-41.201-31.229-55.844C180.612,7.856,158.464,0,132.347,0
                            c-26.123,0-48.27,7.767-66.44,23.282C48.61,38.118,38.289,56.825,34.937,79.396c-0.709,3.711-1.064,7.238-1.064,10.597
                            C33.873,104.817,38.724,117.778,48.436,128.904L48.436,128.904z M152.865,60.749c5.206-6.085,11.514-9.13,18.922-9.13
                            c7.592,0,13.986,3.045,19.194,9.13c5.2,6.076,7.81,13.446,7.81,22.087c0,8.649-2.609,16.021-7.81,22.108
                            c-5.208,6.097-11.603,9.13-19.194,9.13c-7.408,0-13.716-3.033-18.922-9.13c-5.211-6.087-7.814-13.459-7.814-22.108
                            C145.05,74.195,147.654,66.825,152.865,60.749z M124.805,121.428c2.556-3.307,5.065-4.968,7.542-4.968
                            c2.47,0,4.802,1.831,7.012,5.509c2.205,3.662,3.317,7.145,3.317,10.469c0,5.062-3.361,7.581-10.067,7.581
                            c-4.414,0-7.677-1.136-9.792-3.396c-1.237-1.411-1.849-3.147-1.849-5.249C120.969,128.065,122.245,124.752,124.805,121.428z
                             M71.465,60.749c5.295-6.085,11.65-9.13,19.059-9.13c7.406,0,13.762,3.045,19.06,9.13c5.296,6.076,7.948,13.446,7.948,22.087
                            c0,8.649-2.651,16.021-7.948,22.108c-5.297,6.097-11.654,9.13-19.06,9.13c-7.409,0-13.764-3.033-19.059-9.13
                            c-5.292-6.087-7.944-13.459-7.944-22.108C63.521,74.195,66.173,66.825,71.465,60.749z"/>
								</g>
							</svg>
							<div className='fs0-85 pad0-75' style={{color: 'grey'}}>{__('no torrents for')} <b>{this.props.searchText}</b> {__('were found')}</div>
						</div>
						:
						null
				}
			</List>
		);
	}
}
