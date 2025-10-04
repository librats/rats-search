import React from 'react';
import Page from './page';

import Toggle from 'material-ui/Toggle';
import RaisedButton from 'material-ui/RaisedButton';
import TextField from 'material-ui/TextField'
import Slider from 'material-ui/Slider'
import SelectField from 'material-ui/SelectField';
import MenuItem from 'material-ui/MenuItem';
import InputSize from './input-size';
import Checkbox from 'material-ui/Checkbox';

export default class ConfigPage extends Page {
	constructor(props) {
		super(props)
		this.setTitle('Rats filters');
		this.options = {}
		this.basicTypes = ['video',
			'audio',
			'pictures',
			'books',
			'application',
			'archive',
			'disc']
		this.contentType = this.basicTypes.slice(0)
		this.descriptions = {
			main: __('All'),
			video: __('Video'),
			audio: __('Audio/Music'),
			books: __('Books'),
			pictures: __('Pictures/Images'),
			application: __('Apps/Games'),
			archive: __('Archives'),
			disc: __('Discs/ISO')
		}
		this.removed = 0
		this.removedMax = 0
	}
	componentDidMount() {
		this.loadSettings()

		let time = Date.now() - 1000
		this.cleanTorrent = (removed, max, status) => {
			if(Date.now() - time < 400)
				return
			time = Date.now()

			this.removed = removed
			this.removedMax = max
			this.realRemove = (status !== 'check')
			this.forceUpdate()
		};
		window.torrentSocket.on('cleanTorrent', this.cleanTorrent);
	}
	componentWillUnmount()
	{
		if(this.cleanTorrent)
			window.torrentSocket.off('cleanTorrent', this.cleanTorrent);
	}
	loadSettings() {
		window.torrentSocket.emit('config', window.customLoader((options) => {
			this.options = options;

			if(this.options.filters && this.options.filters.contentType && this.options.filters.contentType.length > 0)
				this.contentType = this.options.filters.contentType

			this.forceUpdate();
		}));
	}
	saveSettings() {
		if(this.options.filters && this.basicTypes.length === this.contentType.length)
			this.options.filters.contentType = null // обнуляем в случае если заданы все фильтры
		else
			this.options.filters.contentType = this.contentType

		window.torrentSocket.emit('setConfig', {options: this.options})
		this.settingsSavedMessage = true
		this.forceUpdate()
		setTimeout(() => {
			this.settingsSavedMessage = false
			this.forceUpdate()
		}, 1000)
	}
	render() {
		return (
			<div>
				<div className='row center pad0-75'>
					<RaisedButton label={__("Back to main page")} primary={true} onClick={() => {
						window.router('/')
					}} />
				</div>
          
				<div className='column center w100p pad0-75'>

					<div className='column w100p'>
						<div className='row inline w100p'>
							<div style={{flex: 1}}>{__('Max files per torrent')} ({__('current')}: {this.options.filters && this.options.filters.maxFiles})</div>
							<Slider
								min={0}
								max={50000}
								step={1}
								style={{width: 300}}
								value={this.options.spider && this.options.filters.maxFiles}
								onChange={(event, value) => {
									this.options.filters.maxFiles = value
									this.forceUpdate()
								}}
							/>
							<TextField
								hintText={__('Max files')}
								className='pad0-75'
								style={{width: 200}}
								value={this.options.filters && this.options.filters.maxFiles}
								onChange={(e, value) => {
									if(!this.options.filters)
										return
                
									this.options.filters.maxFiles = parseInt(value)
									this.forceUpdate()
								}}
							/>
						</div>
						<div className='fs0-75' style={{color: 'grey'}}>* 0 - {__('Disabled')}.
						</div>
					</div>

					<div className='column w100p'>
						<div className='row inline w100p'>
							<div style={{flex: 1}}>{__('Torrent name regular extension filtering')}</div>
							<TextField
								hintText="regex"
								className='pad0-75'
								style={{width: 400}}
								value={this.options.filters && this.options.filters.namingRegExp}
								onChange={(e, value) => {
									if(!this.options.filters)
										return
                
									this.options.filters.namingRegExp = value
									this.forceUpdate()
								}}
							/>
							<SelectField
								style={{marginLeft: 15}}
								floatingLabelText={__('Examples')}
								value={this.options.filters && this.options.filters.namingRegExp}
								onChange={(event, index, value) => {
									if(!this.options.filters)
										return

									this.options.filters.namingRegExp = value
									this.forceUpdate()
								}}
							>
								<MenuItem value={String.raw`^[А-Яа-я0-9A-Za-z.!@?#"$%&:;() *\+,\/;\-=[\\\]\^_{|}<>\u0400-\u04FF]+$`} primaryText={__('Russian + English only (With symbols)')} />
								<MenuItem value={'^[0-9A-Za-z.!@?#"$%&:;() *\+,\/;\-=[\\\]\^_{|}<>]+$'} primaryText={__('English only (With symbols)')} />
								<MenuItem value={'^((?!badword).)*$'} primaryText={__('Ignore badword')} />
							</SelectField>
						</div>
						<Toggle
							style={{marginTop: '10px'}}
							label={__('Negative regular extension filtering')}
							toggled={this.options.filters && this.options.filters.namingRegExpNegative}
							onToggle={(e, checked) => {
								if(!this.options.filters)
									return

								this.options.filters.namingRegExpNegative = checked
								this.forceUpdate()
							}}
						/>
						<div className='fs0-75' style={{color: 'grey'}}>
          * - {__('clean string means disabled')}
						</div>
					</div>

					<div className='column w100p'>
						<Toggle
							style={{marginTop: '10px'}}
							label={__('Adult filter')}
							toggled={this.options.filters && this.options.filters.adultFilter}
							onToggle={(e, checked) => {
								if(!this.options.filters)
									return

								this.options.filters.adultFilter = checked
								this.forceUpdate()
							}}
						/>
						<div className='fs0-75' style={{color: 'grey'}}>
                            * - {__('* - enabled means ignoring all adult content')}
						</div>
					</div>

					{
						this.options.filters // bug with props
                        &&
                    <div className='w100p'>
                    	<InputSize 
                    		value={this.options.filters && this.options.filters.size} 
                    		enabled={this.options.filters && this.options.filters.sizeEnabled} 
                    		maxSize={this.options.filters && this.options.filters.maxSize} 
                    		onChange={({size, maxSize, enabled}) => {
                    			this.options.filters.size = size
                    			this.options.filters.maxSize = maxSize
                    			this.options.filters.sizeEnabled = enabled
                    			this.forceUpdate()
                    		}} />
                    </div>
					}

					<div className='column w100p'>
						<div style={{flex: 1, padding: '8px 0px'}}>{__('disable some categories')}:</div>
						{
							this.basicTypes.map(type => (<Checkbox
								label={this.descriptions[type]}
								checked={this.contentType && this.contentType.indexOf(type) >= 0}
								onCheck={e => {
									if(e.target.checked)
									{
										this.contentType.push(type)
									}
									else
									{
										const index = this.contentType.indexOf(type)
										this.contentType.splice(index, 1)
									}
									if(this.contentType.length == 0)
										this.contentType = this.basicTypes.slice(0)

									this.forceUpdate()
								}}
							/>))
						}
					</div>

					{
						this.toRemoveProbably && this.toRemoveProbably > 0
							?
							<div style={{color: 'orange'}}>{__('Torrents to clean')}: {this.toRemoveProbably}</div>
							:
							null
					}
					{
						this.toRemove && this.toRemove > 0
							?
							<div style={{color: 'red'}}>{__('Torrents cleaned')}: {this.toRemove}</div>
							:
							null
					}
					{
						this.removed > 0 && !this.toRemove && !this.toRemoveProbably
							?
							<div style={{color: 'purple'}}>{this.realRemove ? __('removing') : __('calculation')}...: {this.removed}{this.removedMax > 0 ? '/' + this.removedMax : ''}</div>
							:
							null
					}

					{
						this.settingsSavedMessage
          &&
          <div style={{color: 'green'}}>{__('Settings saved')}</div>
					}

					<div className='row center pad0-75'>
						<RaisedButton label={__('Check torrents')} primary={true} onClick={() => {
							this.toRemoveProbably = null
							this.toRemove = null
							window.torrentSocket.emit('removeTorrents', {checkOnly: true}, window.customLoader((toRemove) => {
								this.toRemoveProbably = toRemove
								this.forceUpdate()
							}));
						}} />
						<RaisedButton label={__('Clean torrents')} secondary={true} onClick={() => {
							this.toRemoveProbably = null
							this.toRemove = null
							window.torrentSocket.emit('removeTorrents', {checkOnly: false}, window.customLoader((toRemove) => {
								this.toRemove = toRemove
								this.forceUpdate()
							}));
						}} />
					</div>

					<div className='row center pad0-75'>
						<RaisedButton label={__('Save Settings')} primary={true} onClick={() => {
							this.saveSettings()
						}} />
					</div>

				</div>

			</div>
		);
	}
}
