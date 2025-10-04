import React, { Component } from 'react';
import InputRange from 'react-input-range';
import './input-range.css';
import formatBytes from './format-bytes'

import SelectField from 'material-ui/SelectField';
import MenuItem from 'material-ui/MenuItem';
import Checkbox from 'material-ui/Checkbox';

export default class InputSize extends Component {
	constructor(props)
	{
		super(props)
		this.state = { 
			size: this.props.value || { min: 0, max: 500 * 1024 },
			enabled: false || this.props.enabled,
			maxSize: this.props.maxSize || 1024 * 1024 // 1mb
		}
	}
	setState(val)
	{
		if(val.maxSize && this.state.size.max > val.maxSize)
			val.size = {min: this.state.size.min, max: val.maxSize};

		if(val.maxSize && this.state.size.min > val.maxSize)
			val.size = {min: 0, max: val.size ? val.size.max || this.state.size.max : this.state.size.max };

		super.setState(val, () => {
			if(this.props.onChange)
				this.props.onChange({
					enabled: this.state.enabled,
					maxSize: this.state.maxSize,
					size: !this.state.enabled ? {min: 0, max: 0} : this.state.size
				})
		})
	}
	render() {
		return (
			<div className='filter-row row inline w100p'>
				<Checkbox
					label={__('Size filter')}
					checked={this.state.enabled}
					style={{width: 150, display: 'flex', minWidth: 130}}
					onCheck={() => this.setState({enabled: !this.state.enabled})}
				/>
				<div className='filter-control-row row inline w100p' style={{opacity: this.state.enabled ? 1 : 0.4, transition: '0.5s', paddingLeft: 9}}>
					<InputRange
						maxValue={this.state.maxSize}
						minValue={0}
						value={this.state.size}
						formatLabel={size => formatBytes(size)}
						style={this.props.style}
						className={this.props.className}
						onChange={size => this.setState({ size })} 
					/>
					<SelectField
						floatingLabelText={__('Size type max')}
						value={this.state.maxSize}
						onChange={(event, index, value) => this.setState({maxSize: value})}
						className='filter-control-border'
					>
						<MenuItem value={1024} primaryText="KB" />
						<MenuItem value={1024 * 1024} primaryText="MB" />
						<MenuItem value={1024 * 1024 * 100} primaryText="100 MB" />
						<MenuItem value={1024 * 1024 * 1024} primaryText="GB" />
						<MenuItem value={10 * 1024 * 1024 * 1024} primaryText="10 GB" />
						<MenuItem value={100 * 1024 * 1024 * 1024} primaryText="100 GB" />
						<MenuItem value={1024 * 1024 * 1024 * 1024} primaryText="TB" />
					</SelectField>
				</div>
			</div>
		);
	}
}
