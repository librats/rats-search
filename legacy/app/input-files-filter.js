import React, { Component } from 'react';
import InputRange from 'react-input-range';
import './input-range.css';
import formatBytes from './format-bytes'

import SelectField from 'material-ui/SelectField';
import MenuItem from 'material-ui/MenuItem';
import Checkbox from 'material-ui/Checkbox';

export default class InputFilesFilter extends Component {
	constructor(props)
	{
		super(props)
		this.state = { 
			files: this.props.value || { min: 0, max: 50 },
			enabled: false || this.props.enabled,
			filesMax: this.props.filesMax || 100 // 1mb
		}
	}
	setState(val)
	{
		if(val.filesMax && this.state.files.max > val.filesMax)
			val.files = {min: this.state.files.min, max: val.filesMax};

		if(val.filesMax && this.state.files.min > val.filesMax)
			val.files = {min: 0, max: val.files ? val.files.max || this.state.files.max : this.state.files.max };

		super.setState(val, () => {
			if(this.props.onChange)
				this.props.onChange({
					enabled: this.state.enabled,
					filesMax: this.state.filesMax,
					files: !this.state.enabled ? {min: 0, max: 0} : this.state.files
				})
		})
	}
	render() {
		return (
			<div className='filter-row row inline w100p'>
				<Checkbox
					label={__("Files filter")}
					checked={this.state.enabled}
					style={{width: 150, display: 'flex', minWidth: 130}}
					onCheck={() => this.setState({enabled: !this.state.enabled})}
				/>
				<div className='filter-control-row row inline w100p' style={{opacity: this.state.enabled ? 1 : 0.4, transition: '0.5s', paddingLeft: 9}}>
					<InputRange
						maxValue={this.state.filesMax}
						minValue={0}
						value={this.state.files}
						style={this.props.style}
						className={this.props.className}
						onChange={files => this.setState({ files })} 
					/>
					<SelectField
						floatingLabelText={__('Size type')}
						value={this.state.filesMax}
						onChange={(event, index, value) => this.setState({filesMax: value})}
						className='filter-control-border'
					>
						<MenuItem value={10} primaryText={"10 " + __("Files or less")} />
						<MenuItem value={100} primaryText={"100 " + __("Files or less")} />
						<MenuItem value={1000} primaryText={"1000 " + __("Files or less")} />
						<MenuItem value={10000} primaryText={"10000 " + __("Files or less")} />
						<MenuItem value={100000} primaryText={"100000 " + __("Files or less")} />
						<MenuItem value={1000000} primaryText={"1000000 " + __("Files or less")} />
					</SelectField>
				</div>
			</div>
		);
	}
}
