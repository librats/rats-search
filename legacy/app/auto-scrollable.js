import React, { Component } from 'react';
import ReactDOM from 'react-dom';

export default class AutoScrollable extends Component {
	componentDidMount() {
        if(this.onBottomScroll)
		{
            let component = this;
			let prevScrollValue = 0;
			let outFireStart = 0;
			this.onScroll = () => {
				let scrollHeight = Math.max(
                    document.body.scrollHeight, document.documentElement.scrollHeight,
                    document.body.offsetHeight, document.documentElement.offsetHeight,
                    document.body.clientHeight, document.documentElement.clientHeight
                );

				if(prevScrollValue != scrollHeight)
				{
					prevScrollValue = scrollHeight;
					outFireStart = scrollHeight / 6; // это значит что-то подгрузило на страницу и можно вполне увеличивать отступ скрола
				}

                if ((window.innerHeight + document.documentElement.scrollTop) >= scrollHeight - outFireStart) {
					outFireStart = -1;
			      	component.onBottomScroll();
			    }
			};
			window.addEventListener('scroll', this.onScroll);
		}
	}
	componentWillUnmount() {
		if(this.onBottomScroll)
		{
			window.removeEventListener('scroll', this.onScroll);
		}
	}
}
