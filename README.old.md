# Rats on The Boat - BitTorrent Search Engine

<p align="center"><a href="https://github.com/DEgITx/rats-search"><img src="https://raw.githubusercontent.com/DEgITx/rats-search/master/resources/rat-logo.png"></a></p>

[![CircleCI Build Status](https://circleci.com/gh/DEgITx/rats-search.png?style=shield)](https://circleci.com/gh/DEgITx/rats-search)
[![Appveyor Build Status](https://ci.appveyor.com/api/projects/status/1eh0lug97fboscib?svg=true)](https://ci.appveyor.com/project/DEgITx/rats-search)
[![Travis Build Status](https://travis-ci.org/DEgITx/rats-search.svg?branch=master)](https://travis-ci.org/DEgITx/rats-search)
[![semantic-release](https://img.shields.io/badge/%20%20%F0%9F%93%A6%F0%9F%9A%80-semantic--release-e10079.svg)](https://github.com/semantic-release/semantic-release)
[![Release](https://img.shields.io/github/release/DEgITx/rats-search.svg)](https://github.com/DEgITx/rats-search/releases)
[![Documentation](https://img.shields.io/badge/docs-faq-brightgreen.svg)](https://github.com/DEgITx/rats-search/blob/master/docs/MANUAL.md)

A BitTorrent search program for desktop and web. It collects and allows navigation through torrent statistics, categories, and provides easy access to them. Works over a P2P network and supports Windows, Linux, and macOS platforms.

## Features
* Works over P2P torrent network, doesn't require any trackers
* Supports its own P2P protocol for additional data transfer (search between Rats clients, descriptions/votes transfer, etc.)
* Search over torrent collection
* Torrent and files search
* Search filters (size ranges, files, seeders, etc.)
* Collection filters (regex filters, adult filters)
* Tracker peers scan support
* Integrated torrent client
* Collects only statistical information and doesn't save any internal torrent data
* Supports torrent rating (voting)
* P2P Search protocol: Search in other Rats clients
* Web version (web interface) for servers
* Top list (most common and popular torrents)
* Feed list (Rats clients activity feed)
* Translations: English, Russian, Ukrainian, Chinese, Spanish, French, German, Japanese, Portuguese, Italian, Hindi
* Drag and drop torrents (expand local search database with specific torrents)
* Description association from trackers
* Torrent generation and automatic adding to search database
* [WebSockets & REST API for server/search engine. You can make search requests and create your own UI client.](docs/API.md)

## Architecture
![Basic Architecture](docs/img/ratsarch.png)

## Contributing
We welcome all contributions: bug fixes, improvements, code refactoring, and other enhancements.

[Translation Guide](docs/TRANSLATION.md)

[Own Strategy / Other Tracker Support](docs/TRACKERS.md)

## Usage Manuals
* [English](docs/USAGE.md)
* [Russian](docs/USAGE.RU.md)

## Download Desktop Client
We recommend using the desktop version of the application. [<b>Download and install the most recent version</b>](https://github.com/DEgITx/rats-search/releases) for Windows, macOS, or Linux.

### Start Desktop Client from Master Branch
Clone repository with submodules:
```bash
git clone --recurse-submodules https://github.com/DEgITx/rats-search.git
```

Make sure you have Node.js and NPM installed on your system, then install all required packages:

```bash
npm install --force
```
Start the master development branch version:
```bash
npm start
```

## Server WebUI Version Installation
Besides the default desktop cross-platform client, there's also a separate WebUI that can run from a server independently as a web-client + Node.js server.

Clone the repository, make sure you have Node.js and NPM installed on your system, then:

```bash
npm install --force
```

Compile the web version:

```bash
npm run buildweb
```

Start the server application:

```bash
npm run server
```

You can now access the web interface on port 8095: http://localhost:8095

[More about configuration](docs/SERVER.md)

[More about server compatibility and known issues](docs/SERVER_COMPATIBILITY.md)

[API usage implementation for clients](docs/API.md)

## Docker Image

You can easily run a Docker image with the prepared server version. First, download the latest sources:

```bash
git clone --recurse-submodules https://github.com/DEgITx/rats-search.git
```

Build and run the Docker image:

```bash
docker build -t rats-search:latest rats-search
docker run -p 8095:8095 rats-search:latest
```

Now you can open http://localhost:8095/ in your browser

## Donation

[**Support Rats Search development on OpenCollective**](https://opencollective.com/RatsSearch)

## Contacts / Support

- Twitter: [@RatsSearch](https://twitter.com/RatsSearch)
- [Discord (Support)](https://discord.gg/t9GQtxA)

## Screenshots

![Main Window](docs/img/screen_1.png)

## License
[MIT](https://github.com/DEgITx/rats-search/blob/master/LICENSE)
