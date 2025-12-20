# Server Configuration

This document covers server configuration for both Qt 2.0 and the legacy Node.js version.

---

## Rats Search 2.0 (Qt Version)

### Built-in API Server

Rats Search 2.0 includes a built-in REST/WebSocket API server that can be enabled in the settings.

**Enable via UI:** File → Settings → Network → "Enable REST API server"

**Or via configuration file** (`rats.json`):

```json
{
    "restApiEnabled": true,
    "httpPort": 8095
}
```

### Configuration File Location

The configuration file `rats.json` is created in the data directory:

| Platform | Location |
|----------|----------|
| Windows | `%APPDATA%/RatsSearch/rats.json` or application directory |
| Linux | `~/.config/RatsSearch/rats.json` or `~/.local/share/RatsSearch/` |
| macOS | `~/Library/Application Support/RatsSearch/rats.json` |

### Qt 2.0 Configuration Options

```json
{
    "p2pPort": 4445,
    "dhtPort": 4446,
    "httpPort": 8095,
    "restApiEnabled": true,
    "indexerEnabled": true,
    "trackersEnabled": true,
    "darkMode": true,
    "trayOnMinimize": true,
    "trayOnClose": false,
    "startMinimized": false
}
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `p2pPort` | int | 4445 | P2P communication port |
| `dhtPort` | int | 4446 | DHT operations port |
| `httpPort` | int | 8095 | REST API server port |
| `restApiEnabled` | bool | false | Enable REST API server |
| `indexerEnabled` | bool | true | Enable DHT torrent indexer |
| `trackersEnabled` | bool | true | Enable tracker peer checking |
| `darkMode` | bool | true | Use dark theme |
| `trayOnMinimize` | bool | true | Hide to tray on minimize |
| `trayOnClose` | bool | false | Hide to tray on close |
| `startMinimized` | bool | false | Start minimized to tray |

---

## Legacy Version (Node.js)

> The legacy Electron/Node.js version is located in the `legacy/` folder.

### Data Directory Configuration

File `package.json` contains path to data directory. You can move this folder to any location (by default, data directory for web server is the same as root folder with package.json):

```json
{
    "serverDataDirectory": "./"
}
```

This directory always contains logs and other configuration entities.

### Legacy Configuration

After server start, `rats.json` will be created in the root folder:

```json
{
    "dbPath": "/path/to/db",
    "httpPort": 8095
} 
```

| Option | Description |
|--------|-------------|
| `httpPort` | Port which HTTP server listens on |
| `dbPath` | Path to database folder with torrent collection |
