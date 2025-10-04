import React, { Component } from 'react';
import SelectField from 'material-ui/SelectField';
import MenuItem from 'material-ui/MenuItem';
import InputSize from './input-size';
import FilesFilterInput from './input-files-filter';

export default class AdvancedSearchControl extends Component {
	constructor(props)
	{
		super(props)
		this.state = {
			type: undefined,
			size: {min: 0, max: 0},
			maxSize: 1024 * 1024 * 1024,
			sizeEnabled: false,
			filesEnabled: false,
			files: {min: 0, max: 0},
			filesMax: 100,
		}
		if(this.props.state)
			this.state = Object.assign(this.state, this.props.state)
	}
	setState(val)
	{
		super.setState(val, (v) => {
			if(this.props.onChange)
				this.props.onChange(this.state)
		})
	}
	render() {
		return (
			<div className='column w100p' style={{position: 'absolute', top: '70px', zIndex: 3, boxShadow: '0 0 10px rgba(0,0,0,0.5)', maxWidth: 750, overflow: 'hidden', padding: '0px 18px 15px', background: 'white', borderRadius: 3}}>
				<SelectField
					floatingLabelText={__('Filter content type')}
					value={this.state.type}
					onChange={(event, index, value) => this.setState({type: value})}
				>
					<MenuItem value={undefined} primaryText="" />
					<MenuItem value='video' primaryText={__('Video')} />
					<MenuItem value='audio' primaryText={__('Audio')} />
					<MenuItem value='pictures' primaryText={__('Pictures')} />
					<MenuItem value='books' primaryText={__('Books')} />
					<MenuItem value='application' primaryText={__('Applications')} />
					<MenuItem value='archive' primaryText={__('Archives')} />
					<MenuItem value='disc' primaryText={__('Disk Images')} /> 
				</SelectField>
				<div className='w100p'>
					<InputSize value={this.state.size} enabled={this.state.sizeEnabled} maxSize={this.state.maxSize} onChange={({size, maxSize, enabled}) => this.setState({size, maxSize, sizeEnabled: enabled})} />
				</div>
				<div className='w100p'>
					<FilesFilterInput value={this.state.files} filesMax={this.state.filesMax} enabled={this.state.filesEnabled} onChange={({files, filesMax, enabled}) => this.setState({files, filesMax, filesEnabled: enabled})} />
				</div>
			</div>
		);
	}
}
