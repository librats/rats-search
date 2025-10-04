import LinearProgressOriginal from 'material-ui/LinearProgress';

export default class LinearProgress extends LinearProgressOriginal
{
	barUpdate(id, ...args)
	{
		super.barUpdate(id, ...args)
		return this.timers[id]
	}
}
