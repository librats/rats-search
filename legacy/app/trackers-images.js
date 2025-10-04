import React from 'react';

import RutrackerIcon from './images/strategies/rutracker.png'
import NyaaIcon from './images/strategies/nyaa.jpg'
import RutorIcon from './images/strategies/rutor.png'

export default (props) => {
	let className = ''
	if(props.className)
		className += props.className
	const {info} = props
	if(!info)
		return null

	if(!info.trackers)
		return null

	return (
		<div>
			{
				info.trackers.includes('rutracker')
            &&
            <a href={`https://rutracker.org/forum/viewtopic.php?t=${info.rutrackerThreadId}`}><img src={RutrackerIcon} style={{height: 32}} /></a>
			}
			{
				info.trackers.includes('nyaa')
            &&
           <img src={NyaaIcon} style={{height: 32}} />
			}
			{
				info.trackers.includes('rutor')
            &&
            <a href={`http://www.rutor.is/torrent/${info.rutorThreadId}`}><img src={RutorIcon} style={{height: 32}} /></a>
			}
			{
				info.trackers.includes('1337x')
            &&
            <a href={`https://1337x.to${info.x1337Href ? info.x1337Href : ''}`}><img src='https://upload.wikimedia.org/wikipedia/commons/thumb/b/bb/1337X_logo.svg/1200px-1337X_logo.svg.png' style={{height: 32}} /></a>
			}
		</div>
	)
}