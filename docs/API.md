# Rats Search API

Rats Search 2.0 (Qt) includes a built-in REST + WebSocket API server for integration with external applications and custom clients.

The API is a thin transport in front of the unified **`ApiRouter`** method surface
(`src/rest/api_router.cpp`). Every method is a dotted name (`search.torrents`,
`download.add`, `config.set`, …); the same router backs both HTTP and WebSocket.

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

See [WebSocket Events](#websocket-events) for details.

---

## Method Summary

| Method | Purpose |
|--------|---------|
| `search.torrents` | Search torrents by name (with filters; DHT fallback for info-hash queries) |
| `search.files` | Search by file names inside torrents |
| `search.top` | Top torrents by seeders for a content type |
| `search.recent` | Recently added torrents |
| `torrent.get` | Get a single torrent by hash (DHT fallback) |
| `torrent.remove` | Remove torrents by hash list |
| `torrent.create` | Create a `.torrent` file (optionally start seeding) |
| `torrent.import` | Parse a `.torrent` file and index it |
| `download.add` | Start downloading (hash or magnet) |
| `download.addFile` | Start downloading from a `.torrent` file |
| `download.pause` | Pause a download |
| `download.resume` | Resume a download |
| `download.remove` | Remove/cancel a download |
| `download.list` | List all active downloads |
| `download.selectFiles` | Choose which files to download |
| `feed.get` | Get the voted/popular feed |
| `vote.cast` | Up/down-vote a torrent |
| `vote.get` | Get vote counts for a torrent |
| `config.get` | Get the full configuration |
| `config.set` | Update configuration keys |
| `stats.get` | Database + peer statistics |
| `peers.list` | Connected P2P peers |
| `tracker.check` | Trigger a tracker scrape for a torrent |
| `update.check` | Check GitHub for an application update |

---

## API Methods

### Search

#### `search.torrents` - Search torrents by name

```
GET http://localhost:8095/api/search.torrents?text=ubuntu&limit=10
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `text` (or `query`) | string | yes | | Search query. Also accepts magnet links and 40-char info hashes |
| `index` (or `offset`) | int | | `0` | Pagination offset |
| `limit` | int | | `10` | Max results per page |
| `orderBy` (or `sort`) | string | | | Field to order by |
| `orderDesc` | bool | | `true` | Sort descending |
| `safeSearch` | bool | | `false` | Enable safe-search filter |
| `type` (or `contentType`) | string | | | Content type filter (e.g. `video`, `audio`, `pictures`, `books`, `software`, `games`, `archive`) |
| `size` | object | | | Size filter: `{"min": 0, "max": 1000000000}` |
| `files` | object | | | File count filter: `{"min": 1, "max": 100}` |

**Response** - `data` is an array of torrent objects (see [Torrent Object Format](#torrent-object-format)).

> **Note:** When the query is a bare 40-char info hash and nothing matches locally, a DHT/BEP 9 metadata lookup is attempted automatically; a hit is indexed and returned with `"fromDHT": true`.

---

#### `search.files` - Search by file names inside torrents

```
GET http://localhost:8095/api/search.files?text=readme.txt&limit=10
```

Same parameters as `search.torrents`. The query must be longer than 2 characters.

**Response** - torrent objects that matched on a file path, carrying extra fields:

```json
{
    "success": true,
    "data": [
        {
            "hash": "abc123...",
            "name": "Some Torrent",
            "fileMatch": true,
            "matchingPaths": ["path/to/<b>readme.txt</b>"],
            "files_list": [
                { "path": "file1.txt", "size": 1024 },
                { "path": "path/to/readme.txt", "size": 512 }
            ]
        }
    ]
}
```

---

#### `search.top` - Get top torrents by seeders

```
GET http://localhost:8095/api/search.top?type=video&limit=20&time=week
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `type` | string | | | Content type (`video`, `audio`, `pictures`, `books`, `software`, `games`, `archive`) |
| `time` | string | | | Time period: `hours`, `week`, `month` |
| `index` | int | | `0` | Pagination offset |
| `limit` | int | | `20` | Max results |

**Response** - array of torrent objects.

---

#### `search.recent` - Get recently added torrents

```
GET http://localhost:8095/api/search.recent?limit=20
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `limit` | int | | `10` | Number of recent torrents |

**Response** - array of torrent objects.

---

### Torrent Lifecycle

#### `torrent.get` - Get a single torrent by hash

```
GET http://localhost:8095/api/torrent.get?hash=29ebe63feb8be91b6dcff02bacc562d9a99ea864&files=true
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | 40-character hex info hash |
| `files` | bool | | `false` | Include the file list (`files_list`) |

**Response** - a single torrent object. If the torrent is currently downloading, a `download` object (progress/paused state) is attached.

> If the torrent is not indexed locally and BitTorrent/DHT is enabled, metadata is fetched via DHT (BEP 9) automatically and returned with `"fromDHT": true`. On a miss the call fails with `"Torrent not found"`.

---

#### `torrent.remove` - Remove torrents by hash

```
GET http://localhost:8095/api/torrent.remove?hashes=["29ebe63...","abc123..."]
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hashes` | array | yes | | Array of 40-character info hashes to remove |

Emits `torrent.remove.progress` WebSocket events during a large removal.

**Response:**

```json
{
    "success": true,
    "data": { "total": 2, "removed": 2 }
}
```

---

#### `torrent.create` - Create a .torrent file

```
GET http://localhost:8095/api/torrent.create?path=C:/Media/Album&output=C:/album.torrent
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `path` | string | yes | | File or directory to build the torrent from |
| `output` | string | required unless `seed` | | Destination `.torrent` file path |
| `trackers` | array | | | Tracker announce URLs |
| `comment` | string | | | Optional comment |
| `seed` | bool | | `false` | If `true`, create and immediately start seeding |

**Response** - `{ "file": "<output>" }`, or when `seed` is true `{ "hash": "...", "seeding": true }`.

---

#### `torrent.import` - Parse a .torrent file and index it

```
GET http://localhost:8095/api/torrent.import?path=C:/Downloads/example.torrent
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `path` (or `file`) | string | yes | | Path to a `.torrent` file on disk |

Parses the file and inserts it through the single `IndexingService` path (content detection + filters). Does **not** start downloading.

**Response** - the indexed torrent object plus:

```json
{
    "alreadyExists": false,
    "imported": true
}
```

---

### Downloads

#### `download.add` - Start downloading a torrent

```
GET http://localhost:8095/api/download.add?hash=29ebe63feb8be91b6dcff02bacc562d9a99ea864&savePath=C:/Downloads
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` (or `magnet`) | string | yes | | 40-character info hash or a magnet link |
| `savePath` | string | | | Custom save directory (default download path otherwise) |

Indexed metadata (name/size) is reused when the torrent is already known.

---

#### `download.addFile` - Start downloading from a .torrent file

```
GET http://localhost:8095/api/download.addFile?path=C:/Downloads/example.torrent&savePath=C:/Downloads
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `path` (or `file`) | string | yes | | Path to a `.torrent` file |
| `savePath` | string | | | Custom save directory |

---

#### `download.pause` / `download.resume` - Pause or resume a download

```
GET http://localhost:8095/api/download.pause?hash=29ebe63...
GET http://localhost:8095/api/download.resume?hash=29ebe63...
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | Info hash of the download |

Fails with `"Download not found"` if the hash is not an active download.

---

#### `download.remove` - Remove (cancel) a download

```
GET http://localhost:8095/api/download.remove?hash=29ebe63...&saveResumeData=false
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | Info hash of the download |
| `saveResumeData` | bool | | `false` | Persist resume data before removing |

---

#### `download.list` - Get all active downloads

```
GET http://localhost:8095/api/download.list
```

No parameters. Returns an array of active-download objects with progress info.

---

#### `download.selectFiles` - Select files for download

```
GET http://localhost:8095/api/download.selectFiles?hash=29ebe63...&files=[0,2,5]
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | Info hash |
| `files` | array | yes | | Array of file indices to download |

---

### Feed

#### `feed.get` - Get the feed (voted/popular torrents)

```
GET http://localhost:8095/api/feed.get?index=0&limit=20
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `index` | int | | `0` | Pagination offset |
| `limit` | int | | `20` | Number of items |

**Response** - array of feed items (torrent objects ranked by recency + votes).

---

### Voting

Votes are stored in the librats distributed store (one record per peer per torrent) and aggregated swarm-wide. These methods answer asynchronously.

#### `vote.cast` - Vote on a torrent

```
GET http://localhost:8095/api/vote.cast?hash=29ebe63...&good=true
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | 40-character info hash |
| `good` (or `isGood`) | bool | | `true` | `true` for up-vote, `false` for down-vote |

**Response** - the aggregated vote result (e.g. `hash`, `good`, `bad`, self-vote / distributed flags).

---

#### `vote.get` - Get vote counts for a torrent

```
GET http://localhost:8095/api/vote.get?hash=29ebe63...
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | 40-character info hash |

**Response** - the aggregated `good`/`bad` counts for the torrent.

---

### Configuration

#### `config.get` - Get current configuration

```
GET http://localhost:8095/api/config.get
```

Returns the full configuration object (all `ConfigStore` settings).

---

#### `config.set` - Update configuration

```
GET http://localhost:8095/api/config.set?darkMode=false&httpPort=9000
```

Pass any configuration keys as parameters. Returns the list of keys that actually changed:

```json
{
    "success": true,
    "data": { "changed": ["darkMode", "httpPort"] }
}
```

---

### Statistics & Peers

#### `stats.get` - Database and peer statistics

```
GET http://localhost:8095/api/stats.get
```

**Response:**

```json
{
    "success": true,
    "data": {
        "torrents": 150000,
        "files": 2500000,
        "size": 890000000000000,
        "peers": 12
    }
}
```

---

#### `peers.list` - Connected P2P peers

```
GET http://localhost:8095/api/peers.list
```

**Response** - array of connected peers, each carrying its advertised `PeerStats`:

```json
{
    "success": true,
    "data": [
        {
            "peerId": "abc123...",
            "clientVersion": "2.0.0",
            "torrents": 50000,
            "files": 800000,
            "totalSize": 120000000000000,
            "peersConnected": 8,
            "connectedAt": 1700000000000
        }
    ]
}
```

---

### Trackers

#### `tracker.check` - Trigger a tracker scrape

```
GET http://localhost:8095/api/tracker.check?hash=29ebe63feb8be91b6dcff02bacc562d9a99ea864
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hash` | string | yes | | 40-character info hash |

Fire-and-forget: queues a scrape of the swarm (seeders/leechers/completed) and website info; the results are written back into the index asynchronously.

**Response:**

```json
{
    "success": true,
    "data": { "hash": "29ebe63...", "status": "checking" }
}
```

---

### Updates

#### `update.check` - Check for an application update

```
GET http://localhost:8095/api/update.check
```

No parameters. Queries GitHub releases asynchronously and answers once.

**Response** when an update is available:

```json
{
    "success": true,
    "data": {
        "available": true,
        "version": "2.1.0",
        "downloadUrl": "https://github.com/.../RatsSearch.zip",
        "releaseNotes": "…",
        "downloadSize": 45000000,
        "publishedAt": "2026-01-01T00:00:00Z",
        "prerelease": false
    }
}
```

When up to date: `{ "available": false }`.

---

## WebSocket Events

Push events are broadcast to all connected WebSocket clients as `{ "event": name, "data": {…} }`. Events are emitted by `ApiRouter::event()` as work progresses.

| Event | Description | Data |
|-------|-------------|------|
| `torrent.remove.progress` | Progress of a bulk `torrent.remove` | `{ "processed": 100, "removed": 98, "total": 500 }` |

> The peer/UI layers additionally observe remote P2P activity (remote search results, replicated torrents, feed/vote updates) through `PeerApi` Qt signals; those are surfaced in the GUI. The WebSocket channel forwards whatever `ApiRouter` broadcasts.

---

## Torrent Object Format

Most search/torrent methods return objects with these fields (serialized by `rats::domain::codec`):

| Field | Type | Description |
|-------|------|-------------|
| `hash` | string | 40-character hex info hash |
| `name` | string | Torrent name |
| `size` | int64 | Total size in bytes |
| `files` | int | Number of files |
| `pieceLength` | int | Piece size in bytes |
| `added` | int64 | Timestamp (milliseconds since epoch) |
| `contentType` | string | Detected content type (`video`, `audio`, `pictures`, `books`, `software`, `games`, `archive`, …) |
| `contentCategory` | string | Content sub-category |
| `seeders` | int | Number of seeders (from tracker) |
| `leechers` | int | Number of leechers (from tracker) |
| `completed` | int | Download count (from tracker) |
| `trackersChecked` | int64 | Last tracker check timestamp (ms) |
| `good` | int | Up-vote count |
| `bad` | int | Down-vote count |
| `info` | object | Scraped extras (poster, description, tracker payloads); included when present |
| `files_list` | array | `[{ "path", "size" }]`; included when files are requested |

Search hits (`search.torrents` / `search.files`) may additionally carry `fileMatch`, `matchingPaths`, `peer` (source peer id) and `remote` for results found by file name or received from a P2P peer. Results resolved via DHT carry `fromDHT: true`.

---

## Examples

### cURL - Search torrents

```bash
curl "http://localhost:8095/api/search.torrents?text=ubuntu&limit=5"
```

### cURL - Get statistics

```bash
curl "http://localhost:8095/api/stats.get"
```

### cURL - Start a download

```bash
curl "http://localhost:8095/api/download.add?hash=29ebe63feb8be91b6dcff02bacc562d9a99ea864"
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
