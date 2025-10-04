let startTime  = process.hrtime()
let startUsage = process.cpuUsage()

let keepTime = process.hrtime()
let keepUsage = process.cpuUsage()
let sw = false

const cpuTimer = setInterval(() => {
	if(!sw) {
		keepTime = process.hrtime();
		keepUsage = process.cpuUsage();
		sw = true;
	} else {
		startTime = keepTime;
		startUsage = keepUsage;
		sw = false;
	} 
}, 500)

cpuTimer.unref()

module.exports = () => {
	function secNSec2ms (secNSec) {
		return secNSec[0] * 1000 + secNSec[1] / 1000000
	}

	var elapTime = process.hrtime(startTime)
	var elapUsage = process.cpuUsage(startUsage)

	var elapTimeMS = secNSec2ms(elapTime)
	var elapUserMS = elapUsage.user
	var elapSystMS = elapUsage.system

	return Math.round(100 * ((elapUserMS + elapSystMS) / 1000) / elapTimeMS)
}