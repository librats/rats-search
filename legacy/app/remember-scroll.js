let scrollLock = false

export default () => {
	if(scrollLock)
		return
	scrollLock = true
	// set scroll to prev position
	if(window.rememberYOffset)
	{
		console.log('scroll back')
		setTimeout(() => {
			window.scrollTo(0, window.rememberYOffset + (window.rememberYOffset > 15 ? 330 : 0))
			delete window.rememberYOffset
			scrollLock = false
		}, 10);
	}
	else
	{
		scrollLock = false
	}
}