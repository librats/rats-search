const { createApp } = Vue;

const app = createApp({
    data() {
        return {
            locale: localStorage.getItem('locale') || 'en',
            searchQuery: '',
            lastQuery: '',
            searching: false,
            searched: false,
            results: [],
            stats: null,
            p2pStatus: { connectedPeers: 0, dhtNodes: 0 },
            selectedTorrent: null,
            torrentFiles: [],
            activeTab: 'top',
            topTorrents: [],
            feedTorrents: [],
            downloads: [],
            favorites: [],
            config: null,
            darkMode: true,
            contextMenu: { visible: false, x: 0, y: 0, torrent: null },
            dragging: false,
            activityQueue: [],
            activityPaused: false,
            activityTimer: null,
            toasts: [],
            toastId: 0,
            ws: null,
            lastVote: null,
            sortKey: '',
            sortDir: 1,
            sortSource: '',
            settingsTab: 'general',
            cleanupStatus: ''
        };
    },

    async mounted() {
        const boot = document.getElementById('boot');
        if (boot) { boot.style.opacity = '0'; setTimeout(() => boot.remove(), 300); }

        const savedTheme = localStorage.getItem('theme');
        this.darkMode = savedTheme ? savedTheme === 'dark' : true;
        this.applyTheme();

        await this.loadStats();
        await this.loadP2PStatus();
        await this.loadTopTorrents();
        await this.loadFeed();
        await this.loadFavorites();
        await this.loadConfig();
        if (this.config && this.config.language) {
            this.setLocale(this.config.language);
        }

        setInterval(() => {
            this.loadP2PStatus();
            if (this.activeTab === 'downloads') {
                this.loadDownloads();
            }
        }, 5000);

        document.addEventListener('keydown', this.handleKeydown);
        document.addEventListener('dragover', this.onGlobalDragOver);
        document.addEventListener('dragleave', this.onGlobalDragLeave);
        document.addEventListener('drop', this.onGlobalDrop);

        this.connectWebSocket();
    },

    beforeUnmount() {
        document.removeEventListener('keydown', this.handleKeydown);
        document.removeEventListener('dragover', this.onGlobalDragOver);
        document.removeEventListener('dragleave', this.onGlobalDragLeave);
        document.removeEventListener('drop', this.onGlobalDrop);
        if (this.ws) this.ws.close();
    },

    watch: {
        'config.language'(val) {
            if (val) this.setLocale(val);
        }
    },

    computed: {
        sortedTopTorrents() {
            return this.sortSource === 'top' ? this.applySorting(this.topTorrents) : this.topTorrents;
        },
        sortedFeedTorrents() {
            return this.sortSource === 'feed' ? this.applySorting(this.feedTorrents) : this.feedTorrents;
        },
        sortedResults() {
            return this.sortSource === 'search' ? this.applySorting(this.results) : this.results;
        },
        sortedFavorites() {
            return this.sortSource === 'favorites' ? this.applySorting(this.favorites) : this.favorites;
        },
        currentDict() {
            return I18N[this.locale] || {};
        }
    },

    methods: {
        t(key) {
            const dict = this.currentDict;
            return dict[key] || key;
        },

        toggleTheme() {
            this.darkMode = !this.darkMode;
            localStorage.setItem('theme', this.darkMode ? 'dark' : 'light');
            this.applyTheme();
        },

        applyTheme() {
            document.body.classList.toggle('light-theme', !this.darkMode);
        },

        setLocale(lang) {
            this.locale = lang;
            localStorage.setItem('locale', lang);
        },

        switchTab(tab) {
            this.activeTab = tab;
            if (tab !== 'search') {
                this.searched = false;
                this.results = [];
            }
            if (tab === 'downloads') {
                this.loadDownloads();
            }
        },

        async search() {
            if (!this.searchQuery.trim() || this.searching) return;

            this.searching = true;
            this.searched = true;
            this.lastQuery = this.searchQuery;
            this.results = [];

            try {
                const response = await fetch(`/api/search.torrents?text=${encodeURIComponent(this.searchQuery)}&limit=50`);
                const data = await response.json();

                if (data.success && data.data) {
                    this.results = data.data;
                }
            } catch (error) {
                console.error('Search error:', error);
            } finally {
                this.searching = false;
            }
        },

        clearSearch() {
            this.searched = false;
            this.searchQuery = '';
            this.results = [];
        },

        async loadStats() {
            try {
                const response = await fetch('/api/stats.get');
                const data = await response.json();

                if (data.success && data.data) {
                    this.stats = data.data;
                }
            } catch (error) {
                console.error('Stats error:', error);
            }
        },

        async loadP2PStatus() {
            try {
                const response = await fetch('/api/stats.p2pStatus');
                const data = await response.json();

                if (data.success && data.data) {
                    this.p2pStatus = {
                        connectedPeers: data.data.peerCount || 0,
                        dhtNodes: data.data.dhtNodes || 0
                    };
                }
            } catch (error) {
                console.error('P2P status error:', error);
            }
        },

        async loadTopTorrents() {
            try {
                const response = await fetch('/api/search.top?limit=20');
                const data = await response.json();

                if (data.success && data.data) {
                    this.topTorrents = data.data;
                }
            } catch (error) {
                console.error('Top torrents error:', error);
            }
        },

        async loadFeed() {
            try {
                const response = await fetch('/api/feed.get?index=0&limit=20');
                const data = await response.json();

                if (data.success && data.data) {
                    this.feedTorrents = data.data;
                }
            } catch (error) {
                console.error('Feed error:', error);
            }
        },

        async loadDownloads() {
            try {
                const response = await fetch('/api/download.list');
                const data = await response.json();

                if (data.success && data.data) {
                    this.downloads = data.data;
                }
            } catch (error) {
                console.error('Downloads error:', error);
            }
        },

        async loadFavorites() {
            try {
                const stored = localStorage.getItem('favorites');
                this.favorites = stored ? JSON.parse(stored) : [];
            } catch (error) {
                console.error('Favorites error:', error);
                this.favorites = [];
            }
        },

        saveFavorites() {
            localStorage.setItem('favorites', JSON.stringify(this.favorites));
        },

        isFavorite(hash) {
            return this.favorites.some(f => f.hash === hash);
        },

        toggleFavorite(torrent) {
            if (this.isFavorite(torrent.hash)) {
                this.removeFavorite(torrent.hash);
                this.showToast(this.t('toast.removedFav'), 'info');
            } else {
                this.addFavorite(torrent);
                this.showToast(this.t('toast.addedFav'), 'success');
            }
        },

        addFavorite(torrent) {
            if (!this.isFavorite(torrent.hash)) {
                this.favorites.push({
                    hash: torrent.hash,
                    name: torrent.name,
                    size: torrent.size,
                    seeders: torrent.seeders,
                    leechers: torrent.leechers,
                    contentType: torrent.contentType
                });
                this.saveFavorites();
            }
        },

        removeFavorite(hash) {
            this.favorites = this.favorites.filter(f => f.hash !== hash);
            this.saveFavorites();
        },

        async loadConfig() {
            try {
                const response = await fetch('/api/config.get');
                const data = await response.json();

                if (data.success && data.data) {
                    this.config = data.data;
                }
            } catch (error) {
                console.error('Config error:', error);
            }
        },

        async saveSettings() {
            if (!this.config) return;

            try {
                const response = await fetch('/api/config.set', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(this.config)
                });
                const data = await response.json();

                if (data.success) {
                    this.showToast(this.t('toast.settingsSaved'), 'success');
                } else {
                    this.showToast('Failed to save: ' + (data.error || 'Unknown error'), 'error');
                }
            } catch (error) {
                console.error('Save settings error:', error);
                this.showToast('Failed to save settings', 'error');
            }
        },

        async checkTorrents() {
            this.cleanupStatus = this.t('settings.checkingTorrents') || 'Checking...';
            try {
                const resp = await fetch('/api/torrent.cleanup?dryRun=true');
                const data = await resp.json();
                if (data.success && data.data) {
                    const matched = data.data.matched || 0;
                    const scanned = data.data.scanned || 0;
                    this.cleanupStatus = `${matched} of ${scanned} torrents don't match the current filters.`;
                } else {
                    this.cleanupStatus = data.error || 'Check failed';
                }
            } catch (e) {
                this.cleanupStatus = 'Check failed: ' + e.message;
            }
        },

        async cleanTorrents() {
            if (!confirm(this.t('settings.confirmClean') || 'Remove torrents that don\'t match the current filters?')) return;
            this.cleanupStatus = this.t('settings.cleaningTorrents') || 'Cleaning...';
            try {
                const resp = await fetch('/api/torrent.cleanup?dryRun=false');
                const data = await resp.json();
                if (data.success && data.data) {
                    const matched = data.data.matched || 0;
                    this.cleanupStatus = `Removed ${matched} torrents that didn't match the filters.`;
                } else {
                    this.cleanupStatus = data.error || 'Clean failed';
                }
            } catch (e) {
                this.cleanupStatus = 'Clean failed: ' + e.message;
            }
        },

        isContentTypeEnabled(type) {
            if (!this.config || !this.config.filters) return true;
            const ct = this.config.filters.contentType || '';
            if (ct === '') return true;
            return ct.split(',').includes(type);
        },

        toggleContentType(type, event) {
            if (!this.config || !this.config.filters) return;
            let ct = this.config.filters.contentType || '';
            const types = ct ? ct.split(',') : [];

            if (event.target.checked) {
                if (!types.includes(type)) types.push(type);
            } else {
                const idx = types.indexOf(type);
                if (idx >= 0) types.splice(idx, 1);
            }

            this.config.filters.contentType = types.join(',');
        },

        async pauseDownload(hash) {
            try {
                await fetch(`/api/download.pause?hash=${hash}`);
                await this.loadDownloads();
            } catch (error) {
                console.error('Pause error:', error);
            }
        },

        async resumeDownload(hash) {
            try {
                await fetch(`/api/download.resume?hash=${hash}`);
                await this.loadDownloads();
            } catch (error) {
                console.error('Resume error:', error);
            }
        },

        async cancelDownload(hash) {
            try {
                await fetch(`/api/download.remove?hash=${hash}`);
                await this.loadDownloads();
                this.showToast(this.t('toast.downloadCancelled'), 'info');
            } catch (error) {
                console.error('Cancel error:', error);
            }
        },

        async showDetails(torrent) {
            this.selectedTorrent = torrent;
            this.torrentFiles = [];

            try {
                const response = await fetch(`/api/torrent.get?hash=${torrent.hash}&files=true`);
                const data = await response.json();

                if (data.success && data.data) {
                    this.selectedTorrent = data.data;
                    if (data.data.filesList) {
                        this.torrentFiles = data.data.filesList;
                    }
                }
            } catch (error) {
                console.error('Details error:', error);
            }
        },

        async exportTorrent(torrent) {
            if (!torrent || !torrent.hash) return;

            try {
                const response = await fetch(`/api/torrent.export?hash=${torrent.hash}`);
                const data = await response.json();

                if (data.success && data.data && data.data.path) {
                    const fileName = data.data.name || torrent.hash;
                    const downloadUrl = `/download/${torrent.hash.toLowerCase()}.torrent`;

                    const a = document.createElement('a');
                    a.href = downloadUrl;
                    a.download = `${fileName}.torrent`;
                    document.body.appendChild(a);
                    a.click();
                    document.body.removeChild(a);
                    this.showToast(this.t('toast.exported'), 'success');
                } else {
                    this.showToast(data.error || 'Failed to export torrent', 'error');
                }
            } catch (error) {
                console.error('Export error:', error);
                this.showToast('Failed to export torrent', 'error');
            }
        },

        formatSize(bytes) {
            if (bytes === null || bytes === undefined || bytes === 0) return '0 B';

            const units = ['B', 'KB', 'MB', 'GB', 'TB', 'PB'];
            const k = 1024;
            const i = Math.floor(Math.log(bytes) / Math.log(k));

            return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + units[i];
        },

        formatSpeed(bytesPerSec) {
            if (!bytesPerSec || bytesPerSec === 0) return '0 B/s';
            return this.formatSize(bytesPerSec) + '/s';
        },

        getMagnetLink(torrent) {
            if (!torrent || !torrent.hash) return '';
            const name = encodeURIComponent(torrent.name || '');
            return `magnet:?xt=urn:btih:${torrent.hash}&dn=${name}`;
        },

        getTypeIcon(type) {
            if (!type) return '?';
            const t = type.toLowerCase();
            if (t.includes('video')) return '🎬';
            if (t.includes('audio') || t.includes('music')) return '🎵';
            if (t.includes('image') || t.includes('picture')) return '🖼';
            if (t.includes('book') || t.includes('document')) return '📖';
            if (t.includes('archive') || t.includes('zip')) return '📦';
            if (t.includes('software') || t.includes('app')) return '💿';
            return '📄';
        },

        getTypeColor(type) {
            if (!type) return '#666';
            const t = type.toLowerCase();
            const colors = {
                video: '#1ee359', audio: '#1e94e3', music: '#1e94e3',
                pictures: '#e31ebc', image: '#e31ebc',
                books: '#e3d91e', book: '#e3d91e',
                software: '#e3561e', application: '#e3561e', app: '#e3561e',
                games: '#9c27b0', game: '#9c27b0',
                archive: '#1e25e3', zip: '#1e25e3',
                disc: '#1ee381'
            };
            for (const [key, color] of Object.entries(colors)) {
                if (t.includes(key)) return color;
            }
            return '#666';
        },

        getSeedersClass(seeders) {
            if (seeders > 50) return 'seed-high';
            if (seeders > 10) return 'seed-mid';
            if (seeders > 0) return 'seed-low';
            return 'seed-zero';
        },

        getLeechersClass(leechers) {
            if (leechers > 50) return 'leech-high';
            if (leechers > 10) return 'leech-mid';
            if (leechers > 0) return 'leech-low';
            return 'leech-zero';
        },

        getRatingPercent() {
            const good = this.selectedTorrent?.good || 0;
            const bad = this.selectedTorrent?.bad || 0;
            const total = good + bad;
            if (total === 0) return 50;
            return Math.round((good / total) * 100);
        },

        getRatingText() {
            const good = this.selectedTorrent?.good || 0;
            const bad = this.selectedTorrent?.bad || 0;
            const total = good + bad;
            if (total === 0) return 'N/A';
            return this.getRatingPercent() + '%';
        },

        formatDate(ts) {
            if (!ts) return '';
            const d = new Date(typeof ts === 'number' ? ts * 1000 : ts);
            return d.toLocaleDateString();
        },

        async voteGood() {
            if (!this.selectedTorrent) return;
            try {
                await fetch(`/api/vote.cast?hash=${this.selectedTorrent.hash}&good=true`);
                this.lastVote = 'good';
                this.showToast(this.t('toast.voteGood'), 'success');
            } catch (e) {
                this.showToast('Vote failed', 'error');
            }
        },

        async voteBad() {
            if (!this.selectedTorrent) return;
            try {
                await fetch(`/api/vote.cast?hash=${this.selectedTorrent.hash}&good=false`);
                this.lastVote = 'bad';
                this.showToast(this.t('toast.voteBad'), 'info');
            } catch (e) {
                this.showToast('Vote failed', 'error');
            }
        },

        async startDownload(torrent) {
            if (!torrent || !torrent.hash) return;
            try {
                const resp = await fetch(`/api/download.add?hash=${torrent.hash}`);
                const data = await resp.json();
                if (data.success) {
                    this.showToast(this.t('toast.downloadStarted'), 'success');
                    this.loadDownloads();
                } else {
                    this.showToast(data.error || 'Failed', 'error');
                }
            } catch (e) {
                this.showToast('Failed to start download', 'error');
            }
        },

        async copyHash(torrent) {
            if (!torrent || !torrent.hash) return;
            try {
                await navigator.clipboard.writeText(torrent.hash);
                this.showToast('Hash copied', 'success');
            } catch (e) {
                this.showToast('Copy failed', 'error');
            }
        },

        async copyMagnetLink(torrent) {
            const link = this.getMagnetLink(torrent);
            try {
                await navigator.clipboard.writeText(link);
                this.showToast(this.t('toast.magnetCopied'), 'success');
            } catch (err) {
                const textArea = document.createElement('textarea');
                textArea.value = link;
                document.body.appendChild(textArea);
                textArea.select();
                document.execCommand('copy');
                document.body.removeChild(textArea);
                this.showToast(this.t('toast.magnetCopied'), 'success');
            }
        },

        // === Context Menu ===

        openContextMenu(event, torrent) {
            this.contextMenu = {
                visible: true,
                x: Math.min(event.clientX, window.innerWidth - 220),
                y: Math.min(event.clientY, window.innerHeight - 250),
                torrent: torrent
            };
        },

        hideContextMenu() {
            this.contextMenu.visible = false;
        },

        contextOpenMagnet() {
            if (this.contextMenu.torrent) {
                window.open(this.getMagnetLink(this.contextMenu.torrent), '_blank');
            }
            this.hideContextMenu();
        },

        contextCopyHash() {
            if (this.contextMenu.torrent) {
                navigator.clipboard.writeText(this.contextMenu.torrent.hash).then(() => {
                this.showToast(this.t('toast.hashCopied'), 'success');
                });
            }
            this.hideContextMenu();
        },

        contextCopyMagnet() {
            if (this.contextMenu.torrent) {
                this.copyMagnetLink(this.contextMenu.torrent);
            }
            this.hideContextMenu();
        },

        contextToggleFavorite() {
            if (this.contextMenu.torrent) {
                this.toggleFavorite(this.contextMenu.torrent);
            }
            this.hideContextMenu();
        },

        contextExport() {
            if (this.contextMenu.torrent) {
                this.exportTorrent(this.contextMenu.torrent);
            }
            this.hideContextMenu();
        },

        contextShowDetails() {
            if (this.contextMenu.torrent) {
                this.showDetails(this.contextMenu.torrent);
            }
            this.hideContextMenu();
        },

        // === Drag & Drop ===

        onDragOver(event) {
            event.preventDefault();
            this.dragging = true;
        },

        onGlobalDragOver(event) {
            event.preventDefault();
            if (event.dataTransfer && event.dataTransfer.types.includes('Files')) {
                this.dragging = true;
            }
        },

        onGlobalDragLeave(event) {
            if (event.clientX === 0 && event.clientY === 0) {
                this.dragging = false;
            }
        },

        onGlobalDrop(event) {
            this.dragging = false;
        },

        async handleDrop(event) {
            this.dragging = false;
            const files = event.dataTransfer.files;
            if (!files || files.length === 0) return;

            const torrentFiles = Array.from(files).filter(f => f.name.endsWith('.torrent'));
            if (torrentFiles.length === 0) {
                this.showToast('Only .torrent files can be indexed (place them in server torrent directory)', 'info');
                return;
            }

            this.showToast(`${torrentFiles.length} .torrent file(s) detected — use Ctrl+O to import from server path`, 'info');
        },

        // === Keyboard Shortcuts ===

        handleKeydown(event) {
            if (event.key === 'Escape') {
                if (this.selectedTorrent) {
                    this.selectedTorrent = null;
                } else if (this.contextMenu.visible) {
                    this.hideContextMenu();
                }
                return;
            }

            if (event.key === '/' && !this.isInputFocused()) {
                event.preventDefault();
                const input = document.querySelector('.search-box input');
                if (input) input.focus();
                return;
            }

            if ((event.ctrlKey || event.metaKey) && event.key === 'k') {
                event.preventDefault();
                const input = document.querySelector('.search-box input');
                if (input) input.focus();
                return;
            }

            if ((event.ctrlKey || event.metaKey) && event.key === 'o') {
                event.preventDefault();
                const path = prompt('Enter server path to .torrent file:');
                if (path) this.importTorrentByPath(path);
                return;
            }

            if ((event.ctrlKey || event.metaKey) && event.shiftKey && event.key === 'N') {
                event.preventDefault();
                this.switchTab('top');
                return;
            }

            if (event.key >= '1' && event.key <= '5' && !this.isInputFocused() && !event.ctrlKey && !event.metaKey) {
                const tabs = ['top', 'feed', 'activity', 'downloads', 'favorites'];
                const idx = parseInt(event.key) - 1;
                if (idx < tabs.length) {
                    this.switchTab(tabs[idx]);
                }
                return;
            }
        },

        isInputFocused() {
            const el = document.activeElement;
            return el && (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA' || el.isContentEditable);
        },

        sortBy(source, key) {
            if (this.sortKey === key) {
                this.sortDir *= -1;
            } else {
                this.sortKey = key;
                this.sortDir = 1;
            }
            this.sortSource = source;
        },

        applySorting(arr) {
            if (!this.sortKey) return arr;
            const key = this.sortKey;
            const dir = this.sortDir;
            return [...arr].sort((a, b) => {
                let va = a[key] ?? '';
                let vb = b[key] ?? '';
                if (typeof va === 'string') va = va.toLowerCase();
                if (typeof vb === 'string') vb = vb.toLowerCase();
                if (va < vb) return -1 * dir;
                if (va > vb) return 1 * dir;
                return 0;
            });
        },

        async importTorrentByPath(path) {
            try {
                const resp = await fetch(`/api/download.addFile?path=${encodeURIComponent(path)}`);
                const data = await resp.json();
                if (data.success) {
                    const name = data.data?.name || path.split('/').pop();
                    this.showToast(`"${name}" imported`, 'success');
                } else {
                    this.showToast(data.error || 'Failed to import', 'error');
                }
            } catch (e) {
                this.showToast('Failed to import torrent', 'error');
            }
        },

        // === Activity Stream ===

        connectWebSocket() {
            const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
            const httpPort = parseInt(location.port) || (location.protocol === 'https:' ? 443 : 80);
            const wsPort = httpPort + 1;
            const url = `${protocol}//${location.hostname}:${wsPort}`;

            try {
                this.ws = new WebSocket(url);

                this.ws.onmessage = (event) => {
                    try {
                        const msg = JSON.parse(event.data);
                        const eventName = msg.event || msg.method;
                        if (eventName === 'torrentIndexed' && msg.data) {
                            this.onTorrentIndexed(msg.data);
                        }
                        if (eventName === 'downloadProgress' && msg.data) {
                            this.onDownloadProgress(msg.data);
                        }
                        if (eventName === 'downloadCompleted' && msg.data) {
                            this.showToast(`Download completed: ${msg.data.name || msg.data.hash}`, 'success');
                        }
                    } catch (e) {
                        // not JSON or not our format
                    }
                };

                this.ws.onclose = () => {
                    setTimeout(() => this.connectWebSocket(), 5000);
                };

                this.ws.onerror = () => {
                    this.ws.close();
                };
            } catch (e) {
                console.error('WebSocket error:', e);
            }
        },

        onTorrentIndexed(torrent) {
            if (this.activityPaused) return;

            this.activityQueue.unshift({
                hash: torrent.hash,
                name: torrent.name || 'Unknown',
                size: torrent.size || 0,
                files: torrent.files || 0,
                seeders: torrent.seeders || 0,
                leechers: torrent.leechers || 0,
                contentType: torrent.contentType || '',
                _indexed: true
            });

            if (this.activityQueue.length > 100) {
                this.activityQueue = this.activityQueue.slice(0, 100);
            }
        },

        onDownloadProgress(params) {
            const idx = this.downloads.findIndex(d => d.hash === params.hash);
            if (idx >= 0) {
                Object.assign(this.downloads[idx], params);
            }
        },

        // === Toast Notifications ===

        showToast(message, type = 'info') {
            const id = ++this.toastId;
            this.toasts.push({ id, message, type });

            setTimeout(() => {
                this.toasts = this.toasts.filter(t => t.id !== id);
            }, 3000);
        }
    }
});

app.mount('#app');
