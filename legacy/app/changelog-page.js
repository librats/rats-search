import React from 'react';
import Page from './page';
import ReactMarkdown from 'react-markdown'
import fs from 'fs'

export default class ChangeLog extends Page {
	constructor(props) {
		super(props)
		this.setTitle('Changelog');

		let changelogPath = 'CHANGELOG.md'
		if(!fs.existsSync(changelogPath))
			changelogPath = 'resources/CHANGELOG.md'

		this.changelog = fs.readFileSync(changelogPath)
		if(!this.changelog)
			throw new Error('no changelog file')
	}
	render() {
		return (
			<div className='pad0-75'>
				<ReactMarkdown skipHtml={true} children={this.changelog.toString()} />
			</div>
		);
	}
}
