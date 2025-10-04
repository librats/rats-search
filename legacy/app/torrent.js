import React, { Component } from 'react';
import formatBytes from './format-bytes'
import {ListItem} from 'material-ui/List';
import Divider from 'material-ui/Divider';
import ToolTip from './tooltip';

import PagesPie from './pages-pie.js';
import TorrentPage from './torrent-page'

import LinearProgress from './LinearProgress';
let rating = require('./rating');
import scrollBack from './remember-scroll'
import TrackersImages from './trackers-images'
import DownloadTorrentMenu from './download-torrent-menu'

const contentIcon = (type, category, fill = 'grey') => {
	if(category == 'xxx')
	{
		return (
			<svg viewBox="0 0 18.282 18.282" fill={fill}>
				<g>
					<path d="M16.435,3.832H1.847C0.827,3.832,0,4.659,0,5.678v6.925c0,1.021,0.827,1.848,1.847,1.848h14.588
            c1.021,0,1.847-0.827,1.847-1.848V5.678C18.282,4.659,17.455,3.832,16.435,3.832z M3.194,7.123H2.583v4.042h0.611v0.54H1.876V6.583
            h1.318V7.123z M6.197,10.986l-0.392-0.784C5.644,9.9,5.541,9.676,5.419,9.425H5.406c-0.09,0.251-0.199,0.476-0.334,0.777
            l-0.36,0.784H3.593l1.254-2.191L3.638,6.654h1.125l0.379,0.791C5.27,7.709,5.367,7.921,5.47,8.165h0.013
            c0.103-0.276,0.187-0.47,0.296-0.72l0.366-0.791h1.118L6.042,8.768l1.285,2.218C7.327,10.986,6.197,10.986,6.197,10.986z
             M10.068,10.986l-0.392-0.784C9.515,9.9,9.412,9.676,9.29,9.425H9.277c-0.091,0.251-0.2,0.476-0.335,0.777l-0.359,0.784H7.464
            l1.253-2.191L7.509,6.654h1.125l0.379,0.791c0.128,0.264,0.225,0.476,0.328,0.72h0.013c0.103-0.276,0.186-0.47,0.295-0.72
            l0.366-0.791h1.119L9.913,8.768l1.284,2.218C11.197,10.986,10.068,10.986,10.068,10.986z M13.94,10.986l-0.393-0.784
            c-0.16-0.302-0.263-0.526-0.386-0.777h-0.012c-0.091,0.251-0.2,0.476-0.335,0.777l-0.36,0.784h-1.117l1.253-2.191l-1.209-2.141
            h1.125l0.379,0.791c0.129,0.264,0.226,0.476,0.328,0.72h0.013c0.104-0.276,0.187-0.47,0.296-0.72l0.366-0.791h1.118l-1.221,2.114
            l1.286,2.218C15.071,10.986,13.94,10.986,13.94,10.986z M16.756,11.705h-1.311v-0.54h0.611V7.116h-0.611v-0.54h1.311V11.705z"/>
				</g>
			</svg>
		)
	}

	switch(type)
	{
	case 'video':
		return (
			<svg viewBox="0 0 491.858 491.858" fill={fill}>
				<path d="M357.714,423.331c0,9.328-10.676,16.891-23.847,16.891H23.847C10.676,440.222,0,432.659,0,423.331V203.735
                  c0-9.33,10.676-16.892,23.847-16.892h310.02c13.171,0,23.847,7.564,23.847,16.892V423.331L357.714,423.331z"/>
				<circle cx="89.428" cy="118.706" r="59.619"/>
				<circle cx="253.381" cy="103.801" r="74.524"/>
				<path d="M491.858,447.677c0,0-1.986,14.904-15.899,14.904c-13.912,0-103.34-83.42-103.34-94.397V258.882
                  c0-10.976,87.443-94.398,103.34-94.398c15.899,0,15.899,14.905,15.899,14.905V447.677z"/>
			</svg>
		)
	case 'audio':
		return (
			<svg viewBox="0 0 46 46" fill={fill}>
				<path d="M28.38,0c-0.551,0-1.097,0.153-1.579,0.444c-0.046,0.027-0.09,0.059-0.13,0.093L13.121,12H2.487c-0.553,0-1,0.447-1,1v19
              c0,0.553,0.447,1,1,1h10.61L26.64,45.436c0.05,0.046,0.104,0.086,0.161,0.12C27.284,45.847,27.83,46,28.38,46
              c1.713,0,3.106-1.416,3.106-3.156V3.156C31.487,1.416,30.093,0,28.38,0z M14.487,31c0,0.553-0.447,1-1,1s-1-0.447-1-1v-4
              c0-0.553,0.447-1,1-1s1,0.447,1,1V31z M14.487,18c0,0.553-0.447,1-1,1s-1-0.447-1-1v-4c0-0.553,0.447-1,1-1s1,0.447,1,1V18z"/>
				<path d="M44.513,22.5c0-5.972-4.009-11.302-9.749-12.962c-0.533-0.151-1.084,0.152-1.238,0.684
              c-0.153,0.53,0.152,1.085,0.684,1.238c4.889,1.413,8.304,5.953,8.304,11.04s-3.415,9.627-8.304,11.04
              c-0.531,0.153-0.837,0.708-0.684,1.238c0.127,0.438,0.526,0.723,0.961,0.723c0.092,0,0.185-0.013,0.277-0.039
              C40.504,33.802,44.513,28.472,44.513,22.5z"/>
			</svg>

		)
	case 'pictures':
		return (
			<svg viewBox="0 0 58 58" fill={fill}>
				<path d="M57,6H1C0.448,6,0,6.447,0,7v44c0,0.553,0.448,1,1,1h56c0.552,0,1-0.447,1-1V7C58,6.447,57.552,6,57,6z M16,17
            c3.071,0,5.569,2.498,5.569,5.569c0,3.07-2.498,5.568-5.569,5.568s-5.569-2.498-5.569-5.568C10.431,19.498,12.929,17,16,17z
             M52.737,35.676c-0.373,0.406-1.006,0.435-1.413,0.062L40.063,25.414l-9.181,10.054l4.807,4.807c0.391,0.391,0.391,1.023,0,1.414
            s-1.023,0.391-1.414,0L23.974,31.389L7.661,45.751C7.471,45.918,7.235,46,7,46c-0.277,0-0.553-0.114-0.751-0.339
            c-0.365-0.415-0.325-1.047,0.09-1.412l17.017-14.982c0.396-0.348,0.994-0.329,1.368,0.044l4.743,4.743l9.794-10.727
            c0.179-0.196,0.429-0.313,0.694-0.325c0.264-0.006,0.524,0.083,0.72,0.262l12,11C53.083,34.636,53.11,35.269,52.737,35.676z"/>
			</svg>
		)
	case 'application':
		return (
			<svg viewBox="0 0 483.85 483.85" fill={fill}>
				<path d="M471.325,211.856l-56.9-56.9c-23.4-23.4-9.1-48.1,16.4-49.6c42-2.6,65.6-47.4,31.3-84.7c-37.3-34.2-81.9-10.7-84.5,31.2
                c-1.6,25.5-26.5,39.9-49.8,16.6l-55.7-55.7c-16.7-16.7-43.8-16.7-60.5,0l-56.4,56.4c-23.4,23.4-48.2,8.9-49.8-16.6
                c-2.6-42-47.6-65.9-84.9-31.6c-34.4,37.4-10.5,82.4,31.5,85c25.5,1.6,40,26.5,16.7,49.9l-56.2,56.1c-16.7,16.7-16.7,43.8,0,60.5
                l55.7,55.7c23.4,23.3,9.5,47.6-16,49.2c-42,2.6-65.5,47.3-31.2,84.6c37.3,34.3,81.8,10.9,84.4-31.1c1.6-25.5,26-39.5,49.4-16.2
                l56.2,56.2c17,17,44.8,17,61.8,0.1l39.4-39.4l16.9-16.9c22.1-23.1,7.8-47.4-17.4-49c-42-2.6-65.8-47.6-31.5-84.9
                c37.3-34.3,82.3-10.4,84.9,31.6c1.6,25.2,25.8,39.4,48.9,17.3l15.3-15.3l41.2-41.2c0.1-0.1,0.1-0.1,0.2-0.2l0.6-0.6
                C488.025,255.656,488.025,228.556,471.325,211.856z"/>
			</svg>
		)
	case 'books':
		return (
			<svg viewBox="0 0 296.999 296.999" fill={fill}>
				<g>
					<path d="M45.432,35.049c-0.008,0-0.017,0-0.025,0c-2.809,0-5.451,1.095-7.446,3.085c-2.017,2.012-3.128,4.691-3.128,7.543
                  v159.365c0,5.844,4.773,10.61,10.641,10.625c24.738,0.059,66.184,5.215,94.776,35.136V84.023c0-1.981-0.506-3.842-1.461-5.382
                  C115.322,40.849,70.226,35.107,45.432,35.049z"/>
					<path d="M262.167,205.042V45.676c0-2.852-1.111-5.531-3.128-7.543c-1.995-1.99-4.639-3.085-7.445-3.085c-0.009,0-0.018,0-0.026,0
                  c-24.793,0.059-69.889,5.801-93.357,43.593c-0.955,1.54-1.46,3.401-1.46,5.382v166.779
                  c28.592-29.921,70.038-35.077,94.776-35.136C257.394,215.651,262.167,210.885,262.167,205.042z"/>
					<path d="M286.373,71.801h-7.706v133.241c0,14.921-12.157,27.088-27.101,27.125c-20.983,0.05-55.581,4.153-80.084,27.344
                  c42.378-10.376,87.052-3.631,112.512,2.171c3.179,0.724,6.464-0.024,9.011-2.054c2.538-2.025,3.994-5.052,3.994-8.301V82.427
                  C297,76.568,292.232,71.801,286.373,71.801z"/>
					<path d="M18.332,205.042V71.801h-7.706C4.768,71.801,0,76.568,0,82.427v168.897c0,3.25,1.456,6.276,3.994,8.301
                  c2.545,2.029,5.827,2.78,9.011,2.054c25.46-5.803,70.135-12.547,112.511-2.171c-24.502-23.19-59.1-27.292-80.083-27.342
                  C30.49,232.13,18.332,219.963,18.332,205.042z"/>
				</g>
			</svg>
		)
	case 'archive':
		return (
			<svg viewBox="0 0 390 390" fill={fill}>
				<g>
					<path d="M182.681,205.334c0,5.21,4.227,9.436,9.436,9.436h5.765c5.21,0,9.436-4.226,9.436-9.436c0-5.211-4.226-9.438-9.436-9.438
                h-5.765C186.908,195.897,182.681,200.123,182.681,205.334z"/>
					<path d="M383.889,126.058c-4.478-5.191-10.868-8.314-17.674-8.686V64.899c0-25.562-20.797-46.359-46.361-46.359h-75.278
                c-25.052,0-38.351,10.578-48.062,18.303c-7.807,6.208-12.518,9.955-22.626,9.955H65.099c-22.78,0-41.313,18.062-41.313,40.264
                v30.311c-6.806,0.371-13.196,3.494-17.674,8.686c-4.78,5.541-6.912,12.888-5.839,20.125l30.194,203.803
                c1.828,12.338,12.417,21.475,24.89,21.475h279.286c12.473,0,23.063-9.137,24.891-21.475l30.195-203.802
                C390.801,138.945,388.669,131.599,383.889,126.058z M161.833,320.412v-10.428c0-2.399,1.945-4.345,4.345-4.345h32.443
                c2.399,0,4.345,1.945,4.345,4.345v10.428c0,2.4-1.945,4.345-4.345,4.345h-32.443C163.778,324.757,161.833,322.813,161.833,320.412z
                 M195,160.323c4.274,0,7.738-3.467,7.738-7.738v-10.718h6.07c4.615,0,8.445,3.564,8.776,8.167l4.893,67.999
                c0.175,2.438-0.671,4.838-2.336,6.626c-1.664,1.79-3.996,2.806-6.441,2.806h-37.401c-2.444,0-4.777-1.016-6.441-2.806
                c-1.665-1.788-2.511-4.188-2.336-6.626l4.893-67.999c0.331-4.603,4.161-8.167,8.776-8.167h6.07v10.718
                C187.262,156.856,190.726,160.323,195,160.323z M198.621,244.81c2.399,0,4.345,1.945,4.345,4.345v10.429
                c0,2.399-1.945,4.345-4.345,4.345h-32.443c-2.4,0-4.345-1.945-4.345-4.345v-10.429c0-2.399,1.945-4.345,4.345-4.345H198.621z
                 M228.167,350.538c0,2.399-1.945,4.345-4.345,4.345h-32.443c-2.399,0-4.345-1.945-4.345-4.345V340.11
                c0-2.4,1.946-4.346,4.345-4.346h32.443c2.399,0,4.345,1.945,4.345,4.346V350.538z M228.167,289.708c0,2.4-1.945,4.345-4.345,4.345
                h-32.443c-2.399,0-4.345-1.944-4.345-4.345V279.28c0-2.4,1.946-4.346,4.345-4.346h32.443c2.399,0,4.345,1.945,4.345,4.346V289.708z
                 M335.919,117.333H54.081V87.062c0-6.239,5.602-9.968,11.019-9.968h108.788c20.687,0,32.219-9.173,41.484-16.542
                c8.552-6.803,14.732-11.717,29.204-11.717h75.278c8.858,0,16.066,7.206,16.066,16.064V117.333z"/>
				</g>
			</svg>
		)
	case 'disc':
		return (
			<svg viewBox="0 0 49.652 49.652" fill={fill}>
				<g>
					<circle cx="24.826" cy="24.825" r="3.529"/>
					<path d="M42.381,7.271C37.693,2.582,31.458,0,24.826,0C18.195,0,11.96,2.583,7.271,7.271c-9.68,9.68-9.68,25.43,0,35.11
                  c4.689,4.688,10.923,7.271,17.555,7.271c6.632,0,12.867-2.582,17.555-7.271C52.061,32.701,52.061,16.951,42.381,7.271z
                   M24.86,45.002l0.039-12.587c-1.967,0.019-3.941-0.719-5.442-2.22c-2.965-2.965-2.964-7.772,0-10.737
                  c0.022-0.022,0.047-0.04,0.069-0.062l-8.935-8.936c4.059-4.072,9.234-6.027,14.363-5.91l-0.039,12.689
                  c1.915,0.022,3.82,0.759,5.28,2.219c2.942,2.942,2.96,7.699,0.063,10.668l8.967,8.968C35.166,43.164,29.99,45.119,24.86,45.002z"
					/>
				</g>
			</svg>
		)
	default:
		return (
			<svg viewBox="0 0 123.769 123.769" fill={fill}>
				<g>
					<path d="M76.05,1.568l-10.101,9.3c-2.3,2.1-5.8,2.1-8.1,0l-10.2-9.2c-3.1-2.8-8-1.7-9.6,2.1l-8.3,20h64.2l-8.3-20.1
                C84.05-0.131,79.149-1.231,76.05,1.568z"/>
					<path d="M10.749,42.068c-2.9,1.4-1.8,5.7,1.3,5.7h49.8h49.701c3.199,0,4.199-4.3,1.399-5.7l-12.2-6.3h-77.8L10.749,42.068z"/>
					<path d="M0.549,90.168l5.3,28.801c0.5,2.899,3,4.8,5.9,4.8h50.1h50.201c2.899,0,5.399-2,5.899-4.8l5.3-28.801
                c0.5-2.8-1-5.6-3.699-6.699c-12.801-5-26.2-7.7-36.801-9.301c-2.699-0.399-5.3,1.101-6.3,3.5l-10.1,22.9c-1.8,4-7.5,4-9.201-0.1
                l-9.8-22.7c-1.1-2.5-3.7-4-6.4-3.601c-10.6,1.5-24,4.301-36.7,9.301C1.549,84.469-0.051,87.269,0.549,90.168z"/>
				</g>
			</svg>
		)
	}
};
export {contentIcon}

export default class Torrent extends Component {
  state = {
  	downloading: false,
  	downloaded: false,
  	startingDownloading: false,
  	downloadProgress: {},
  	downloadRemoveOnDone: false,
  	downloadPaused: false,
  }
  downloadInited = false;

  constructor(props)
  {
	super(props)
	
	const download = props.download || props.torrent.download;
	this.updateDownload(download);
  	this.downloadPathInput = React.createRef();
  }

  updateDownload(download)
  {
	if(download)
	{
		const { progress, downloaded, downloadSpeed, removeOnDone, paused } = download
		this.state.downloadProgress = {
			progress, downloaded, downloadSpeed
		}
		this.state.downloading = progress < 1
		this.state.downloaded = progress === 1
		this.state.downloadRemoveOnDone = removeOnDone
		this.state.downloadPaused = paused

		this.downloadInited = true;
	}
  }

  componentDidUpdate(prevProps)
  {
	  const download = this.props.download || this.props.torrent.download;
	  if(prevProps && !prevProps.download && download && !this.downloadInited) {
		this.updateDownload(download);
		this.forceUpdate();
	  }
  }

  componentDidMount()
  {
  	scrollBack()

  	this.downloading = (hash) => {
  		if(this.props.torrent.hash != hash)
  			return;

  		this.setState({
  			downloading: true,
  			startingDownloading: false
  		})
  	}
  	window.torrentSocket.on('downloading', this.downloading);

  	this.downloadDone = (hash, canceled) => {
  		if(this.props.torrent.hash != hash)
  			return;

  		this.setState({
  			downloading: false,
  			downloaded: !canceled,
  			startingDownloading: false
  		})
  	}
  	window.torrentSocket.on('downloadDone', this.downloadDone);

  	this.downloadProgress = (hash, progress) => {
  		if(this.props.torrent.hash != hash)
  			return;

  		this.setState({
  			downloading: true,
  			startingDownloading: false,
  			downloadProgress: progress
  		})
  	}
  	window.torrentSocket.on('downloadProgress', this.downloadProgress);
      
  	this.downloadUpdate = (hash, options) => {
  		if(this.props.torrent.hash != hash)
  			return;

  		this.setState({
  			downloadRemoveOnDone: options.removeOnDone,
  			downloadPaused: options.paused
  		})
  	}
  	window.torrentSocket.on('downloadUpdate', this.downloadUpdate);
  }
  componentWillUnmount()
  {
  	if(this.downloading)
  		window.torrentSocket.off('downloading', this.downloading);
  	if(this.downloadDone)
  		window.torrentSocket.off('downloadDone', this.downloadDone);
  	if(this.downloadProgress)
  		window.torrentSocket.off('downloadProgress', this.downloadProgress);
  	if(this.downloadUpdate)
  		window.torrentSocket.off('downloadUpdate', this.downloadUpdate);
  }
  render()
  {
  	const torrent = this.props.torrent;
  	if(!torrent)
  		return null // can try draw null torrent (for example on downloading not started)  

  	let torrentRating = -1
  	if(torrent.good > 0 || torrent.bad > 0)
  		torrentRating = Math.round(rating(torrent.good, torrent.bad) * 100);

  	const canDeleteDownloadAfterFinish = (this.state.downloading || this.state.startingDownloading) && !this.state.downloaded
  	const canPause = (this.state.downloading || this.state.startingDownloading)

  	return (
  		<div>
  			<ListItem 
  				className='torrentRow'
  				innerDivStyle={{paddingRight: 84}}
  				onClick={(e) => {
  					const link = '/torrent/' + torrent.hash;
  					if(e.button === 1)
  						return false;

  					/*
            if(e.ctrlKey && e.button === 0) {
              let win = window.open(link, '_blank');
              //win.focus();
              return true;
            }
            */
  					window.rememberYOffset = window.pageYOffset
  					window.routerFix()
  					PagesPie.instance().open(TorrentPage, {replace: 'all', hash: torrent.hash, peer: torrent.peer})
  				}} 
  				primaryText={
  					<a href={'/torrent/' + torrent.hash} ref={(node) => {
  						if(node)
  							node.onclick = () => { return false }
  					}}>
  						<span className='break-word torrentName' style={{
  							color: torrent.contentCategory != 'xxx' ? (torrent.peer ? '#5643db' : 'black') : (torrent.peer ? '#9083e2' : 'grey')
  						}}>
  							{torrent.name}
  						</span>
  					</a>
  				}
  				secondaryText={
  					<a href={'/torrent/' + torrent.hash} ref={(node) => {
  						if(node)
  							node.onclick = () => { return false }
  					}}>
  						<div className='column' style={{height: 'auto', whiteSpace: 'normal', paddingTop: '0.30em'}}>
  							{
  								torrent.info && torrent.info.name
                            &&
                            <div className='row w100p inline fs0-85' style={{color: 'grey', padding: '4px 0px 5px'}}>
                            	{torrent.info.name}
                            </div>
  							}
  							<div className='row w100p inline torrentSize'>
  								<div style={{color: torrent.contentCategory != 'xxx' ? (torrent.peer ? '#5252d1' : 'black') : (torrent.peer ? '#9083e2' : 'grey')}}>
  									{
  										formatBytes(torrent.size, 1) + ' (' + torrent.files + ' files)'
  									}
  								</div>
  							</div>
  							{
  								torrent.path && torrent.path.length > 0
  									?
  									torrent.path.map((path, index) => {
  										const boldRe = /\<b\>(.+?)\<\/b\>/g;
  										let boldText;
  										let newPath = [];
  										let idx = 0;
  										while ((boldText = boldRe.exec(path)) !== null) {
  											newPath.push(<span>{path.substring(idx, boldText.index)}</span>);
  											newPath.push(<b>{boldText[1]}</b>);
  											idx = boldRe.lastIndex;
  										}
  										newPath.push(<span>{path.substring(idx, path.length)}</span>);
  										return <div key={index} className='break-word fs0-75' style={{paddingTop: '0.3em', marginLeft: '0.6em'}}>{newPath}</div>
  									})
  									:
  									null
  							}
  							{
  								torrent.seeders || torrent.leechers || torrent.completed
  									?
  									<div className='break-word fs0-85' style={{paddingTop: '0.35em'}}>
  										<span style={{color: (torrent.seeders > 0 ? '#00C853' : 'grey')}}>{torrent.seeders} {__('seeders')}</span>
  										<span style={{color: (torrent.leechers > 0 ? '#AA00FF' : 'grey'), marginLeft: '12px'}}>{torrent.leechers} {__('leechers')}</span>
  										<span style={{color: (torrent.completed > 0 ? '#FF6D00' : 'grey'), marginLeft: '12px'}}>{torrent.completed} {__('completed')}</span>
  									</div>
  									:
  									null
  							}
  							{
  								(torrent.good > 0 || torrent.bad > 0)
                  &&
                  <div className='row w100p inline' style={{maxWidth: 470}}>
                  	<LinearProgress 
                  		mode="determinate" 
                  		value={torrentRating}
                  		color={torrentRating >= 50 ? '#00E676' : '#FF3D00'}
                  		style={{
                  			height: '5px',
                  		}}
                  	/>
                  	<div className='row center pad0-5 fs0-85 text-nowrap' style={{color: torrentRating >= 50 ? '#00E676' : '#FF3D00', width: '190px'}}>{__('Torrent rating')}: {torrentRating}%</div>
                  </div>
  							}
  							{
  								(this.state.downloading || this.state.downloaded)
                                    &&
                                    <div className='row w100p inline text-nowrap' style={{maxWidth: 580}}>
                                    	<div style={{marginRight: 5, color: 'rgb(0, 188, 212)'}}>{__('downloading')}: </div>
                                    	<LinearProgress 
                                    		style={{height: '5px', width: '44%', marginTop: 2}}
                                    		mode="determinate" 
                                    		value={this.state.downloadProgress && (this.state.downloadProgress.progress ? this.state.downloadProgress.progress : 0) * 100}
                                    	/>
                                    	<div className='pad0-75 progressDownloading' style={{marginLeft: 20}} style={{color: 'rgb(0, 188, 212)'}}>{this.state.downloadProgress && (this.state.downloadProgress.progress * 100).toFixed(1)}%</div>
                                    	{
                                    		this.state.downloadProgress.progress !== 1
                                            &&
                                            <div style={{marginLeft: 5, color: 'rgb(0, 188, 212)'}}>{this.state.downloadProgress && formatBytes(this.state.downloadProgress.downloadSpeed || 0, 0)}/s</div>
                                    	}
                                    </div>
  							}
  							<TrackersImages info={torrent.info} className='row' />
  						</div>
  					</a>
  				}
  				leftIcon={contentIcon(torrent.contentType, torrent.contentCategory, torrent.contentCategory != 'xxx' ? (torrent.peer ? '#6f5ee0' : 'grey') : (torrent.peer ? '#9083e2' : '#d3d3d3'))}
  				rightIcon={
  					<div className='row inline' style={{width: 63 + (canDeleteDownloadAfterFinish ? 40 : 0) + (canPause ? 40 : 0)}}>
  						{
  							// mark delete after finish
  							canPause
                              &&
                              <ToolTip hint={__('Pause torrent downloading')} right={true}>
                              	<a href={`magnet:?xt=urn:btih:${torrent.hash}`}>
                              		<svg className='pauseTorrent' style={{
                              			height: '24px',
                              			marginRight: 16,
                              			fill: this.state.downloadPaused ? '#42f47a' : 'black'
                              		}} onClick={(e) => {
                              			e.preventDefault();
                              			e.stopPropagation();
                              			window.torrentSocket.emit('downloadUpdate', {hash: torrent.hash, options: {pause: 'switch'}})
                              		}} viewBox="0 0 438.536 438.536">
                              			<g>
                              				<path d="M164.453,0H18.276C13.324,0,9.041,1.807,5.425,5.424C1.808,9.04,0.001,13.322,0.001,18.271v401.991
                                                    c0,4.948,1.807,9.233,5.424,12.847c3.619,3.617,7.902,5.428,12.851,5.428h146.181c4.949,0,9.231-1.811,12.847-5.428
                                                    c3.617-3.613,5.424-7.898,5.424-12.847V18.271c0-4.952-1.807-9.231-5.428-12.847C173.685,1.807,169.402,0,164.453,0z"/>
                              				<path d="M433.113,5.424C429.496,1.807,425.215,0,420.267,0H274.086c-4.949,0-9.237,1.807-12.847,5.424
                                                    c-3.621,3.615-5.432,7.898-5.432,12.847v401.991c0,4.948,1.811,9.233,5.432,12.847c3.609,3.617,7.897,5.428,12.847,5.428h146.181
                                                    c4.948,0,9.229-1.811,12.847-5.428c3.614-3.613,5.421-7.898,5.421-12.847V18.271C438.534,13.319,436.73,9.04,433.113,5.424z"/>
                              			</g>
                              		</svg>
                              	</a>
                              </ToolTip>
  						}
  						{
  							// mark delete after finish
  							canDeleteDownloadAfterFinish
                              &&
                              <ToolTip hint={__('Dont start to seed torrent after download finish')} right={true}>
                              	<a href={`magnet:?xt=urn:btih:${torrent.hash}`}>
                              		<svg style={{
                              			height: '24px',
                              			marginRight: 16,
                              			fill: this.state.downloadRemoveOnDone ? '#f45c41' : 'black'
                              		}} onClick={(e) => {
                              			e.preventDefault();
                              			e.stopPropagation();
                              			window.torrentSocket.emit('downloadUpdate', {hash: torrent.hash, options: {removeOnDone: 'switch'}})
                              		}} viewBox="0 0 512 512">
                              			<g>
                              				<path d="M456.313,85.333h-55.527C386.809,36.16,341.594,0,288,0s-98.809,36.16-112.785,85.333h-69.441l-3.482-11.938
                                                c-5.271-18.094-22.115-30.729-40.958-30.729H32c-5.896,0-10.667,4.771-10.667,10.667C21.333,59.229,26.104,64,32,64h29.333
                                                c9.427,0,17.844,6.313,20.479,15.365L148.208,307l-34.021,42.521c-4.854,6.073-7.521,13.688-7.521,21.458
                                                c0,18.948,15.406,34.354,34.354,34.354h296.313c5.896,0,10.667-4.771,10.667-10.667S443.229,384,437.333,384H141.021
                                                c-7.177,0-13.021-5.844-13.021-13.021c0-2.948,1.01-5.844,2.854-8.135L165.133,320h209.221c16.448,0,31.604-9.615,38.615-24.5
                                                l74.438-158.177c2.135-4.552,3.26-9.604,3.26-14.615v-3.021C490.667,100.74,475.26,85.333,456.313,85.333z M288,21.333
                                                c52.938,0,96,43.063,96,96s-43.063,96-96,96s-96-43.063-96-96S235.063,21.333,288,21.333z M469.333,122.708
                                                c0,1.906-0.427,3.823-1.24,5.542l-74.427,158.167c-3.51,7.438-11.083,12.25-19.313,12.25H168l-56.004-192h59.211
                                                c-0.319,3.518-0.54,7.066-0.54,10.667c0,64.698,52.635,117.333,117.333,117.333s117.333-52.635,117.333-117.333
                                                c0-3.6-0.221-7.148-0.54-10.667h51.52c7.177,0,13.021,5.844,13.021,13.021V122.708z"/>
                              				<path d="M149.333,426.667c-23.531,0-42.667,19.135-42.667,42.667S125.802,512,149.333,512S192,492.865,192,469.333
                                                S172.865,426.667,149.333,426.667z M149.333,490.667c-11.76,0-21.333-9.573-21.333-21.333c0-11.76,9.573-21.333,21.333-21.333
                                                c11.76,0,21.333,9.573,21.333,21.333C170.667,481.094,161.094,490.667,149.333,490.667z"/>
                              				<path d="M405.333,426.667c-23.531,0-42.667,19.135-42.667,42.667S381.802,512,405.333,512S448,492.865,448,469.333
                                                S428.865,426.667,405.333,426.667z M405.333,490.667c-11.76,0-21.333-9.573-21.333-21.333c0-11.76,9.573-21.333,21.333-21.333
                                                c11.76,0,21.333,9.573,21.333,21.333C426.667,481.094,417.094,490.667,405.333,490.667z"/>
                              				<path d="M248.458,156.875c2.083,2.083,4.813,3.125,7.542,3.125s5.458-1.042,7.542-3.125L288,132.417l24.458,24.458
                                                c2.083,2.083,4.813,3.125,7.542,3.125s5.458-1.042,7.542-3.125c4.167-4.167,4.167-10.917,0-15.083l-24.458-24.458l24.458-24.458
                                                c4.167-4.167,4.167-10.917,0-15.083c-4.167-4.167-10.917-4.167-15.083,0L288,102.25l-24.458-24.458
                                                c-4.167-4.167-10.917-4.167-15.083,0c-4.167,4.167-4.167,10.917,0,15.083l24.458,24.458l-24.458,24.458
                                                C244.292,145.958,244.292,152.708,248.458,156.875z"/>
                              			</g>
                              		</svg>
                              	</a>
                              </ToolTip>
  						}
  						{
  							!this.state.startingDownloading && !this.state.downloading && !this.state.downloaded
  								?
  								<DownloadTorrentMenu torrent={torrent} onAdded={(added) => {
  									if(added)
  										this.setState({startingDownloading: true})
  								}}>
  									<ToolTip hint={__('Download using built-in client')} right={true}>
  										<a className='downloadButton' href={`magnet:?xt=urn:btih:${torrent.hash}`}>
  											<svg style={{
  												height: '24px',
  												marginRight: 12,
  												fill: torrent.contentCategory != 'xxx' ? (torrent.peer ? '#5643db' : 'black') : (torrent.peer ? '#9083e2' : 'grey')
  											}} viewBox="0 0 56 56">
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
  										</a>
  									</ToolTip>
  								</DownloadTorrentMenu>
  								:
  								this.state.startingDownloading && !this.state.downloading
  									?
  									<ToolTip hint={__('Serching metadata in progress... Click will delete this torrent.')} right={true}>
  										<div className="overlay-loader" onClick={(e) => {
  											e.preventDefault();
  											e.stopPropagation();
  											window.torrentSocket.emit('downloadCancel', {hash: torrent.hash})
  										}}>
  											<div className="loader">
  												<div></div>
  												<div></div>
  												<div></div>
  												<div></div>
  												<div></div>
  												<div></div>
  												<div></div>
  											</div>
  										</div>
  									</ToolTip>
  									:
  									this.state.downloaded
  										?
  										<ToolTip hint={__('Delete download (files saved)')} right={true}>
  											<a className='deleteDownload deleteDownloadAfterFinish' href={`magnet:?xt=urn:btih:${torrent.hash}`}>
  												<svg style={{
  													height: '24px',
  													fill: '#00C853',
  													marginRight: 12
  												}} onClick={(e) => {
  													e.preventDefault();
  													e.stopPropagation();

  													window.torrentSocket.emit('downloadCancel', {hash: torrent.hash})
  												}} viewBox="0 0 18 18"><path d="M9 1C4.58 1 1 4.58 1 9s3.58 8 8 8 8-3.58 8-8-3.58-8-8-8zm4 10.87L11.87 13 9 10.13 6.13 13 5 11.87 7.87 9 5 6.13 6.13 5 9 7.87 11.87 5 13 6.13 10.13 9 13 11.87z"/></svg>
  											</a>
  										</ToolTip>
  										:
  										this.state.downloading
                                            &&
                                            <ToolTip hint={__('Delete download (files saved)')} right={true}>
                                            	<a className='deleteDownload deleteDownloadBeforeFinish' href={`magnet:?xt=urn:btih:${torrent.hash}`}>
                                            		<svg style={{
                                            			height: '24px',
                                            			marginRight: 12,
                                            			fill: torrent.contentCategory != 'xxx' ? (torrent.peer ? '#5643db' : 'black') : (torrent.peer ? '#9083e2' : 'grey')
                                            		}} onClick={(e) => {
                                            			e.preventDefault();
                                            			e.stopPropagation();

                                            			window.torrentSocket.emit('downloadCancel', {hash: torrent.hash})
                                            		}} viewBox="0 0 18 18"><path d="M9 1C4.58 1 1 4.58 1 9s3.58 8 8 8 8-3.58 8-8-3.58-8-8-8zm4 10.87L11.87 13 9 10.13 6.13 13 5 11.87 7.87 9 5 6.13 6.13 5 9 7.87 11.87 5 13 6.13 10.13 9 13 11.87z"/></svg>
                                            	</a>
                                            </ToolTip>
  						}
  						<ToolTip hint={__('Open torrent in external torrent client')} right={true}>
  							<a style={{float: 'right'}} href={`magnet:?xt=urn:btih:${torrent.hash}`}>
  								<svg style={{
  									height: '24px',
  									fill: torrent.contentCategory != 'xxx' ? (torrent.peer ? '#5643db' : 'black') : (torrent.peer ? '#9083e2' : 'grey')
  								}} onClick={(e) => {
  									e.preventDefault();
  									e.stopPropagation();
  									var win = window.open(`magnet:?xt=urn:btih:${torrent.hash}`, '_self');
  								}} viewBox="0 0 24 24">
  									<path d="M15.82 10.736l-5.451 6.717c-.561.691-1.214 1.042-1.94 1.042-1.144 
                  0-2.327-.899-2.753-2.091-.214-.6-.386-1.76.865-2.784 3.417-2.794 6.716-5.446 
                  6.716-5.446l-3.363-4.174s-4.532 3.657-6.771 5.487c-2.581 2.108-3.123 4.468-3.123 
                  6.075 0 4.416 4.014 8.438 8.42 8.438 1.604 0 3.963-.543 6.084-3.128 1.835-2.237 
                  5.496-6.773 5.496-6.773l-4.18-3.363zm-2.604 9.079c-1.353 1.647-3.01 2.519-4.796 
                  2.519-3.471 0-6.753-3.291-6.753-6.771 0-1.789.867-3.443 2.51-4.785 1.206-.986 
                  2.885-2.348 4.18-3.398l1.247 1.599c-1.074.87-2.507 2.033-4.118 3.352-1.471 
                  1.202-1.987 2.935-1.38 4.634.661 1.853 2.479 3.197 4.322 3.197h.001c.86 0 
                  2.122-.288 3.233-1.658l3.355-4.134 1.572 1.294c-1.044 1.291-2.392 2.954-3.373 
                  4.151zm6.152-7.934l4.318-2.88-1.575-.638 1.889-2.414-4.421 2.788 1.716.695-1.927 
                  2.449zm-7.292-7.186l4.916-1.667-1.356-1.022 2.448-2.006-4.991 1.712 
                  1.478 1.114-2.495 1.869z"/></svg>
  							</a>
  						</ToolTip>
  					</div>
  				}
  			/>
  			<Divider />
  		</div>
  	)
  }
}