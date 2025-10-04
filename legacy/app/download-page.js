import React from 'react';
import Page from './page';

import TorrentLine from './torrent'
import {List} from 'material-ui/List';
import Subheader from 'material-ui/Subheader';
import RaisedButton from 'material-ui/RaisedButton';

export default class TopPage extends Page {
  downloads = []

  constructor(props) {
  	super(props)
  	this.setTitle('Current Downloads');
  }
  getDownloads()
  {
  	window.torrentSocket.emit('downloads', window.customLoader((downloads) => {
  		this.downloads = downloads
  		this.forceUpdate()
  	}))
  }
  componentDidMount()
  {
  	super.componentDidMount();
  	this.getDownloads()
  	this.downloading = () => this.getDownloads()
  	window.torrentSocket.on('downloading', this.downloading);
  	this.downloadDone = () => this.getDownloads()
  	window.torrentSocket.on('downloadDone', this.downloadDone);
  }
  componentWillUnmount()
  {
  	if(this.downloading)
  		window.torrentSocket.off('downloading', this.downloading);
  	if(this.downloadDone)
  		window.torrentSocket.off('downloadDone', this.downloadDone);
  }
  render() {
  	return (
  		<div>
  			<div className='column center w100p pad0-75'>
  				<RaisedButton label={__('Back to main page')} primary={true} onClick={() => {
  					window.router('/')
  				}} />
  				<List style={{paddingBottom: '70px', minWidth: '60%'}} className='animated recent-torrents downloads-list'>
  					{
  						this.downloads.map((torrentDownload, index) => {
  							const {torrentObject: torrent, ...download} = torrentDownload
  							return <TorrentLine key={index} torrent={torrent} download={download} />
  						})
  					}
  				</List>
  			</div>
  		</div>
  	);
  }
}
