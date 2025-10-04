import React, {Component} from 'react';
import {List, ListItem} from 'material-ui/List';

export default class ContextMenu extends Component {
	constructor(props) {
		super(props)
		this.state = {toggle: false}
	}
	render()
	{
		return (
			<div style={{position: 'relative', display: 'inline-block', zIndex: this.props.style?.zIndex || null}}>
				<div style={{display: 'inline-block', zIndex: this.props.style?.zIndex || null}} onClick={(e) => {
					this.setState({toggle: !this.state.toggle})
					if(this.props.onClick)
						this.props.onClick(e)
				}}>
					{this.props.children}
				</div>
				{
					this.state.toggle
                    &&
                    <div style={{width: '100%', height: '100%', position: 'fixed', top: 0, left: 0, zIndex: 2}} onClick={(e) => { 
                    	this.setState({toggle: !this.state.toggle}) 
                    	e.preventDefault()
                    	e.stopPropagation()
                    }}><span></span></div>
				}
				{
					this.state.toggle
                &&
                <List className='context-menu' style={Object.assign({
                	position: 'absolute', 
                	maxWidth: 350, 
                	minWidth: 200, 
                	top: -20,
                	backgroundColor: '#ffffff',
                	boxShadow: '0 0 10px rgba(0,0,0,0.45)',
                	borderRadius: 6,
                	zIndex: 3
                }, !this.props.rightAlign ? { left: -30 } : { right: -30 })}>
                	{
                		this.props.menu && this.props.menu.map((menu, index) => <ListItem key={index} className={menu.className} style={{fontSize: '0.9em'}} primaryText={menu.name} leftIcon={menu.icon} onClick={(e) => {
                			menu.click()
                			this.setState({toggle: !this.state.toggle})
                			e.preventDefault()
                			e.stopPropagation()
                		}} />)
                	}
                </List>
				}
			</div>
		)
	}
}