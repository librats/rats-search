const dgram = require('dgram');
const server = dgram.createSocket("udp4")
const config = require('../config');
const debug = require('debug')('peers-scrape');

const ACTION_CONNECT = 0
const ACTION_ANNOUNCE = 1
const ACTION_SCRAPE = 2
const ACTION_ERROR = 3

const connectionIdHigh = 0x417
const connectionIdLow = 0x27101980
const requests = {};

let message = function (buf, host, port) {
	server.send(buf, 0, buf.length, port, host, function(err, bytes) {
		if (err) {
			logT('udp-tracker', err.message);
		}
	});
};

let connectTracker = function(connection) {
	debug('start screape connection');
	let buffer = new Buffer(16);

	const transactionId = Math.floor((Math.random() * 100000) + 1);

	buffer.fill(0);

	buffer.writeUInt32BE(connectionIdHigh, 0);
	buffer.writeUInt32BE(connectionIdLow, 4);
	buffer.writeUInt32BE(ACTION_CONNECT, 8);
	buffer.writeUInt32BE(transactionId, 12);

	// очистка старых соединений
	for(const transaction in requests) {
		if((new Date).getTime() - requests[transaction].date.getTime() > config.udpTrackersTimeout) {
			delete requests[transaction];
		}
	}

	requests[transactionId] = connection;
	message(buffer, connection.host, connection.port);
};

let scrapeTorrent = function (connectionIdHigh, connectionIdLow, transactionId) {
	let connection = requests[transactionId];
	if(!connection)
		return;

	if(!connection.hash || connection.hash.length != 40)
		return

	debug('start scrape');
	let buffer = new Buffer(56)

	buffer.fill(0);

	buffer.writeUInt32BE(connectionIdHigh, 0);
	buffer.writeUInt32BE(connectionIdLow, 4);
	buffer.writeUInt32BE(ACTION_SCRAPE, 8);
	buffer.writeUInt32BE(transactionId, 12);

	try
	{
		buffer.write(connection.hash, 16, buffer.length, 'hex');
		// do scrape
		message(buffer, connection.host, connection.port);
	} catch(error)
	{
		logT('udp-tracker', 'ERROR on scrape', error)
	}
};

server.on("message", function (msg, rinfo) {
	let buffer = new Buffer(msg)

	const action = buffer.readUInt32BE(0, 4);
	const transactionId = buffer.readUInt32BE(4, 4);
    
	if(!(transactionId in requests))
		return;

	debug("returned action: " + action);
	debug("returned transactionId: " + transactionId);

	if (action === ACTION_CONNECT) {
		debug("connect response");

		let connectionIdHigh = buffer.readUInt32BE(8, 4);
		let connectionIdLow = buffer.readUInt32BE(12, 4);

		scrapeTorrent(connectionIdHigh, connectionIdLow, transactionId);

	} else if (action === ACTION_SCRAPE) {
		debug("scrape response");

		let seeders = buffer.readUInt32BE(8, 4);
		let completed = buffer.readUInt32BE(12, 4);
		let leechers = buffer.readUInt32BE(16, 4);

		let connection = requests[transactionId];
		connection.callback({
			host: connection.host,
			port: connection.port,
			hash: connection.hash,
			seeders,
			completed,
			leechers
		})
		delete requests[transactionId];
	} else if (action === ACTION_ERROR) {
		delete requests[transactionId];
		logT('udp-tracker', "error in scrape response");
	}
});

let getPeersStatistic = (host, port, hash, callback) => {
	let connection = {
		host, port, hash, callback, date: new Date()
	}
	connectTracker(connection);
}

server.on("listening", function () {
	var address = server.address();
	logT('udp-tracker', "listening udp tracker respose on " + address.address + ":" + address.port);
});

server.bind(config.udpTrackersPort);

module.exports = getPeersStatistic;

//getPeersStatistic('tracker.glotorrents.com', 6969, "d096ff66557a5ea7030680967610e38b37434ea8", (data) => {
//    console.log(data)
//});

