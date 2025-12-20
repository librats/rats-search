# Rats Search API

Rats Search provides a REST API for integration with external applications and custom clients.

---

## Rats Search 2.0 (Qt Version)

The Qt 2.0 version includes a built-in REST API server.

### Enabling the API

**Via Settings UI:**
1. Open File → Settings
2. Go to Network section
3. Enable "Enable REST API server"
4. Set the HTTP port (default: 8095)
5. Click Save (restart required)

**Via Configuration File** (`rats.json`):

```json
{
    "restApiEnabled": true,
    "httpPort": 8095
}
```

The API server will be available at: `http://localhost:8095`

---

## Legacy Version (Node.js)

For the legacy Electron/Node.js version, the API uses WebSockets (socket.io) for internal communication. A REST API is also available but requires periodic polling of `/api/queue` for responses.

### Legacy Installation

```bash
# From the repository root
cd legacy
npm install --force
npm run buildweb
npm run server
```

### Legacy Configuration

Edit **rats.json**:

```json
{
    "restApi": true
}
```

---

## REST API Reference

## API Endpoints

#### Search of torrents

endpoint (GET REQUEST):
```
https://localhost:8095/api/searchTorrent?text=DEgITx&navigation={}
```

example of request:
```json
{
    "text": "Search Name of the torrent",
    "navigation": {
        "index": 0,
        "limit": 10,
        "orderBy": "order_field",
        "orderDesc": "DESC",
        "safeSearch": false
    }
}
```

| Field | Type | Optional | Default Value | Description |
| ----- | ---- | -------- | ------------- | ----------- |
| text | string | ❎ |  | torrent search name |
| navigation | object (Navigation) | ✅ |  | object with navigation params |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; index | int | ✅ | 0 | stating of torrent index of navigation |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; limit | int | ✅ | 10 | max number of results on page |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; orderBy | text | ✅ |  | field which is using for order results |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; orderDesc | enum [**DESC, ASC**] | ✅ | ASC | sort direction of the field |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; safeSearch | bool | ✅ | true | disable/enable safe search for torrents |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; type | string | ✅ |  | type of content for search |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; size | object (Interval) | ✅ |  | size of torrent |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; min | uint64 | ✅ |  | minumum size of the torrent |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; max | uint64 | ✅ |  | maximum size of the torrent |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; files | object (Interval) | ✅ |  | files on the torrent |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; min | int | ✅ |  | minumum size of the torrent |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; max | int | ✅ |  | maximum size of the torrent |

### Reading queue

As said before after each request and periodicly you need to read queue for additional messages.

endpoint (GET REQUEST):
```
https://localhost:8095/api/queue
```

after executing of search **/api/searchTorrent** request **additional result of search will be in queue**!

### Search of the torrent by files

endpoint (GET REQUEST):
```
https://localhost:8095/api/searchFiles?text=TorrentWithFileName&navigation={}
```

| Field | Type | Optional | Default Value | Description |
| ----- | ---- | -------- | ------------- | ----------- |
| text | string | ❎ |  | torrent search name |
| navigation | object (Navigation) | ✅ |  | object with navigation params |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; index | int | ✅ | 0 | stating of torrent index of navigation |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; limit | int | ✅ | 10 | max number of results on page |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; orderBy | text | ✅ |  | field which is using for order results |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; orderDesc | enum [**DESC, ASC**] | ✅ | ASC | sort direction of the field |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; safeSearch | bool | ✅ | true | disable/enable safe search for torrents |

### Recheck trackers info for the torrent

endpoint (GET REQUEST):
```
https://localhost:8095/api/checkTrackers?hash=29ebe63feb8be91b6dcff02bacc562d9a99ea864
```

| Field | Type | Optional | Default Value | Description |
| ----- | ---- | -------- | ------------- | ----------- |
| hash | string | ❎ |  | torrent hash to refresh token |

### Top torrents

endpoint (GET REQUEST):
```
https://localhost:8095/api/topTorrents?type=video&navigation={"time":"week"}
```

| Field | Type | Optional | Default Value | Description |
| ----- | ---- | -------- | ------------- | ----------- |
| type | string | ❎ |  | type of category for top of the torrents |
| navigation | object (Navigation) | ✅ |  | object with navigation params (check /api/searchTorrent for mo details) |
| &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; time | enum [hours, week, month] | ✅ |  | time for the top

