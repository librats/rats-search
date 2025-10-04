const webpack = require('webpack');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const SWPrecacheWebpackPlugin = require('sw-precache-webpack-plugin');
const glob = require('glob')
const path = require('path')
const CopyWebpackPlugin = require('copy-webpack-plugin')

module.exports = {
	mode: 'development',
	//mode: 'production',
	entry: path.resolve("src/app/index.js"),
	output: {
		path: path.resolve('web'),
	},
	module: {
		rules: [
			{
				test: /\.js$/,
				exclude: /node_modules/,
				use: ["babel-loader"]
			},
			{
				test: /\.css$/,
				use: ["style-loader", "css-loader"]
			},
			{
				test: /\.(?:ico|gif|png|jpg|jpeg|webp)$/,
				use: ['url-loader']
			}
		]
	},
	plugins: [
		new HtmlWebpackPlugin({
			inject: true,
			template: 'app/app.html',
			minify: {
				removeComments: true,
				collapseWhitespace: true,
				removeRedundantAttributes: true,
				useShortDoctype: true,
				removeEmptyAttributes: true,
				removeStyleLinkTypeAttributes: true,
				keepClosingSlash: true,
				minifyJS: true,
				minifyCSS: true,
				minifyURLs: true,
			},
		}),
		new webpack.DefinePlugin({WEB: true}),
		new CopyWebpackPlugin({patterns: ['translations/**']}),
	],
	resolve: {
		fallback: {
			dgram: false,
			fs: false,
			net: false,
			tls: false,
		},
	}
};