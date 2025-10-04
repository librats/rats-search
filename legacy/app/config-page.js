import React from 'react';
import Page from './page';

import Toggle from 'material-ui/Toggle';
import RaisedButton from 'material-ui/RaisedButton';
import TextField from 'material-ui/TextField'
import Slider from 'material-ui/Slider'
import {Tabs, Tab} from 'material-ui/Tabs';
import SvgIcon from 'material-ui/SvgIcon';

import fs from 'fs'
let dialog
if(typeof WEB === 'undefined')
	dialog = require('@electron/remote').dialog

export default class ConfigPage extends Page {
	constructor(props) {
		super(props)
		this.setTitle('Rats settings');
		this.options = {}
		this.state = {tabIndex: 'main'}
	}
	componentDidMount() {
		this.loadSettings()
	}
	loadSettings() {
		window.torrentSocket.emit('config', window.customLoader((options) => {
			this.options = options;
			this.forceUpdate();
		}));
	}
	saveSettings() {
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
				<Tabs value={this.state.tabIndex} onChange={(index) => this.setState({tabIndex: index})}>
					<Tab value='back' label={__('Back to main page')} onActive={() => {
						window.router('/')
					}} />
					<Tab value='main' label={__("Main settings")} icon={<SvgIcon viewBox="0 0 264.725 264.725">
						<path d="M220.195,71.427c-0.589-7.654-9.135-15.619-17.979-16.209c-8.844-0.584-17.398,0.301-12.087,6.483
                            c5.308,6.188,7.074,12.091,4.423,11.212c-2.66-0.896-13.267-7.08-45.104-2.066c-4.126,1.17-21.221-12.682-44.513-12.977
                            c-23.283-0.295-40.381,6.346-64.85,72.296c-2.356,5.828-18.866,19.386-27.71,25.865C3.536,162.529,0.007,169.787,0,182.763
                            c-0.018,18.158,25.934,27.187,81.648,26.889c55.715-0.292,85.195-9.388,85.195-9.388c-62.789,6.773-158.907,10.52-158.907-18.687
                            c0-20.641,28.321-28.47,36.281-28.184c7.958,0.3,13.562,12.673,33.307,5.603c3.247-0.295,1.48,4.423-1.18,7.369
                            c-2.651,2.942-0.586,6.487,9.73,6.487c10.315,0,41.183,0.295,47.707,0c6.531-0.299,11.839-11.792-9.384-12.68
                            c-18.548,0.311,12.023-5.773,15.915-21.813c0.709-3.927,8.84-4.139,15.918-4.119c20.777,0.029,34.485,38.193,38.912,38.338
                            c4.416,0.15,17.979,1.621,17.683-4.273c-0.292-5.897-11.491-3.241-13.854-6.487c-2.359-3.234-10.023-15.504-7.366-21.104
                            c2.65-5.59,12.674-21.229,24.463-22.988c11.789-1.777,42.451,7.361,47.459,0c5.012-7.372-6.783-11.512-15.918-28.611
                            C243.779,80.572,238.768,71.728,220.195,71.427z"/>
					</SvgIcon>
					}>
						<div className='column center w100p pad0-75'>
							<Toggle
								style={{marginTop: '10px'}}
								label={__('Enabled network scanning')}
								toggled={this.options.indexer}
								onToggle={(e, checked) => {
									this.options.indexer = checked
									if(!this.options.indexer)
										this.options.p2p = false
									this.forceUpdate()
								}}
							/>

							<div className='column w100p'>
								<div className='row inline w100p'>
									<div style={{flex: 1}}>{__('Scanning port')}</div>
									<TextField
										style={{width: 65}}
										hintText={__('Port')}
										errorText={this.options.spiderPort > 0 ? undefined : __('This field is required')}
										value={this.options.spiderPort}
										onChange={(e, value) => {
											if(!value)
												value = 0
											if(value > 65535)
												value = 65535

											this.options.spiderPort = parseInt(value)
											this.forceUpdate()
										}}
									/>
								</div>
								<div className='fs0-75' style={{color: 'grey'}}>* {__('For current work TCP and UDP ports must be fully open and forward in case of router usage')}</div>
							</div>

							<div className='column w100p'>
								<div className='row inline w100p'>
									<div style={{flex: 1}}>{__('Trackers responce port')}</div>
									<TextField
										style={{width: 65}}
										hintText="Port"
										errorText={this.options.udpTrackersPort > 0 ? undefined : __('This field is required')}
										value={this.options.udpTrackersPort}
										onChange={(e, value) => {
											if(!value)
												value = 0
											if(value > 65535)
												value = 65535

											this.options.udpTrackersPort = parseInt(value)
											this.forceUpdate()
										}}
									/>
								</div>
								<div className='fs0-75' style={{color: 'grey'}}>* {__('For current work UDP port must be fully open and forward in case of router usage')}</div>
							</div>

							<Toggle
								style={{marginTop: '10px'}}
								label={__('Enabled UPnP')}
								toggled={this.options.upnp}
								onToggle={(e, checked) => {
									this.options.upnp = checked
									this.forceUpdate()
								}}
							/>

							<div className='row inline w100p'>
								<div style={{flex: 1}}>{__('Collection directory')}</div>
								<TextField
									hintText={__('Db path')}
									errorText={this.options.dbPath && this.options.dbPath.length > 0 ? undefined : __('This field is required')}
									value={this.options.dbPath}
									onChange={(e, value) => {
										if(!fs.existsSync(value))
											return

										this.options.dbPath = value
										this.forceUpdate()
									}}
								/>
								<RaisedButton style={{marginLeft: 20}} label={__('Browse')} primary={true} onClick={() => {
									if(!dialog)
										return
									const dir = dialog.showOpenDialogSync({properties: ['openDirectory']})[0]
									if(dir)
									{
										this.options.dbPath = dir
										this.forceUpdate()
									}
								}} />
							</div>

							<div className='row inline w100p'>
								<div style={{flex: 1}}>{__('Download torrents directory')}</div>
								<TextField
									hintText={__('Download path')}
									value={this.options.client && this.options.client.downloadPath}
									onChange={(e, value) => {
										if(!fs.existsSync(value))
											return

										this.options.client.downloadPath = value
										this.forceUpdate()
									}}
								/>
								<RaisedButton style={{marginLeft: 20}} label={__('Browse')} primary={true} onClick={() => {
									if(!dialog)
										return
									const dir = dialog.showOpenDialogSync({properties: ['openDirectory']})[0]
									if(dir)
									{
										this.options.client.downloadPath = dir
										this.forceUpdate()
									}
								}} />
							</div>

							<Toggle
								style={{marginTop: '10px'}}
								label={__('Hide to tray on application minimize')}
								toggled={this.options.trayOnMinimize}
								onToggle={(e, checked) => {
									this.options.trayOnMinimize = checked
									this.forceUpdate()
								}}
							/>

							<Toggle
								style={{marginTop: '10px'}}
								label={__('Hide to tray on application close')}
								toggled={this.options.trayOnClose}
								onToggle={(e, checked) => {
									this.options.trayOnClose = checked
									this.forceUpdate()
								}}
							/>

							<Toggle
								style={{marginTop: '10px'}}
								label={__('Start application minimized')}
								toggled={this.options.startMinimized}
								onToggle={(e, checked) => {
									this.options.startMinimized = checked
									this.forceUpdate()
								}}
							/>

							<Toggle
								style={{marginTop: '10px'}}
								label={__('Dark mode theme')}
								toggled={this.options.darkMode}
								onToggle={(e, checked) => {
									this.options.darkMode = checked
									this.forceUpdate()
								}}
							/>

							<Toggle
								style={{marginTop: '10px'}}
								label={__('REST API support')}
								toggled={this.options.restApi}
								onToggle={(e, checked) => {
									this.options.restApi = checked
									this.forceUpdate()
								}}
							/>

						</div>
					</Tab>
					<Tab value='p2p' label={__("P2P settings")} icon={<SvgIcon viewBox="0 0 47 47">
						<g>
							<path d="M17.567,15.938l-2.859-2.702c0.333-0.605,0.539-1.29,0.539-2.029c0-2.342-1.897-4.239-4.24-4.239
                        c-2.343,0-4.243,1.896-4.243,4.239c0,2.343,1.9,4.241,4.243,4.241c0.826,0,1.59-0.246,2.242-0.654l2.855,2.699
                        C16.536,16.922,17.023,16.399,17.567,15.938z"/>
							<path d="M29.66,15.6l3.799-6.393c0.374,0.107,0.762,0.184,1.169,0.184c2.347,0,4.244-1.898,4.244-4.241
                        c0-2.342-1.897-4.239-4.244-4.239c-2.343,0-4.239,1.896-4.239,4.239c0,1.163,0.469,2.214,1.227,2.981l-3.787,6.375
                        C28.48,14.801,29.094,15.169,29.66,15.6z"/>
							<path d="M42.762,20.952c-1.824,0-3.369,1.159-3.968,2.775l-5.278-0.521c0,0.04,0.006,0.078,0.006,0.117
                        c0,0.688-0.076,1.36-0.213,2.009l5.276,0.521c0.319,2.024,2.062,3.576,4.177,3.576c2.342,0,4.238-1.896,4.238-4.238
                        C47,22.85,45.104,20.952,42.762,20.952z"/>
							<path d="M28.197,37.624l-1.18-5.156c-0.666,0.232-1.359,0.398-2.082,0.481l1.182,5.157c-1.355,0.709-2.29,2.11-2.29,3.746
                        c0,2.342,1.896,4.237,4.243,4.237c2.342,0,4.238-1.896,4.238-4.237C32.311,39.553,30.479,37.692,28.197,37.624z"/>
							<path d="M14.357,25.37l-6.57,2.201c-0.758-1.158-2.063-1.926-3.548-1.926C1.896,25.645,0,27.542,0,29.884
                        c0,2.345,1.896,4.242,4.239,4.242c2.341,0,4.242-1.897,4.242-4.242c0-0.098-0.021-0.188-0.029-0.284l6.591-2.207
                        C14.746,26.752,14.51,26.077,14.357,25.37z"/>
							<circle cx="23.83" cy="23.323" r="7.271"/>
						</g>
					</SvgIcon>}>
						<div className='column center w100p pad0-75'>

							<div style={{marginTop: 10}}>{__('P2P Rats network settings')}:</div>

							<Toggle
								style={{marginTop: '10px'}}
								label={__('Enabled p2p search')}
								toggled={this.options.p2p}
								onToggle={(e, checked) => {
									this.options.p2p = this.options.indexer && checked
									this.forceUpdate()
								}}
							/>
							<div className='column w100p'>
								<Toggle
									style={{marginTop: '10px'}}
									label={__('Enabled bootstrap peers')}
									toggled={this.options.p2pBootstrap}
									onToggle={(e, checked) => {
										this.options.p2pBootstrap = checked
										this.forceUpdate()
									}}
								/>
								<div className='fs0-75' style={{color: 'grey'}}>* {__('Use extrnral bootstrap nodes to get p2p peers when network setted wrong or need external source')}</div>
							</div>
							<div className='column w100p'>
								<div className='row inline w100p'>
									<div style={{flex: 1}}>{__('Max peers limit')} ({__('current')}: {this.options.p2pConnections})</div>
									<Slider
										min={10}
										max={25}
										step={1}
										style={{width: 300}}
										value={this.options.p2pConnections}
										onChange={(event, value) => {
											this.options.p2pConnections = value
											this.forceUpdate()
										}}
									/>
								</div>
							</div>
							<div className='column w100p'>
								<Toggle
									style={{marginTop: '10px'}}
									label={__('P2P torrents replication server')}
									toggled={this.options.p2pReplicationServer}
									onToggle={(e, checked) => {
										this.options.p2pReplicationServer = checked
										if(!checked)
											this.options.p2pReplication = false
										this.forceUpdate()
									}}
								/>
								<div className='fs0-75' style={{color: 'grey'}}>* {__('Enable torrents replication server for other rats clients (required for replication)')}.</div>
							</div>
							<div className='column w100p'>
								<Toggle
									style={{marginTop: '10px'}}
									label={__('P2P torrents replication')}
									toggled={this.options.p2pReplication}
									onToggle={(e, checked) => {
										this.options.p2pReplication = checked
										if(checked)
											this.options.p2pReplicationServer = true
										this.forceUpdate()
									}}
								/>
								<div className='fs0-75' style={{color: 'grey'}}>* {__('Enable torrents replication from another rats clients. Dont recomended if torrent scanner works correct')}.</div>
							</div>

						</div>
					</Tab>
					<Tab value='performance' label={__("Performance settings")} icon={
						<SvgIcon viewBox="0 0 477.578 477.578">
							<g>
								<g>
									<path d="M471.743,207.281l-28.773-8.078c-1.842-9.613-4.367-19.082-7.555-28.336l20.82-21.32c2.292-2.351,2.914-5.864,1.57-8.86
                                    c-6.727-14.998-14.976-29.266-24.618-42.578c-1.918-2.668-5.28-3.882-8.461-3.055l-28.859,7.391
                                    c-6.416-7.385-13.342-14.31-20.726-20.726l7.391-28.859c0.813-3.18-0.398-6.534-3.055-8.461
                                    c-13.312-9.645-27.579-17.897-42.578-24.625c-2.994-1.357-6.514-0.733-8.859,1.57l-21.32,20.82
                                    c-9.257-3.187-18.729-5.712-28.344-7.555l-8.079-28.773c-0.969-3.452-4.117-5.837-7.703-5.836h-47.609
                                    c-3.586-0.001-6.734,2.384-7.703,5.836l-8.078,28.773c-9.613,1.842-19.082,4.367-28.336,7.555l-21.32-20.82
                                    c-2.346-2.302-5.865-2.926-8.859-1.57c-14.999,6.726-29.267,14.975-42.579,24.617c-2.657,1.927-3.868,5.281-3.055,8.461
                                    l7.391,28.859c-7.385,6.416-14.311,13.342-20.727,20.727l-28.859-7.391c-3.181-0.834-6.546,0.381-8.461,3.055
                                    c-9.645,13.312-17.897,27.579-24.625,42.578c-1.344,2.996-0.721,6.508,1.57,8.859l20.82,21.32
                                    c-3.187,9.257-5.711,18.729-7.555,28.344l-28.773,8.078c-3.452,0.969-5.837,4.117-5.836,7.703v47.602
                                    c-0.001,3.586,2.384,6.734,5.836,7.703l28.773,8.078c1.842,9.615,4.367,19.087,7.555,28.344l-20.82,21.32
                                    c-2.291,2.351-2.914,5.864-1.57,8.859c6.726,14.999,14.975,29.266,24.617,42.578c1.924,2.661,5.281,3.873,8.461,3.055
                                    l28.859-7.391c6.416,7.385,13.342,14.311,20.727,20.727l-7.391,28.859c-0.813,3.18,0.398,6.534,3.055,8.461
                                    c13.312,9.645,27.579,17.897,42.578,24.625c2.996,1.34,6.506,0.718,8.859-1.57l21.32-20.82c9.257,3.187,18.729,5.711,28.344,7.555
                                    l8.078,28.773c0.969,3.452,4.118,5.837,7.703,5.836h47.602c3.586,0.001,6.734-2.384,7.703-5.836l8.078-28.766
                                    c9.615-1.848,19.087-4.375,28.344-7.562l21.32,20.82c2.351,2.291,5.864,2.914,8.859,1.57
                                    c14.998-6.727,29.266-14.976,42.578-24.618c2.657-1.927,3.868-5.281,3.055-8.461l-7.391-28.859
                                    c7.385-6.416,14.31-13.342,20.726-20.726l28.859,7.391c3.18,0.818,6.537-0.394,8.461-3.055
                                    c9.646-13.312,17.897-27.58,24.625-42.579c1.344-2.996,0.721-6.508-1.57-8.859l-20.82-21.32
                                    c3.187-9.257,5.711-18.729,7.555-28.344l28.773-8.078c3.452-0.969,5.837-4.117,5.836-7.703v-47.609
                                    C477.58,211.399,475.195,208.25,471.743,207.281z M461.578,256.531l-27.758,7.797c-2.985,0.837-5.217,3.325-5.727,6.383
                                    c-2.001,12.012-5.146,23.805-9.391,35.219c-1.087,2.9-0.399,6.168,1.766,8.383l20.461,20.953
                                    c-5.058,10.576-10.935,20.739-17.578,30.398l-28.375-7.266c-3.002-0.781-6.183,0.256-8.148,2.656
                                    c-7.76,9.386-16.392,18.016-25.781,25.773c-2.388,1.976-3.423,5.153-2.656,8.156l7.266,28.375
                                    c-9.657,6.644-19.821,12.518-30.398,17.57l-20.954-20.46c-2.212-2.171-5.483-2.86-8.383-1.766
                                    c-11.414,4.245-23.207,7.392-35.219,9.398c-3.057,0.513-5.543,2.743-6.383,5.727l-7.797,27.75h-35.476l-7.797-27.758
                                    c-0.837-2.985-3.325-5.217-6.383-5.727c-12.012-2.001-23.805-5.146-35.219-9.391c-2.899-1.096-6.172-0.407-8.383,1.766
                                    l-20.953,20.461c-10.575-5.058-20.739-10.935-30.398-17.578l7.266-28.375c0.768-3.001-0.267-6.176-2.656-8.148
                                    c-9.386-7.76-18.016-16.392-25.773-25.781c-1.978-2.386-5.153-3.42-8.156-2.656l-28.375,7.266
                                    c-6.641-9.659-12.516-19.823-17.57-30.398l20.46-20.954c2.164-2.216,2.852-5.483,1.766-8.383
                                    c-4.247-11.413-7.391-23.206-9.391-35.219c-0.51-3.058-2.742-5.545-5.727-6.383L16,256.523v-35.477l27.758-7.797
                                    c2.985-0.837,5.217-3.325,5.727-6.383c2.001-12.012,5.146-23.805,9.391-35.219c1.087-2.9,0.399-6.168-1.766-8.383l-20.461-20.953
                                    c5.058-10.576,10.935-20.739,17.578-30.398l28.375,7.266c3.001,0.769,6.177-0.266,8.148-2.656
                                    c7.76-9.386,16.392-18.016,25.781-25.773c2.388-1.976,3.423-5.153,2.656-8.156l-7.266-28.375
                                    c9.659-6.641,19.823-12.516,30.399-17.57l20.953,20.46c2.211,2.172,5.483,2.861,8.383,1.766
                                    c11.412-4.243,23.202-7.388,35.211-9.391c3.058-0.51,5.545-2.742,6.383-5.727L221.047,16h35.484l7.797,27.758
                                    c0.837,2.985,3.325,5.217,6.383,5.727c12.012,2.001,23.805,5.146,35.219,9.391c2.899,1.096,6.172,0.407,8.383-1.766l20.953-20.461
                                    c10.576,5.058,20.739,10.935,30.398,17.578l-7.266,28.375c-0.768,3.001,0.267,6.176,2.656,8.148
                                    c9.386,7.76,18.016,16.392,25.773,25.781c1.97,2.397,5.152,3.433,8.156,2.656l28.375-7.266
                                    c6.644,9.657,12.518,19.821,17.57,30.398l-20.46,20.954c-2.164,2.216-2.852,5.483-1.766,8.383
                                    c4.243,11.412,7.388,23.202,9.391,35.211c0.51,3.058,2.742,5.545,5.727,6.383l27.758,7.797V256.531z"/>
								</g>
								<g>
									<path d="M238.789,78.789c-88.366,0-160,71.634-160,160c0.098,88.325,71.675,159.902,160,160c88.366,0,160-71.634,160-160
                                    S327.155,78.789,238.789,78.789z M345.95,334.638l-16.653-16.653l-11.313,11.313l16.653,16.653
                                    c-24.277,21.798-55.27,34.651-87.849,36.434v-23.595h-16v23.595c-32.578-1.782-63.571-14.636-87.849-36.434l16.653-16.653
                                    l-11.313-11.313l-16.653,16.653c-21.798-24.277-34.652-55.27-36.434-87.849h23.595v-16H95.194
                                    c1.782-32.578,14.636-63.571,36.434-87.849l16.653,16.653l11.313-11.313l-16.653-16.653c24.277-21.798,55.27-34.651,87.849-36.434
                                    v23.595h16V95.194c32.578,1.782,63.571,14.636,87.849,36.434l-16.653,16.653l11.313,11.313l16.653-16.653
                                    c21.798,24.277,34.652,55.27,36.434,87.849h-23.595v16h23.595C380.602,279.367,367.748,310.36,345.95,334.638z"/>
								</g>
								<g>
									<path d="M307.224,168.133c-2.687-1.79-6.186-1.79-8.872,0l-58.062,38.711c-0.5-0.031-0.992-0.055-1.5-0.055
                                    c-17.673,0-32,14.327-32,32s14.327,32,32,32c17.666-0.018,31.982-14.334,32-32c0-0.5-0.023-1-0.055-1.492l38.711-58.07
                                    C311.895,175.55,310.901,170.583,307.224,168.133z M255.781,230.875c-1.094,1.716-1.532,3.768-1.234,5.781
                                    c0.136,0.704,0.217,1.417,0.242,2.133c0,8.837-7.163,16-16,16s-16-7.163-16-16s7.163-16,16-16c0.769,0.03,1.534,0.119,2.289,0.266
                                    c1.966,0.287,3.969-0.161,5.625-1.258l27.234-18.156L255.781,230.875z"/>
								</g>
							</g>
						</SvgIcon>
					}>
						<div className='column center w100p pad0-75'>
                        
							<div className='column w100p'>
								<Toggle
									style={{marginTop: '10px'}}
									label={__('Check torrent files intergrity')}
									toggled={this.options.recheckFilesOnAdding}
									onToggle={(e, checked) => {
										this.options.recheckFilesOnAdding = checked
										this.forceUpdate()
									}}
								/>
								<div className='fs0-75' style={{color: 'grey'}}>* {__('Enable database torrents files intergrity check on adding each torrent. Disable this will free some cpu usage on adding operation.')}</div>
							</div>

							<div style={{marginTop: 10}}>{__('Torrent network scanner settings')}:</div>
                            
							<div className='column w100p'>
								<div className='row inline w100p'>
									<div style={{flex: 1}}>{__('Scanner walk speed')} ({__('current')}: {this.options.spider && this.options.spider.walkInterval}) [{__('affected after program reload')}]</div>
									<Slider
										min={1}
										max={150}
										step={1}
										style={{width: 300}}
										value={this.options.spider && this.options.spider.walkInterval}
										onChange={(event, value) => {
											this.options.spider.walkInterval = value
											this.forceUpdate()
										}}
									/>
								</div>
								<div className='fs0-75' style={{color: 'grey'}}>* {__('Low value')} - {__('fast initial scanning and high cpu usage')}. {__('High Value')} - {__('low cpu usage but very slow scanning')}. 
									{__('Good value between')} 3-60. {__('Defaul value')}: 5</div>
							</div>
							<div className='column w100p'>
								<div className='row inline w100p'>
									<div style={{flex: 1}}>{__('Nodes usage')} ({__('current')}: {this.options.spider && this.options.spider.nodesUsage})</div>
									<Slider
										min={0}
										max={1000}
										step={1}
										style={{width: 300}}
										value={this.options.spider && this.options.spider.nodesUsage}
										onChange={(event, value) => {
											this.options.spider.nodesUsage = value
											this.forceUpdate()
										}}
									/>
								</div>
								<div className='fs0-75' style={{color: 'grey'}}>* {__('Low Value')} - {__('very low usage of nodes, low network traffic, slow torrent scanning')}. {__('High value')} - {__('high traffic, fast scanning, high routers usage')}. 
									{__('Recomended value between')} 10-1000. {__('Defaul value')}: 100. 0 - {__('Ignore this option')} ({__('no limit')}).
								</div>
							</div>
							<div className='column w100p'>
								<div className='row inline w100p'>
									<div style={{flex: 1}}>{__('Reduce network packages')} ({__('current')}: {this.options.spider && this.options.spider.packagesLimit})</div>
									<Slider
										min={0}
										max={2000}
										step={1}
										style={{width: 300}}
										value={this.options.spider && this.options.spider.packagesLimit}
										onChange={(event, value) => {
											this.options.spider.packagesLimit = value
											this.forceUpdate()
										}}
									/>
								</div>
								<div className='fs0-75' style={{color: 'grey'}}>* {__('Low Value')} - {__('ignore more usless network packages, lower traffic and routers usage')}. {__('High Value')} - {__('high traffic and router usage in prospect')}.
									{__('Recomended value between')} 300-2000. {__('Defaul value')}: 500. 0 - {__('Ignore this option')} ({__('no limit')}).
								</div>
							</div>

						</div>              
					</Tab>
				</Tabs>
				<div className='column center w100p pad0-75'>
					{
						this.settingsSavedMessage
          &&
          <div style={{color: 'green'}}>{__('Settings saved')}</div>
					}

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
