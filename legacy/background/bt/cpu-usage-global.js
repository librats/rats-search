var os = require("os");

//Create function to get CPU information
function cpuAverage() {

	//Initialise sum of idle and time of cores and fetch CPU info
	let totalIdle = 0, totalTick = 0;
	const cpus = os.cpus();

	//Loop through CPU cores
	for(let i = 0, len = cpus.length; i < len; i++) {

		//Select CPU core
		const cpu = cpus[i];

		//Total up the time in the cores tick
		for(const type in cpu.times) {
			totalTick += cpu.times[type];
		}     

		//Total up the idle time of the core
		totalIdle += cpu.times.idle;
	}

	//Return the average Idle and Tick times
	return {idle: totalIdle / cpus.length,  total: totalTick / cpus.length};
}

//Grab first CPU Measure
let startMeasure = cpuAverage();
let percentageCPU = 0

//Set delay for second Measure
const cpuTimer = setInterval(function() { 

	//Grab second Measure
	const endMeasure = cpuAverage(); 

	//Calculate the difference in idle and total time between the measures
	const idleDifference = endMeasure.idle - startMeasure.idle;
	const totalDifference = endMeasure.total - startMeasure.total;

	//Calculate the average percentage CPU usage
	percentageCPU = 100 - ~~(100 * idleDifference / totalDifference);
	startMeasure = endMeasure

}, 300);

cpuTimer.unref()

module.exports = () => percentageCPU