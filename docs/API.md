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
| `search.history` | Recent search queries, most recent first |
| `search.history.remove` | Forget one stored query |
| `search.history.clear` | Forget every stored query |
| `torrent.get` | Get a single torrent by hash (DHT fallback) |
| `torrent.remove` | Remove torrents by hash list |
| `torrent.cleanup` | Re-apply the filter policy to the index and drop torrents that no longer pass |
| `torrent.create` | Create a `.torrent` file (optionally start seeding) |
| `torrent.import` | Parse a `.torrent` file and index it |
| `database.export` | Write the whole index to a portable `.ratsdb` dump |
| `database.import` | Merge a `.ratsdb` dump into the index |
| `database.pull` | Ask a connected peer for its whole index |
| `database.peers` | Connected peers that will actually serve their index |
| `database.status` | Current state of the local operation, the serving lane and the snapshot |
| `database.cancel` | Stop the local database operation |
| `database.snapshot` | Rebuild the dump served to peers, now |
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
| `size` | object | | | Size filter in bytes, bounds inclusive: `{"min": 0, "max": 1000000000}`. `0` (or a missing key) leaves that bound unset |
| `files` | object | | | File count filter, bounds inclusive: `{"min": 1, "max": 100}`. `0` (or a missing key) leaves that bound unset |

**Response** - `data` is an array of torrent objects (see [Torrent Object Format](#torrent-object-format)).

> **Note:** When the query is a bare 40-char info hash and nothing matches locally, a DHT/BEP 9 metadata lookup is attempted automatically; a hit is indexed and returned with `"fromDHT": true`.

---

#### `search.files` - Search by file names inside torrents

```
GET http://localhost:8095/api/search.files?text=readme.txt&limit=10
```

Same parameters as `search.torrents`. The query must be longer than 2 characters. The `type`, `safeSearch`, `size` and `files` filters apply to the torrent the matching file belongs to.

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

### Search History

Every `search.torrents` call is recorded as a search-history entry (the GUI records
what is typed into its search field the same way). Entries are deduplicated
case-insensitively — a repeated query moves back to the front and bumps its
`count` — and the newest 100 are kept, in `search-history.json` in the data
directory. Recording is off while the `searchHistory` config key is `false`;
stored entries are kept until cleared explicitly.

#### `search.history` - Recent search queries

```
GET http://localhost:8095/api/search.history?limit=20
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `limit` | int | | `100` | Number of entries (most recent first) |

**Response:**

```json
[
    { "query": "ubuntu", "lastSearchedAt": 1735689600000, "count": 3 }
]
```

#### `search.history.remove` - Forget one query

```
GET http://localhost:8095/api/search.history.remove?query=ubuntu
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `query` | string | yes | | The stored query (matched case-insensitively) |

**Response:** `{ "removed": true }` — `false` when the query was not stored.

#### `search.history.clear` - Forget every query

```
GET http://localhost:8095/api/search.history.clear
```

**Response:** `{ "cleared": true }` — `false` when the history was already empty.

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
| `peer` | string | | | Peer id that offered this torrent in a remote search hit. When `files=true` and the file list isn't available locally, it is fetched from this peer (and cloned into the local index). |

**Response** - a single torrent object. If the torrent is currently downloading, a `download` object (progress/paused state) is attached.

> Remote search hits carry metadata only — never a file list. Requesting `files=true` for such a hit resolves the files on demand: it fetches the full torrent from `peer` (if given), otherwise via DHT, and clones the result — with its files — into the local index. Resolution order when the file list is missing locally is **peer → DHT**; on a miss the call fails with `"Torrent not found"`. A torrent fetched via DHT is returned with `"fromDHT": true`.

---

#### `torrent.remove` - Remove torrents by hash

```
GET http://localhost:8095/api/torrent.remove?hashes=["29ebe63...","abc123..."]
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `hashes` | array | yes | | Array of 40-character info hashes to remove |

Emits `torrentRemoveProgress` WebSocket events during a large removal.

**Response:**

```json
{
    "success": true,
    "data": { "total": 2, "removed": 2 }
}
```

---

#### `torrent.cleanup` - Re-apply filters to the whole index

Walks every indexed torrent and checks it against the current filter policy
(`filters.*` config keys), removing the ones that no longer pass. Use it after
tightening the adult/size/content-type filters.

```
GET http://localhost:8095/api/torrent.cleanup?dryRun=true
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `dryRun` | bool | no | `false` | Only count the matches; remove nothing |
| `filters` | object | no | stored config | Filter overrides applied for this sweep only, key by key |

`filters` takes the same keys as the `filters` config section — `maxFiles`,
`sizeMin`, `sizeMax` (bytes), `adultFilter`, `namingRegExp`,
`namingRegExpNegative`, `contentType` — and each one given replaces the stored
value **for this call only**; nothing is persisted. This is how the settings
dialog previews rules that are still being edited. An unparsable `namingRegExp`
fails the call instead of silently accepting every name.

Emits `torrentCleanupProgress` WebSocket events while it sweeps.

**Response:**

```json
{
    "success": true,
    "data": { "dryRun": true, "scanned": 12045, "matched": 317, "removed": 0 }
}
```

`removed` is always `0` when `dryRun` is set; otherwise it equals `matched`.

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

### Database Replication

Whole-index replication: a dump you can hand to another user (any transport —
file share, USB stick, torrent) or move directly between two peers. Importing
**merges**: torrents you already have keep their local row and only gain what the
incoming copy adds (a file list for a metadata-only row, higher vote counts), and
new ones go in through the same `IndexingService` path a crawled torrent does.

There are **two independent lanes**, and they report separately.

*Local* is what this user asked for — export, import, or a pull from a peer. One
at a time. It returns immediately with the sync status; progress arrives as
`databaseSyncProgress` events and the outcome as `databaseSyncFinished`.

*Serving* is what other peers asked for. It runs in the background, does not block
the local lane, and reports through `databaseServeProgress` /
`databaseServeFinished`. A client must never surface a serving failure as the
user's own: the peer at the other end gave up on a download, which is not this
user's problem.

Serving is answered from a **snapshot** — one dump kept in `<dataDir>/dbsync/` and
handed to every peer that asks. It is rebuilt when it ages past
`databaseSnapshotMaxAgeHours` (default 6) or when the index has drifted more than
`databaseSnapshotMaxDriftPercent` (default 10) away from it. This is why a pull is
usually near-instant: nothing is exported per request, and five peers asking cost
one export rather than five.

#### `database.export` - Write the index to a dump file

```
GET http://localhost:8095/api/database.export?path=C:/backup/index.ratsdb
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `path` (or `file`) | string | yes | | Destination `.ratsdb` file |

**Response** - the sync status object (see `database.status`).

---

#### `database.import` - Merge a dump into the index

```
GET http://localhost:8095/api/database.import?path=C:/from-a-friend.ratsdb
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `path` (or `file`) | string | yes | | `.ratsdb` file to merge |
| `applyFilters` | bool | no | `true` | Run the local filter policy over the incoming torrents |
| `resume` | bool | no | `true` | Continue an interrupted import of the same file |
| `removeWhenDone` | bool | no | `false` | Delete the dump once it has been merged |

A dump that was cut short (interrupted transfer, killed exporter) is still
importable — every complete frame in it is merged and the result is reported as
successful with a truncation warning.

---

#### `database.pull` - Ask a peer for its whole index

```
GET http://localhost:8095/api/database.pull?peer=ab12cd34…
```

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `peer` (or `peerId`) | string | yes | | Peer id from `peers.list` |
| `applyFilters` | bool | no | `true` | Filter policy for the merge that follows |

The peer answers over P2P (`databaseRequest` / `databaseRequest_response`) and
then sends its snapshot over the librats file transfer; it arrives in
`<dataDir>/dbsync/`, is merged, and is deleted once fully merged (an interrupted
merge keeps the file and can be resumed — see `pendingImport` in
`database.status`).

**An interrupted download is continued, not restarted.** 

The response says whether the snapshot is `ready`. If it is, the file offer
follows within seconds. If it is not, the peer builds one and heartbeats its
progress as `databaseProgress` while it works, and those heartbeats are what our
deadline watches. **Every timeout here measures silence, not work** — how long a
peer needs to build a snapshot depends on the size of its index, so no fixed
timeout could be correct for both a 50k and a 3M-row database, whereas how long it
may go quiet does not depend on size at all. Giving up (or `database.cancel`) sends
`databaseCancel`, so the peer stops building and frees its serving slot instead of
finishing an export nobody is waiting for. The peer only agrees when it
has the `databaseSharing` config key enabled — **on by default**, and advertised
in the handshake, so use `database.peers` rather than `peers.list` to pick a
target and a refusal becomes the exception rather than the rule. There is no
interactive confirmation on either side: the decision is entirely the config
key's, which is what makes the headless daemon usable. A daemon can also be
pinned with the `--share-db=on|off` CLI flag, which overrides the key for that
run and cannot be changed through `config.set`. In the other direction, a file offer from a peer we did not ask
is rejected unopened.

---

#### `database.peers` - Peers worth asking

```
GET http://localhost:8095/api/database.peers
```

The subset of `peers.list` that advertised `databaseSharing` in the `client_info`
handshake. Peers with sharing turned off — and clients older than the feature,
which never send the flag — are absent: asking them can only produce a refusal or
a silence, so they are not worth offering as a choice. Same object shape as
`peers.list`.

**Response:**

```json
{
    "success": true,
    "data": [
        {
            "peerId": "ab12cd34…",
            "clientVersion": "2.2.3",
            "torrents": 184320,
            "files": 921600,
            "totalSize": 4398046511104,
            "peersConnected": 8,
            "connectedAt": 1737030000000,
            "databaseSharing": true
        }
    ]
}
```

---

#### `database.status` - State of the running sync

```
GET http://localhost:8095/api/database.status
```

**Response:**

```json
{
    "success": true,
    "data": {
        "operation": "import",
        "running": true,
        "stage": "importing",
        "path": "C:/from-a-friend.ratsdb",
        "peer": "",
        "processed": 12000,
        "total": 45000,
        "inserted": 8400,
        "merged": 3600,
        "rejected": 0,
        "bytes": 4194304,
        "totalBytes": 16777216,
        "sharing": true,
        "serve": {
            "generating": false,
            "processed": 0,
            "total": 0,
            "waiting": 0,
            "sending": 2
        },
        "snapshot": {
            "valid": true,
            "torrents": 184320,
            "bytes": 94371840,
            "created": "2026-08-29T09:14:22",
            "fresh": true
        },
        "pendingImport": {
            "path": "…/dbsync/incoming-ab12cd34.ratsdb",
            "offset": 8388608,
            "size": 94371840
        },
        "pendingTransfers": [
            {
                "peer": "ab12cd34…",
                "snapshot": "snapshot-7.ratsdb",
                "bytes": 6291456000,
                "totalBytes": 9663676416,
                "path": "…/dbsync/incoming-ab12cd34.ratsdb.part"
            }
        ]
    }
}
```

The top-level fields describe the **local** operation. `operation` is one of
`idle`, `export`, `import`, `peerPull`; `stage` is `exporting`, `waiting` (for an
answer), `preparing` (the peer is building its snapshot), `awaitingOffer`,
`transferring`, `importing`, `cancelling`, `done` or `failed`. `processed`/`total`
count torrents, `bytes`/`totalBytes` count bytes of the dump or transfer.

`serve` describes the **serving** lane: whether a snapshot is being generated and
how far, how many peers are queued on it, and how many are receiving it.

`snapshot` describes the dump on disk and whether it can be served as-is.

`pendingImport` is present only when an import was interrupted and can be
continued — pass its `path` to `database.import` to pick up where it stopped.

`pendingTransfers` is present only when a *download* from a peer was interrupted
and can be continued: one entry per peer, `bytes` of `totalBytes` already on
disk. Run `database.pull` against that `peer` to continue it. While such a pull
runs, the top-level `resumedFrom` says where it picked up (absent when it started
from nothing) and `bytes` counts from there, so `bytes`/`totalBytes` is still the
completeness of the file rather than of this attempt.

---

#### `database.cancel` - Stop the running sync

```
GET http://localhost:8095/api/database.cancel
```

Cancels the **local** operation only; peers we are serving are unaffected. An
export removes its partial file. An import keeps everything it already merged and
saves a resume point, so re-running `database.import` on the same file continues
where it stopped. A pull keeps what it has downloaded and tells the peer to stop
preparing; running it again against the same peer continues the transfer.

---

#### `database.snapshot` - Rebuild the served dump now

```
GET http://localhost:8095/api/database.snapshot
```

Discards the current snapshot and starts building a fresh one. Rarely needed —
the snapshot renews itself on age or drift — but it is the way to make recent
crawling visible to peers immediately. Fails while a generation is already
running.

**Response** - the status object (see `database.status`).

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
            "connectedAt": 1700000000000,
            "databaseSharing": true
        }
    ]
}
```

`databaseSharing` is what the peer advertised in its handshake: whether it will
serve its whole index to a `database.pull`. Clients older than the feature never
send it and read as `false`. `database.peers` returns exactly the entries where
it is `true`.

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

Push events are broadcast to all connected WebSocket clients as `{ "event": name, "data": {…} }`. Events are emitted by `ApiRouter::event()` as work progresses. Event names and payload shapes are part of the published contract.

| Event | Description | Data |
|-------|-------------|------|
| `downloadProgress` | A download advanced (emitted at most once per second, and only when the payload changed) | `{ "hash": "…", "downloaded": 1024, "total": 4096, "progress": 0.25, "downloadSpeed": 512, "paused": false, "removeOnDone": false, "timeRemaining": 6 }` |
| `downloadCompleted` | A download finished | `{ "hash": "…", "cancelled": false }` |
| `filesReady` | A magnet's metadata arrived, so its file list is now known | `{ "hash": "…", "files": [ { "path": "a.mkv", "size": 1024, "index": 0, "selected": true, "progress": 0.0 } ] }` |
| `torrentIndexed` | A genuinely new torrent entered the index | `{ "hash": "…", "name": "…" }` |
| `votesUpdated` | Vote counts for a torrent changed (local or remote vote) | `{ "hash": "…", "good": 3, "bad": 1 }` |
| `feedUpdated` | The feed changed | `{ "feed": [ …torrent objects… ] }` |
| `configChanged` | One or more config keys were written | The full config object plus `"changedKeys": ["filters.adultFilter"]` |
| `remoteSearchResults` | A peer answered a P2P search | `{ "searchId": "…", "torrents": [ …torrent objects… ] }` |
| `torrentRemoveProgress` | Progress of a bulk `torrent.remove` | `{ "processed": 100, "removed": 98, "total": 500 }` |
| `torrentCleanupProgress` | Progress of a `torrent.cleanup` sweep | `{ "scanned": 5000, "matched": 120, "total": 12045 }` |
| `databaseSyncStarted` | The **local** export/import/pull began | The status object from `database.status` |
| `databaseSyncProgress` | The local operation advanced | The status object from `database.status` |
| `databaseSyncFinished` | The local operation ended | The status object plus `"success": true` and, on failure, `"error"` |
| `databaseServeProgress` | The **serving** lane advanced (snapshot generation, peers waiting/receiving) | The `serve` object from `database.status` |
| `databaseServeFinished` | One peer's transfer ended | `peer`, `success`, and `reason` when it did not succeed |

`databaseServe*` is informational. A serving failure means a peer gave up on a
download from us; it is not a failure of anything the local user started, and
surfacing it as one is a bug.

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
