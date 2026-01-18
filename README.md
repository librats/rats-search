# Rats on The Boat - BitTorrent Search Engine

<p align="center"><a href="https://github.com/DEgITx/rats-search"><img src="https://raw.githubusercontent.com/DEgITx/rats-search/master/resources/rat-logo.png"></a></p>

[![GitHub Actions Build](https://github.com/DEgITx/rats-search/actions/workflows/build.yml/badge.svg)](https://github.com/DEgITx/rats-search/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/release/DEgITx/rats-search.svg)](https://github.com/DEgITx/rats-search/releases)
[![Documentation](https://img.shields.io/badge/docs-faq-brightgreen.svg)](https://github.com/DEgITx/rats-search/blob/master/docs/MANUAL.md)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A high-performance BitTorrent search program for desktop. It collects and indexes torrents from the DHT network, allowing powerful full-text search through torrent statistics and categories. Works over a P2P network and supports Windows, Linux, and macOS platforms.

## 🚀 Version 2.0 — Now Available!

Rats Search 2.0 is a **complete rewrite in C++/Qt**, delivering a native desktop application with significantly improved performance and efficiency.

### What's New in 2.0

* **Native C++/Qt Application** — Fast, responsive, and lightweight desktop client
* **Powered by [librats](https://github.com/DEgITx/librats)** — High-performance P2P networking core library
* **Improved Resource Efficiency** — Lower memory footprint and CPU usage compared to Electron
* **Modern Dark UI** — Beautiful, modern interface with smooth animations
* **Full-Text Search** — Powered by Manticore Search for blazing-fast queries
* **REST/WebSocket API** — Built-in API server for integrations and custom clients
* **Cross-Platform** — Native builds for Windows, Linux, and macOS

### Legacy Version (1.x)

The previous Electron/Node.js version is available in the `legacy` folder and can still be used for the web interface. See [Legacy Version](#legacy-version-1x-electronnodejs) section below.

## Features

### Core Search & Indexing
* Works over P2P torrent network, doesn't require any trackers
* DHT crawling and automatic torrent indexing
* Full-text search over torrent collection (powered by Manticore)
* Torrent and files search
* Search filters (size ranges, files, seeders, etc.)
* Collection filters (regex filters, adult filters)
* Tracker peers scan support
* Collects only statistical information and doesn't save any internal torrent data

### P2P Network
* Supports its own P2P protocol for additional data transfer (search between Rats clients, descriptions/votes transfer, etc.)
* P2P Search protocol: Search in other Rats clients
* Supports torrent rating (voting)
* Description association from trackers
* Top list (most common and popular torrents)
* Feed list (Rats clients activity feed)

### Torrent Client
* Integrated torrent client for downloading
* Drag and drop torrents (expand local search database with specific torrents)
* Torrent generation and automatic adding to search database

### User Experience
* System tray support with minimize/close to tray
* Translations: English, Russian, Ukrainian, Chinese, Spanish, French, German, Japanese, Portuguese, Italian, Hindi
* Modern dark UI with customizable settings
* [REST & WebSocket API for custom clients and integrations](docs/API.md)

## Powered by librats

Rats Search 2.0 is built on **[librats](https://github.com/DEgITx/librats)** — a high-performance P2P networking library. Key capabilities inherited from librats:

| Feature | Description |
|---------|-------------|
| **BitTorrent Mainline DHT** | Compatible with the largest distributed hash table network (millions of nodes) |
| **mDNS Discovery** | Automatic local network peer discovery without internet |
| **NAT Traversal** | STUN/ICE support for connecting through firewalls and NAT |
| **Noise Protocol Encryption** | End-to-end encryption with Curve25519 + ChaCha20-Poly1305 |
| **GossipSub Messaging** | Scalable publish-subscribe protocol for P2P communication |
| **Thread-safe Design** | Modern C++17 concurrency with minimal overhead |

For more details, see the [librats documentation](https://github.com/DEgITx/librats).

## Architecture
![Basic Architecture](docs/img/ratsarch.png)

## Download

**[⬇️ Download the latest release](https://github.com/DEgITx/rats-search/releases)** for Windows, Linux, or macOS.

| Platform | Package |
|----------|---------|
| Windows | `RatsSearch-Windows-x64.zip` |
| Linux | `RatsSearch-Linux-x64.AppImage` or `.tar.gz` |
| macOS Intel | `RatsSearch-macOS-Intel.zip` |
| macOS ARM | `RatsSearch-macOS-ARM.zip` |

## Building from Source

### Requirements

* **CMake** 3.16+
* **Qt** 6.9+ (with WebSockets module)
* **C++17** compatible compiler (MSVC, GCC, or Clang)
* **Ninja** (recommended) or Make

### Build Instructions

Clone the repository with submodules:

```bash
git clone --recurse-submodules https://github.com/DEgITx/rats-search.git
cd rats-search
```

Configure and build:

```bash
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The executable will be in `build/bin/`.

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `RATS_SEARCH_BUILD_TESTS` | ON | Build unit tests |
| `RATS_SEARCH_USE_SYSTEM_LIBRATS` | OFF | Use system-installed librats |

## Configuration

After first launch, a configuration file `rats.json` will be created in the data directory:

```json
{
    "p2pPort": 4445,
    "dhtPort": 4446,
    "httpPort": 8095,
    "restApiEnabled": true,
    "indexerEnabled": true,
    "darkMode": true
}
```

| Setting | Description |
|---------|-------------|
| `p2pPort` | Port for P2P communication (TCP/UDP) |
| `dhtPort` | Port for DHT operations (UDP) |
| `httpPort` | Port for REST API server |
| `restApiEnabled` | Enable/disable REST API |
| `indexerEnabled` | Enable/disable DHT indexer |

## API

Rats Search 2.0 includes a built-in REST API server for integrations and custom clients.

[📖 API Documentation](docs/API.md)

## Usage Manuals

* [English](docs/USAGE.md)
* [Russian](docs/USAGE.RU.md)

## Contributing

We welcome all contributions: bug fixes, improvements, code refactoring, and other enhancements.

* [Translation Guide](docs/TRANSLATION.md)
* [Tracker Support](docs/TRACKERS.md)

---

## Legacy Version (1.x — Electron/Node.js)

The previous Electron-based version is preserved in the `legacy/` folder for reference and for running the web server interface.

### Running the Legacy Web Server

```bash
cd legacy
npm install --force
npm run buildweb
npm run server
```

Access the web interface at: http://localhost:8095

### Legacy Docker Image

```bash
docker build -t rats-search:legacy -f Dockerfile .
docker run -p 8095:8095 rats-search:legacy
```

[Legacy Server Configuration](docs/SERVER.md)

[Server Compatibility Notes](docs/SERVER_COMPATIBILITY.md)

---

## Screenshots

![Main Window](docs/img/screen_1.png)

## Support & Donation

[**❤️ Support Rats Search development on OpenCollective**](https://opencollective.com/RatsSearch)

## Contacts

- Twitter/X: [@RatsSearch](https://twitter.com/RatsSearch)
- [Discord Community](https://discord.gg/t9GQtxA)
- [GitHub Issues](https://github.com/DEgITx/rats-search/issues)

## License

[MIT License](https://github.com/DEgITx/rats-search/blob/master/LICENSE) © 2026
