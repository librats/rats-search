// https://github.com/sindresorhus/is-port-reachable
// Copyright (c) Sindre Sorhus <sindresorhus@gmail.com> (sindresorhus.com)

const net = require('net');

module.exports = (port, opts) => {
	opts = Object.assign({timeout: 3000}, opts);

	return new Promise((resolve => {
		const socket = new net.Socket();

		const onError = () => {
			socket.destroy();
			resolve(false);
		};

		socket.setTimeout(opts.timeout);
		socket.on('error', onError);
		socket.on('timeout', onError);

		socket.connect(port, opts.host, () => {
			socket.end();
			resolve(true);
		});
	}));
};
