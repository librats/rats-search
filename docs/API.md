# Rats Search API

Rats Search 2.0 (Qt) includes a built-in REST + WebSocket API server for integration with external applications and custom clients.

---

## Enabling the API

**Via Settings UI:**
1. Open File -> Settings
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

---

## Transport Protocols

### REST API (HTTP)

All methods are accessible via HTTP GET requests:

```
GET http://localhost:8095/api/{method}?{params}
```

Query parameters are automatically parsed:
- Numbers are converted from strings (`limit=10` -> `10`)
- Booleans are recognized (`safeSearch=true` -> `true`)
- JSON objects/arrays are parsed when wrapped in `{}` or `[]`

**Response format:**

```json
{
    "success": true,
    "data": {},
    "requestId": "..."
}
```

On error:

```json
{
    "success": false,
    "error": "Error description",
    "requestId": "..."
}
```

CORS headers are included in all responses, so the API can be called from browser clients.

### WebSocket

WebSocket server runs on HTTP port + 1 (e.g., `ws://localhost:8096`).

**Request format (JSON-RPC style):**

```json
{
    "method": "search.torrents",
    "params": { "text": "ubuntu", "limit": 10 },
    "id": "optional-request-id"
}
```

**Response format:**

```json
{
    "success": true,
    "data": [],
    "requestId": "optional-request-id"
}
```

**Server-push events** are automatically sent to all connected WebSocket clients:

```json
{
    "event": "eventName",
    "data": {}
}
```

See [WebSocket Events](#websocket-events) for the full list.

---

## API Methods

### Search

#### `search.torrents` - Search torrents by name

```
GET http://localhost:8095/api/search.torrents?text=ubuntu&limit=10
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `text` | string | yes | | Search query (min 3 chars). Also accepts magnet links and 40-char info hashes |
| `index` | int | | `0` | Pagination offset |
| `limit` | int | | `10` | Max results per page |
| `orderBy` | string | | | Field to order by |
| `orderDesc` | bool | | `true` | Sort descending |
| `safeSearch` | bool | | `false` | Enable safe search filter |
| `type` | string | | | Content type filter (e.g. `video`, `audio`, `pictures`, `books`, `software`, `archive`) |
| `size` | object | | | Size filter: `{"min": 0, "max": 1000000000}` |
| `files` | object | | | File count filter: `{"min": 1, "max": 100}` |

**Response** - `data` is an array of torrent objects:

```json
{
    "success": true,
    "data": [
        {
            "hash": "abc123...",
            "name": "Ubuntu 24.04 LTS",
            "size": 4700000000,
            "files": 1,
            "piecelength": 262144,
            "added": 1700000000000,
            "contentType": "software",
            "contentCategory": "",
            "seeders": 150,
            "leechers": 10,
            "completed": 5000,
            "trackersChecked": 1700000000000,
            "good": 42,
            "bad": 1
        }
    ]
}
```

> **Note:** When searching by info hash, if the torrent is not found locally, a DHT metadata lookup is attempted automatically.

Also triggers a P2P remote search - additional results arrive via the `remoteSearchResults` WebSocket event.

---

#### `search.files` - Search by file names inside torrents

```
GET http://localhost:8095/api/search.files?text=readme.txt&limit=10
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `text` | string | yes | | File name search query (min 3 chars) |
| `index` | int | | `0` | Pagination offset |
| `limit` | int | | `10` | Max results per page |
| `orderBy` | string | | | Field to order by |
| `orderDesc` | bool | | `true` | Sort descending |
| `safeSearch` | bool | | `false` | Enable safe search filter |

**Response** - torrent objects with additional file match fields:

```json
{
    "success": true,
    "data": [
        {
            "hash": "abc123...",
            "name": "Some Torrent",
            "isFileMatch": true,
            "matchingPaths": ["path/to/readme.txt"],
            "path": ["file1.txt", "path/to/readme.txt"]
        }
    ]
}
```

---

#### `search.torrent` - Get a single torrent by hash

```
GET http://localhost:8095/api/search.torrent?hash=29ebe63feb8be91b6dcff02bacc562d9a99ea864&files=true
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | 40-character hex info hash |
| `files` | bool | | `false` | Include file list |
| `peer` | string | | | Fetch from a specific remote peer (P2P) |

**Response:**

```json
{
    "success": true,
    "data": {
        "hash": "29ebe63feb8be91b6dcff02bacc562d9a99ea864",
        "name": "Example Torrent",
        "size": 1500000000,
        "files": 5,
        "filesList": [
            { "path": "video.mp4", "size": 1400000000 },
            { "path": "readme.txt", "size": 1024 }
        ],
        "download": { "progress": 0.5, "paused": false }
    }
}
```

> If the torrent is not in the local database and BitTorrent/DHT is enabled, metadata is fetched via DHT (BEP 9) automatically.

---

#### `search.recent` - Get recently added torrents

```
GET http://localhost:8095/api/search.recent?limit=20
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `limit` | int | | `10` | Number of recent torrents |

---

#### `search.top` - Get top torrents by seeders

```
GET http://localhost:8095/api/search.top?type=video&limit=20&time=week
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `type` | string | yes | | Content type (`video`, `audio`, `pictures`, `books`, `software`, `archive`) |
| `index` | int | | `0` | Pagination offset |
| `limit` | int | | `20` | Max results |
| `time` | string | | | Time period: `hours`, `week`, `month` |

---

### Downloads

#### `downloads.add` - Start downloading a torrent

```
GET http://localhost:8095/api/downloads.add?hash=29ebe63feb8be91b6dcff02bacc562d9a99ea864&savePath=C:/Downloads
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | 40-character info hash |
| `savePath` | string | | | Custom save directory |

---

#### `downloads.cancel` - Cancel a download

```
GET http://localhost:8095/api/downloads.cancel?hash=29ebe63feb8be91b6dcff02bacc562d9a99ea864
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | Info hash of the download to cancel |

---

#### `downloads.update` - Update download settings (pause/resume)

```
GET http://localhost:8095/api/downloads.update?hash=29ebe63feb8be91b6dcff02bacc562d9a99ea864&pause=true
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | Info hash |
| `pause` | bool/string | | | `true`/`false` to pause/resume, `"switch"` to toggle |
| `removeOnDone` | bool/string | | | `true`/`false` to set, `"switch"` to toggle |

**Response:**

```json
{
    "success": true,
    "data": {
        "paused": true,
        "removeOnDone": false
    }
}
```

---

#### `downloads.selectFiles` - Select files for download

```
GET http://localhost:8095/api/downloads.selectFiles?hash=29ebe63...&files=[0,2,5]
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | Info hash |
| `files` | array | yes | | Array of file indices to download |

---

#### `downloads.list` - Get all active downloads

```
GET http://localhost:8095/api/downloads.list
```

No parameters. Returns an array of active download objects with progress info.

---

### Statistics

#### `stats.database` - Get database statistics

```
GET http://localhost:8095/api/stats.database
```

**Response:**

```json
{
    "success": true,
    "data": {
        "torrents": 150000,
        "files": 2500000,
        "size": 890000000000000
    }
}
```

---

#### `stats.peers` - Get P2P peer information

```
GET http://localhost:8095/api/stats.peers
```

**Response:**

```json
{
    "success": true,
    "data": {
        "size": 12,
        "connected": true,
        "dhtNodes": 350
    }
}
```

---

#### `stats.p2pStatus` - Get detailed P2P status

```
GET http://localhost:8095/api/stats.p2pStatus
```

**Response:**

```json
{
    "success": true,
    "data": {
        "running": true,
        "connected": true,
        "peerId": "abc123...",
        "peerCount": 12,
        "dhtRunning": true,
        "dhtNodes": 350,
        "bitTorrentEnabled": true
    }
}
```

---

### Configuration

#### `config.get` - Get current configuration

```
GET http://localhost:8095/api/config.get
```

Returns the full configuration object (see [SERVER.md](SERVER.md) for available options).

---

#### `config.set` - Update configuration

```
GET http://localhost:8095/api/config.set?darkMode=false&httpPort=9000
```

Pass any configuration keys as parameters. Returns list of changed keys:

```json
{
    "success": true,
    "data": {
        "changed": ["darkMode", "httpPort"]
    }
}
```

---

### Torrent Operations

#### `torrent.vote` - Vote on a torrent

```
GET http://localhost:8095/api/torrent.vote?hash=29ebe63...&isGood=true
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | 40-character info hash |
| `isGood` | bool | | `true` | `true` for upvote, `false` for downvote |

Votes are stored both locally and in the P2P distributed store (synced across peers).

**Response:**

```json
{
    "success": true,
    "data": {
        "hash": "29ebe63...",
        "good": 43,
        "bad": 1,
        "selfVoted": true,
        "distributed": true
    }
}
```

---

#### `torrent.getVotes` - Get vote counts for a torrent

```
GET http://localhost:8095/api/torrent.getVotes?hash=29ebe63...
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | 40-character info hash |

**Response:**

```json
{
    "success": true,
    "data": {
        "hash": "29ebe63...",
        "good": 43,
        "bad": 1,
        "selfVoted": true,
        "source": "distributed"
    }
}
```

`source` can be: `distributed`, `local`, `none`, `unavailable`.

---

#### `torrent.checkTrackers` - Check tracker info (seeders/leechers)

```
GET http://localhost:8095/api/torrent.checkTrackers?hash=29ebe63feb8be91b6dcff02bacc562d9a99ea864
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | 40-character info hash |

Scrapes multiple BitTorrent trackers and returns the best result. Updates the database automatically.

**Response:**

```json
{
    "success": true,
    "data": {
        "hash": "29ebe63...",
        "status": "success",
        "seeders": 150,
        "leechers": 10,
        "completed": 5000,
        "tracker": "udp://tracker.example.com:6969"
    }
}
```

---

#### `torrent.scrapeTrackerInfo` - Scrape tracker websites for details

```
GET http://localhost:8095/api/torrent.scrapeTrackerInfo?hash=29ebe63...
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | 40-character info hash |

Queries tracker websites (RuTracker, Nyaa, etc.) for additional info: description, poster image, content category, tracker links. Results are merged into the torrent's `info` field.

---

#### `torrent.addFile` - Add a .torrent file to the search index

```
GET http://localhost:8095/api/torrent.addFile?path=C:/Downloads/example.torrent
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `path` | string | yes | | Path to .torrent file on disk |

Parses the torrent file and indexes it in the database. Does **not** start downloading.

---

#### `torrent.remove` - Remove torrents matching filters

```
GET http://localhost:8095/api/torrent.remove?checkOnly=true
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `checkOnly` | bool | | `false` | If `true`, only count matching torrents without deleting |

---

### Feed

#### `feed.get` - Get feed (voted/popular torrents)

```
GET http://localhost:8095/api/feed.get?index=0&limit=20
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `index` | int | | `0` | Pagination offset |
| `limit` | int | | `20` | Number of items |

---

## WebSocket Events

When connected via WebSocket, the server pushes events for real-time updates:

| Event | Description | Data |
|-------|-------------|------|
| `remoteSearchResults` | Results arrived from P2P peers | `{ "searchId": "...", "torrents": [...] }` |
| `downloadProgress` | Download progress update | `{ "hash": "...", "progress": 0.5, ... }` |
| `downloadCompleted` | Download finished | `{ "hash": "...", "cancelled": false }` |
| `filesReady` | File list available for download | `{ "hash": "...", "files": [...] }` |
| `configChanged` | Configuration was updated | `{ "key": "value", ... }` |
| `votesUpdated` | Vote counts changed | `{ "hash": "...", "good": 43, "bad": 1 }` |
| `feedUpdated` | Feed was updated | `{ "feed": [...] }` |
| `torrentIndexed` | New torrent added to database | `{ "hash": "...", "name": "..." }` |

---

## Torrent Object Format

Most search/torrent endpoints return objects with these fields:

| Field | Type | Description |
|-------|------|-------------|
| `hash` | string | 40-character hex info hash |
| `name` | string | Torrent name |
| `size` | int64 | Total size in bytes |
| `files` | int | Number of files |
| `piecelength` | int | Piece size in bytes |
| `added` | int64 | Timestamp (milliseconds since epoch) |
| `contentType` | string | Detected content type (`video`, `audio`, `pictures`, `books`, `software`, `archive`, etc.) |
| `contentCategory` | string | Content sub-category |
| `seeders` | int | Number of seeders (from tracker) |
| `leechers` | int | Number of leechers (from tracker) |
| `completed` | int | Download count (from tracker) |
| `trackersChecked` | int64 | Last tracker check timestamp |
| `good` | int | Upvote count |
| `bad` | int | Downvote count |
| `info` | object | Additional info from tracker websites (poster, description, etc.) |

---

## Examples

### cURL - Search torrents

```bash
curl "http://localhost:8095/api/search.torrents?text=ubuntu&limit=5"
```

### cURL - Get database stats

```bash
curl "http://localhost:8095/api/stats.database"
```

### cURL - Start a download

```bash
curl "http://localhost:8095/api/downloads.add?hash=29ebe63feb8be91b6dcff02bacc562d9a99ea864"
```

### WebSocket (JavaScript)

```javascript
const ws = new WebSocket('ws://localhost:8096');

ws.onopen = () => {
    // Search for torrents
    ws.send(JSON.stringify({
        method: 'search.torrents',
        params: { text: 'ubuntu', limit: 10 },
        id: 'req-1'
    }));
};

ws.onmessage = (event) => {
    const msg = JSON.parse(event.data);
    
    if (msg.requestId) {
        // This is a response to our request
        console.log('Response:', msg);
    } else if (msg.event) {
        // This is a server-push event
        console.log('Event:', msg.event, msg.data);
    }
};
```

### Python

```python
import requests

# Search for torrents
r = requests.get('http://localhost:8095/api/search.torrents', params={
    'text': 'ubuntu',
    'limit': 10
})
data = r.json()

if data['success']:
    for torrent in data['data']:
        print(f"{torrent['name']} - {torrent['seeders']} seeders")
```

---

## Legacy Version (Node.js)

> The legacy Electron/Node.js version is located in the `legacy/` folder.
> It uses a different API format (socket.io + polling queue). See the legacy README for details.
