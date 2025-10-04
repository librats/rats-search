import React, { Component } from 'react';
import singleton from './singleton'

class PagesPie extends Component {
    pie = [];

    open(pages, params) {
    	if (params && params.replace) {
    		if (params.replace === 'all') {
    			this.pie = [];
    		} else
    		if (params.replace === 'last') {
    			this.pie.pop();     
    		}
    		delete params.replace;
    	}
    	if (Array.isArray(pages)) {
    		for (let i in pages) {
    			this.pie.push({
    				Page: pages[i],
    				params: params
    			});             
    		}
    	} else {
    		this.pie.push({
    			Page: pages,
    			params: params
    		});
    	}
    	if(this._mounted)
    		this.forceUpdate()
    }
    
    componentWillMount()
    {
    	this._mounted = true
    }

    componentWillUnmount()
    {
    	this._mounted = false
    }

    close(count) {
    	if (count && typeof count === 'number') {
    		for (let i = 0; i < count; i++) {
    			this.pie.pop();
    		}
    	} else {
    		this.pie.pop();         
    	}
    	this.forceUpdate();     
    }
    findOpened(windowType) {
    	for (let i in this.refs) {
    		if(this.refs[i] instanceof windowType)
    			return this.refs[i];
    	}
    }
    // ОТРИСОВКА
    render() {
    	if (this.pie.length > 0) {
    		return (
    			<div
    				className={'pie full-size ' + (this.props.className || '')}
    			>
    				{
    					this.pie.map(({Page, params}, index) => {
    						let focus = false;
    						if (index === this.pie.length - 1) {
    							focus = true;
    						}
    						return (
    							<Page
    								focused={focus}
    								closeHandler={() => { index > 0 ? this.close() : null}}
    								index={index}
    								key={index}
    								ref={index}
    								{...params}
    							>
    							</Page>
    						)
    					})
    				}
    			</div>
    		)
    	} else {
    		return null
    	}
    }
}

export default singleton(PagesPie)