import React, { Component } from 'react';
import { listenSwipe, removeSwipeListener } from './touch'

export default class BTComponent extends Component {
	componentDidMount() {
		// Свайп действия
		if(
			this.props.onSwipeLeft ||
            this.props.onSwipeRight ||
            this.props.onSwipeTop ||
            this.props.onSwipeBottom ||
            this.onSwipeLeft ||
            this.onSwipeRight ||
            this.onSwipeTop ||
            this.onSwipeBottom
		)
		{
			this.swipeFunctions = listenSwipe(this, {
				left: this.props.onSwipeLeft || this.onSwipeLeft,
				right: this.props.onSwipeRight || this.onSwipeRight,
				top: this.props.onSwipeTop || this.onSwipeTop,
				bottom: this.props.onSwipeBottom || this.onSwipeBottom,
				initSwipe: this.props.initSwipe || this.initSwipe,
			});
		}
	}
	componentWillUnmount() {
		if(this.swipeFunctions)
		{
			removeSwipeListener(this, this.swipeFunctions);
		}
	}
}