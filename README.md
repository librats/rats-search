# Rats on The Boat - BitTorrent Search Engine

<p align="center"><a href="https://github.com/DEgITx/rats-search"><img src="https://raw.githubusercontent.com/DEgITx/rats-search/master/resources/rat-logo.png"></a></p>

[![GitHub Actions Build](https://github.com/DEgITx/rats-search/actions/workflows/build.yml/badge.svg)](https://github.com/DEgITx/rats-search/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/release/DEgITx/rats-search.svg)](https://github.com/DEgITx/rats-search/releases)
[![Documentation](https://img.shields.io/badge/docs-faq-brightgreen.svg)](https://github.com/DEgITx/rats-search/blob/master/docs/MANUAL.md)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A high-performance BitTorrent search program for desktop and server. It collects and indexes torrents from the DHT network, allowing powerful full-text search through torrent statistics and categories. Works over an encrypted P2P network and supports Windows, Linux, and macOS platforms.

## Features

### Core Search & Indexing
* Works over P2P torrent network, doesn't require any trackers
* DHT crawling and automatic torrent indexing
* Full-text search over torrent collection (powered by Manticore Search)
* Torrent and files search
* Search filters (size ranges, files, seeders, etc.)
* Collection filters (regex filters, adult filters)
* Tracker peers scan support
* Collects only statistical information and doesn't save any internal torrent data

### P2P Network & Security
* Supports its own P2P protocol for additional data transfer (search between Rats clients, descriptions/votes transfer, etc.)
* **End-to-end encryption** with Noise Protocol (Curve25519 + ChaCha20-Poly1305)
* P2P Search protocol: Search in other Rats clients with encrypted communication
* BitTorrent Mainline DHT compatible (millions of nodes)
* mDNS Discovery for automatic local network peer discovery
* NAT Traversal with STUN/ICE support for connecting through firewalls
* GossipSub messaging for scalable publish-subscribe protocol
* Supports torrent rating (voting)
* Description association from trackers
* Top list (most common and popular torrents)
* Feed list (Rats clients activity feed)

### Torrent Client
* Integrated torrent client for downloading
* Drag and drop torrents (expand local search database with specific torrents)
* Torrent generation and automatic adding to search database

### User Experience
* Native C++/Qt application — fast, responsive, and lightweight
* Modern dark UI with customizable settings
* System tray support with minimize/close to tray
* Translations: English, Russian, Ukrainian, Chinese, Spanish, French, German, Japanese, Portuguese, Italian, Hindi
* Console mode for headless server operation
* REST & WebSocket API for custom clients and integrations

## Screenshots

![Rats Search](docs/img/rats_2_1.png)

![Old Version](docs/img/screen_1.png)

## Architecture

![Basic Architecture](docs/img/ratsarch.png)

Rats Search is built on **[librats](https://github.com/DEgITx/librats)** — a high-performance P2P networking library providing:

| Feature | Description |
|---------|-------------|
| **BitTorrent Mainline DHT** | Compatible with the largest distributed hash table network (millions of nodes) |
| **mDNS Discovery** | Automatic local network peer discovery without internet |
| **NAT Traversal** | STUN/ICE support for connecting through firewalls and NAT |
| **Noise Protocol Encryption** | End-to-end encryption with Curve25519 + ChaCha20-Poly1305 |
| **GossipSub Messaging** | Scalable publish-subscribe protocol for P2P communication |
| **Thread-safe Design** | Modern C++17 concurrency with minimal overhead |

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

## Running

### GUI Mode (Default)

Simply run the executable:

```bash
./RatsSearch
```

Command line options:

| Option | Description |
|--------|-------------|
| `-p, --port <port>` | P2P listen port (overrides config setting) |
| `-d, --dht-port <port>` | DHT port (overrides config setting) |
| `--data-dir <path>` | Data directory for database and config |
| `-h, --help` | Display help |
| `-v, --version` | Display version |

### Console Mode (Headless/Server)

For servers without a display, use console mode:

```bash
./RatsSearch --console
```

Console mode options:

| Option | Description |
|--------|-------------|
| `-c, --console` | Run in console mode (no GUI) |
| `-p, --port <port>` | P2P listen port (overrides config setting) |
| `-d, --dht-port <port>` | DHT port (overrides config setting) |
| `--data-dir <path>` | Data directory for database and config |
| `-s, --spider` | Enable torrent spider (disabled by default in console mode) |
| `-m, --max-peers <n>` | Maximum P2P connections (overrides config, range: 10-1000) |

Interactive commands in console mode:

| Command | Description |
|---------|-------------|
| `stats` | Show statistics (torrents, files, peers, DHT nodes) |
| `search <query>` | Search torrents by name |
| `recent [n]` | Show n recent torrents (default: 10) |
| `top [type]` | Show top torrents by type |
| `spider start` | Start the DHT spider |
| `spider stop` | Stop the DHT spider |
| `peers [n]` | Show or set max P2P connections (10-1000) |
| `help` | Show available commands |
| `quit` / `exit` | Exit the application |

Example console session:

```bash
# Start with spider enabled
./RatsSearch --console --spider --data-dir /var/lib/rats-search

# Or start with custom ports
./RatsSearch -c -p 4445 -d 4446 -s
```

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

Rats Search includes a built-in REST API server for integrations and custom clients.

[📖 API Documentation](docs/API.md)

## Usage Manuals

* [English](docs/USAGE.md)
* [Russian](docs/USAGE.RU.md)

## Contributing

We welcome all contributions: bug fixes, improvements, code refactoring, and other enhancements.

* [Translation Guide](docs/TRANSLATION.md)
* [Tracker Support](docs/TRACKERS.md)

## Docker

Run Rats Search in a container (console mode, no GUI):

```bash
docker build -t rats-search .
docker run -d -p 8095:8095 -v rats-data:/data rats-search
```

The default command starts the spider with 30 max P2P peers. Override as needed:

```bash
docker run -d \
  -p 8095:8095 \
  -v rats-data:/data \
  rats-search \
  /app/RatsSearch --console --spider --max-peers 100 --data-dir /data
```

| Port / Volume | Description |
|---------------|-------------|
| `8095` | HTTP REST API (enable `restApi` in `/data/rats.json`) |
| `/data` | Persistent storage for database, config, and logs |

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

[Legacy Server Configuration](docs/SERVER.md)

[Server Compatibility Notes](docs/SERVER_COMPATIBILITY.md)


## Support & Donation

Bitcoin: bc1qsm5akf0gf2jnnxvjpf6nn3cd2p29yt3svxva3g

Subscribe to autor (GitHub donations): https://github.com/sponsors/DEgITx


## Contacts

- [Discord Community](https://discord.gg/t9GQtxA)
- [GitHub Issues](https://github.com/DEgITx/rats-search/issues)
- Twitter/X: [@RatsSearch](https://twitter.com/RatsSearch)

## License

[MIT License](https://github.com/DEgITx/rats-search/blob/master/LICENSE) © 2026
