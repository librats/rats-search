import React from 'react';

export default (props) => {
	let className = 'tooltiptext'
	if(props.right)
		className += ' right'
	return (
		<div className='tooltip'>
			{props.children}
			<span className={className}>{props.hint}</span>
		</div>
	)
}