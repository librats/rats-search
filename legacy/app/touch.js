import ReactDOM from 'react-dom';

let listenSwipe = (component, handlers) =>
{
	let element = ReactDOM.findDOMNode(component),
		startX,
		startY,
		distanceX,
		distanceY,
		threshold = 110,
		allowedTime = 280,
		thresholdAlternativeAxis = 145,
		elapsedTime,
		startTime;
 
	let touchFunctions = {
		touchstart : (e) => {
			let touchObject = e.changedTouches[0]
           
			distanceX = 0;
			distanceY = 0;
            
			startX = touchObject.pageX
			startY = touchObject.pageY
            
			startTime = new Date().getTime()
            
			if(handlers && handlers.preventDefault)
				e.preventDefault();

			if(handlers && handlers.initSwipe)
				handlers.initSwipe.call(component);
		},
		touchmove : (e) => {
			if(handlers && handlers.preventDefault)
				e.preventDefault();
		},
		touchend : (e) => {
			let touchObject = e.changedTouches[0];

			distanceX = touchObject.pageX - startX;
			distanceY = touchObject.pageY - startY;

			elapsedTime = new Date().getTime() - startTime; // get time elapsed
            
			let params = {
				startX,
				startY,
				endX: touchObject.pageX,
				endY: touchObject.pageY,
				distanceX,
				distanceY
			};

			if (elapsedTime <= allowedTime)
			{
				if(distanceX >= threshold && Math.abs(distanceY) <= thresholdAlternativeAxis)
				{
					if(handlers && handlers.left)
						handlers.left.call(component, params);
				} else 
				if(-distanceX >= threshold && Math.abs(distanceY) <= thresholdAlternativeAxis) {
					if(handlers && handlers.right)
						handlers.right.call(component, params);
				} else 
				if(distanceY >= threshold && Math.abs(distanceX) <= thresholdAlternativeAxis) {
					if(handlers && handlers.top)
						handlers.top.call(component, params);
				} else 
				if(-distanceY >= threshold && Math.abs(distanceX) <= thresholdAlternativeAxis) {
					if(handlers && handlers.bottom)
						handlers.bottom.call(component, params);
				}
			}

			if(handlers && handlers.preventDefault)
				e.preventDefault();
		}
	};

	element.addEventListener('touchstart', touchFunctions.touchstart, false);
	element.addEventListener('touchmove', touchFunctions.touchmove, false);
	element.addEventListener('touchend', touchFunctions.touchend, false);

	return touchFunctions;
}

let removeSwipeListener = (component, touchFunctions) => {
	let element = ReactDOM.findDOMNode(component);
	element.removeEventListener('touchstart', touchFunctions.touchstart);
	element.removeEventListener('touchmove', touchFunctions.touchmove);
	element.removeEventListener('touchend', touchFunctions.touchend);
}

export { listenSwipe, removeSwipeListener }