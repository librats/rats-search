import React from 'react';
import Page from './page';

import AppBar from 'material-ui/AppBar';
import IconButton from 'material-ui/IconButton';
import NavigationClose from 'material-ui/svg-icons/navigation/close';

export default class DMCAPage extends Page {

	render() {
		return (
			<div className='w100p column'>
				<AppBar
					title="DMCA / Copyright"
					iconElementLeft={<IconButton onClick={()=>{ window.router('/') }}><NavigationClose /></IconButton>}
				/>

				<div className='column w100p pad1 center'>
					<div>RatsOnTheBoat.org is in compliance with 17 U.S.C. § 512, the Digital Millennium Copyright Act ("DMCA") and the Directive 2001/29/EC of the European Parliament.</div>
            
					<div className='fs1-5 pad0-75'>Content status</div>

            Our main goal is collect and make analysis of information from the torrent network. We don't save/distribute any real content/data and also don't save any torrents files - we are respect DMCA law. 
            Information collected automaticly and the real source are torrent clients based on torrent protocol.

					<div className='fs1-5 pad0-75'>Block mechanisms (for rightholders)</div>

            Right holders can block/remove content that they responsible for. Contact administration or left application about content removal.
				</div>
			</div>
		);
	}
}
