const config = require('./webpack.config.production');
const webpack = require('webpack');

let compiler = webpack(config);
compiler.run((err, stats) => {
	if(err)
		throw new Error(err)
    
    
	if(stats.compilation.errors && stats.compilation.errors.length > 0)
		console.error('compilation errors', stats.compilation.errors)
	else
		console.log('succesfully builded web version')
})
