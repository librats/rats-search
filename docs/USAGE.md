## Usage

> **Note:** This documentation covers the general usage of Rats Search. Screenshots may show either the legacy Electron UI or the new Qt 2.0 interface. Core functionality remains the same across both versions.

### Rats Search 2.0 (Qt Version)

The new Qt 2.0 version provides a native desktop experience with improved performance. Key features:

* **Modern dark theme** with customizable settings
* **System tray integration** — minimize to tray, run in background
* **Built-in REST API** — enabled via Settings → Network → "Enable REST API server"
* **Improved search** — powered by Manticore Search for fast full-text queries

### Getting Started

After installing the program and running, you should have access to the main program window:

[![After start](img/main_no_torrents.png)](https://github.com/DEgITx/rats-search)

The program opens on the feed tab by default. In the tab there are most relevant torrents that are collected, with which some actions have been performed recently (recent likes, comments, etc.). If the tab is empty, you just need to wait for it to sync with any of the available peers (see below).

After the start, initially you do not have a search database. You can use ready-made search when copy datatabase into the settings folder, or wait until you have accumulated your database to a sufficient size, which will automatically happen if the settings are correct. Or simply use [distributed search](#distributed-search).

Collection of new torrents from the network should start automatically, in a minute or two after the program is launched (in case of correctly opened ports).
This can be monitored in the field with torrents under the search, as well as in the list of torrents in the activity tab.

[![A lot of torrents](img/peer.png)](https://github.com/DEgITx/rats-search)

The speed of the collection of torrents will increase with time. For details on configuring the scanner, refer to the [torrent scanner configuration](#torrent-scanner-settings).

[![First](img/first_torrent.png)](https://github.com/DEgITx/rats-search)

In the case when the collection of torrents does not occur, or is very slow (1-2 torrents in a few minutes), make sure that the ports specified in the settings are opened. Check next paragraph.

### Port configuration

[![Settings](img/settings_ports.png)](https://github.com/DEgITx/rats-search)

For correct operation, the following ports need to be open (configurable in Settings):

| Port | Protocol | Default | Purpose |
|------|----------|---------|---------|
| P2P Port | TCP/UDP | 4445 | P2P communication |
| DHT Port | UDP | 4446 | DHT operations |
| HTTP Port | TCP | 8095 | REST API (optional) |

Both UDP and TCP must be open for P2P/DHT ports. On routers with NAT enabled, ports must be forwarded. If your router supports UPnP, ports will be forwarded automatically.

**In Qt 2.0:** Access port settings via **File → Settings → Network**. 

### Using search

Over time, your database of torrents will naturally grow up, and you can search for the torrent you are interested in using the search.

[![A lot of torrents](img/base_big.png)](https://github.com/DEgITx/rats-search)

### Distributed search

In the case of the other ROTB clients found, there will be indicator at the top

[![A lot of torrents](img/peer.png)](https://github.com/DEgITx/rats-search)

You will be able to search advanced search among other ROTB clients, you need to perform a normal search, but additional results will be displayed. Depending on the number of peers and exactly those who found the result of the extended issue may vary.

[![External torrents](img/peers_search.png)](https://github.com/DEgITx/rats-search)

The results of search of other participants are marked with a separate color.

### Torrent scanner settings

[![Settings](img/settings_limits.png)](https://github.com/DEgITx/rats-search)

In the settings there are 3 parameters responsible for configuring the search for torrents in the network, each of them affects the application load, the rate of collection of torrents, the generation of traffic, as well as the total load for equipping the intermediate nodes of the network (router, etc.)

Recommended values:
* Maximum fast search / high load:
  * Scanner walk speed: 5
  * Nodes usage: 0 (disabled = max usage)
  * Reduce netowork packages: 0 (disabled = unlimited)
* Average search speed / average load:
  * Scanner walk speed: 15
  * Nodes usage: 100
  * Reduce netowork packages: 600
* Low search speed / average load:
  * Scanner walk speed: 30
  * Nodes usage: 10
  * Reduce netowork packages: 450

### Configuring global torrent filters

In some cases, you may not be interested in torrents of a certain type, size or language, and you want the scanner to ignore them, and your database does not save them. To do this, use settings area for the settings of torrent filter

[![Filters](img/filters.png)](https://github.com/DEgITx/rats-search)

To do this, select the appropriate settings and save them. After that, depending on the need, you can check and clean the database in accordance with the new settings.