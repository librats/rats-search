# Changelog

All notable changes to this project will be documented in this file.

## [2.0.17](https://github.com/DEgITx/rats-search/compare/v2.0.16...v2.0.17) (2026-02-14)


### 🐛 Bug Fixes


- **tray:** Restored starting minimized support([65b5afe](https://github.com/DEgITx/rats-search/commit/65b5afe31c269cc9dc41a0b8a1fdd06ae0ee440e)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.16([311ed89](https://github.com/DEgITx/rats-search/commit/311ed8987e4214fa6344f199e48f922cb03985fa)) by @github-actions[bot]


### 🚀 Features


- **db:** Upgrade to manticore 17.5.1([ab54cf0](https://github.com/DEgITx/rats-search/commit/ab54cf0906bd74e5757df5c0218d018a64e0c9ac)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 2 commits


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.16](https://github.com/DEgITx/rats-search/compare/v2.0.15...v2.0.16) (2026-02-13)


### 🐛 Bug Fixes


- **p2p:** Fix stale problem during connection to p2p([e04ee39](https://github.com/DEgITx/rats-search/commit/e04ee39a5bf118e2a88b914013764623a82a2fe7)) by @DEgITx
- **p2p:** Fix program stuck at data sending between peers([9796c33](https://github.com/DEgITx/rats-search/commit/9796c332f327c1a761a56159e9b3776ad81fb61d)) by @DEgITx
- **p2p:** Fix memory leak on p2p disconnects([2677ff2](https://github.com/DEgITx/rats-search/commit/2677ff2a2c88159235e77ee5aa9bd5380a8569ba)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.15([ac798f7](https://github.com/DEgITx/rats-search/commit/ac798f7271cdd25b9336fcad2538b8707c3e5101)) by @github-actions[bot]


### 🚀 Features


- **migration:** Add legacy migrator for database([b5db97d](https://github.com/DEgITx/rats-search/commit/b5db97d0006f2d45775fe831466441f21d12c2a9)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 4 commits


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.15](https://github.com/DEgITx/rats-search/compare/v2.0.14...v2.0.15) (2026-02-10)


### 🐛 Bug Fixes


- **encoding:** Fix description encoding([3e566b7](https://github.com/DEgITx/rats-search/commit/3e566b77691d132cf403169e3867996b773cbda0)) by @DEgITx
- **cleanup:** More clean window (refactoring)([d76c880](https://github.com/DEgITx/rats-search/commit/d76c88085025d6ec9a2cf9f510a88cdefce1c29e)) by @DEgITx
- **db:** Fixed insert of some torrents([35987eb](https://github.com/DEgITx/rats-search/commit/35987ebae832778e37d1dd4ecd9a825b389a77be)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.14([9d59f5a](https://github.com/DEgITx/rats-search/commit/9d59f5af57f45be61646d6d83a95f6324f80258f)) by @github-actions[bot]


### 🚀 Features


- **scape:** Restored torrent scraper([cf7f5e0](https://github.com/DEgITx/rats-search/commit/cf7f5e030c021a32b25cb5665b96194256abcdfa)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 4 commits


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.14](https://github.com/DEgITx/rats-search/compare/v2.0.13...v2.0.14) (2026-02-09)


### 🐛 Bug Fixes


- **p2p:** Fix port initialization when ports are bust([257fc9d](https://github.com/DEgITx/rats-search/commit/257fc9d5db7da33838d7f8c5303626842c996d0c)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.13([7312089](https://github.com/DEgITx/rats-search/commit/7312089d6b078d579508e74fc61e3f7df8db4b0b)) by @github-actions[bot]


### 🚀 Features


- **p2p:** Info about peer torrents([cab1cb5](https://github.com/DEgITx/rats-search/commit/cab1cb5bc2185cf08c09f855e91b0557ce96cd26)) by @DEgITx
- **trackers:** Auto-update tracker info on click([c4dc2a9](https://github.com/DEgITx/rats-search/commit/c4dc2a935c8686fddb2377cc36827a89f60f8887)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 3 commits


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.13](https://github.com/DEgITx/rats-search/compare/v2.0.12...v2.0.13) (2026-02-08)


### 🐛 Bug Fixes


- **linux:** Better comparability for older repos([4d3296f](https://github.com/DEgITx/rats-search/commit/4d3296fd3b69e3ef48fb7138c85dedc0c60648c0)) by @DEgITx
- **feed:** Fix initialization order([1cdf27f](https://github.com/DEgITx/rats-search/commit/1cdf27f5722861f9c9dd55e57b4bcd7ca20cde4a)) by @DEgITx
- **p2p:** Some cleanup for connections([2c5b417](https://github.com/DEgITx/rats-search/commit/2c5b417edec28e25151476312a1f69e00012cb78)) by @DEgITx
- **network:** Small improvements([bad2e03](https://github.com/DEgITx/rats-search/commit/bad2e030cdcb54cc404368017e22f0e923401c24)) by @DEgITx
- **scan:** Use only target ip for metdata getting([e3d5b22](https://github.com/DEgITx/rats-search/commit/e3d5b2202dff1e370266d3ca3fd20bb808b70a27)) by @DEgITx
- **torrent:** Fix incorrect port during announce scanning([592695d](https://github.com/DEgITx/rats-search/commit/592695d6d9d27a9d16575dd2102ebd4601247db0)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.12([73f3ba0](https://github.com/DEgITx/rats-search/commit/73f3ba0d41d34cdb066ed663603116d3aa7f603f)) by @github-actions[bot]
- **readme:** Readme update([117cc13](https://github.com/DEgITx/rats-search/commit/117cc13d001d7b6d3a158d133c56db51561b1317)) by @DEgITx


### 🚀 Features


- **torrent:** Increase speed of metadata getting([36d27b2](https://github.com/DEgITx/rats-search/commit/36d27b2f52e6da91de20512e27b06759c3cf5054)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 8 commits


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.12](https://github.com/DEgITx/rats-search/compare/v2.0.11...v2.0.12) (2026-02-04)


### 🐛 Bug Fixes


- **p2p:** Improved peer search logic on low number of peers([1d85ceb](https://github.com/DEgITx/rats-search/commit/1d85ceb9dc405f41da6a801f54a59f99661324e1)) by @DEgITx
- **detector:** Fix content detection on some cases according to v1([7b2b4cd](https://github.com/DEgITx/rats-search/commit/7b2b4cdc3a733a920120ec3791387a37fda06938)) by @DEgITx
- **feed:** Legal feed([451cf34](https://github.com/DEgITx/rats-search/commit/451cf349c68ba19d8c5af1d0e784b045bc032eb3)) by @DEgITx
- **migrator:** Fix migration version bounds([d4b15c3](https://github.com/DEgITx/rats-search/commit/d4b15c37ab8e113abd439a3e2c9452be1672c881)) by @DEgITx
- **migration:** Cleanup empty one one-time([e21abc7](https://github.com/DEgITx/rats-search/commit/e21abc7cc57e46f977c69b9b0b7c8f44b56daccb)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.11([307c3f3](https://github.com/DEgITx/rats-search/commit/307c3f37543a53db169a8031b035f65e489f8b5a)) by @github-actions[bot]


### 🚀 Features


- **agreement:** User agreement([80291b8](https://github.com/DEgITx/rats-search/commit/80291b81b61f20be1a725d357dd9c4bf2ec8c513)) by @DEgITx
- **changelog:** Dialog for changelog like in old version([fa5ea8f](https://github.com/DEgITx/rats-search/commit/fa5ea8fa9984b571492fe665efdc04041797c68e)) by @DEgITx
- **migration:** Restored migration module([a40f189](https://github.com/DEgITx/rats-search/commit/a40f18964cb8c6d3242ca0cd9610c02a65343ae9)) by @DEgITx
- **formats:** New formats support([cb3e8e3](https://github.com/DEgITx/rats-search/commit/cb3e8e33d0ec1aa21e2d565d8561fc54153f78f7)) by @DEgITx
- **p2p:** Our peer information for other clients([0a37815](https://github.com/DEgITx/rats-search/commit/0a37815ec28a7726635e0da9990d92ae9c8669f9)) by @DEgITx
- **p2p:** Protocol bump([6798557](https://github.com/DEgITx/rats-search/commit/6798557d840fb657219befb9423a9bef1d8690e7)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 11 commits


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.11](https://github.com/DEgITx/rats-search/compare/v2.0.10...v2.0.11) (2026-02-03)


### 🐛 Bug Fixes


- **icon:** Fix icon type display([45ae3a8](https://github.com/DEgITx/rats-search/commit/45ae3a866b3d147ee6bd104249b6d37d181ab739)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.10([ca66dcc](https://github.com/DEgITx/rats-search/commit/ca66dcce6ce91383c42b6473a29dc3e8dee95287)) by @github-actions[bot]


### 🚀 Features


- **log:** Session rotation([ab69d72](https://github.com/DEgITx/rats-search/commit/ab69d72d65237ec551eb5afe04584799f5fc625c)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 2 commits


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.10](https://github.com/DEgITx/rats-search/compare/v2.0.9...v2.0.10) (2026-02-02)


### 🐛 Bug Fixes


- **search:** Click on torrent always priority to download from remote peer([f9d8cb8](https://github.com/DEgITx/rats-search/commit/f9d8cb84166b101b0849e860a2d0634f8795cdce)) by @DEgITx
- **replication:** Update torrent counter properly([326b048](https://github.com/DEgITx/rats-search/commit/326b048f7e9f1cba4405004b736674105a952e2f)) by @DEgITx
- **performance:** Increase performance during replication when inserting torrent([54646ef](https://github.com/DEgITx/rats-search/commit/54646ef6b1ae306cc38f6ba4d8de030270b837d5)) by @DEgITx
- **replication:** Fix replication for remote torrent open([74ce61a](https://github.com/DEgITx/rats-search/commit/74ce61a8747cd8dbb64c43399e546c29687d20ae)) by @DEgITx
- **top:** Invalidate cache([b55b32a](https://github.com/DEgITx/rats-search/commit/b55b32af05d355887668f5e84986aed4304ba387)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.9([479f93b](https://github.com/DEgITx/rats-search/commit/479f93b4aaebc975f739a20d46829af84eb67589)) by @github-actions[bot]


### 🚀 Features


- **log:** More log for analyze([9abebe8](https://github.com/DEgITx/rats-search/commit/9abebe807de925e270466f990e29839848e76cf3)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 6 commits


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.9](https://github.com/DEgITx/rats-search/compare/v2.0.8...v2.0.9) (2026-02-01)


### 🐛 Bug Fixes


- **p2p:** Fix peers exchange for incoming connections([aaa68fc](https://github.com/DEgITx/rats-search/commit/aaa68fc3229a1ae67340a3c69a14ec1fa8ec652c)) by @DEgITx
- **ui:** Fix new line break for right panel to avoid horizontal size problems([dc2165b](https://github.com/DEgITx/rats-search/commit/dc2165b8b8cdad9ac3fea4914f69e96f3692ab26)) by @DEgITx
- **ui:** Removed activity tab - old one, not needed([f83110d](https://github.com/DEgITx/rats-search/commit/f83110d346ffaabb7281469b5538cf5ca690eb89)) by @DEgITx
- **ui:** Real status([382b36e](https://github.com/DEgITx/rats-search/commit/382b36e891b8a46b9b57155280f51d8bc09b6453)) by @DEgITx
- **ui:** Removed statistic tab - will be reworked in future([b6d708d](https://github.com/DEgITx/rats-search/commit/b6d708d854d1811ae15430faa6365f907e127ab0)) by @DEgITx
- **theme:** Fix focus on activity elements([6b6fb05](https://github.com/DEgITx/rats-search/commit/6b6fb05451c028255c47b2481e93777f9a843ec1)) by @DEgITx
- **tabs:** Fix switch torrents between tabs when they focused([894c232](https://github.com/DEgITx/rats-search/commit/894c2321c5453453796de5814aff8c356b17f767)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.8([1e5330d](https://github.com/DEgITx/rats-search/commit/1e5330db23da34d21eb467c5f1f65c59ce0a8cde)) by @github-actions[bot]


### 🚀 Features


- **ui:** New panel indication in the bottom (including green p2p indication)([dc42e40](https://github.com/DEgITx/rats-search/commit/dc42e405c337095ec13240e974fc1f7d520b88bd)) by @DEgITx
- **activity:** Restored activity tab([886f895](https://github.com/DEgITx/rats-search/commit/886f895d77bb418df787b012c3d382c95af4b089)) by @DEgITx
- **search:** Auto focus tab of search when searching([a2ffe5d](https://github.com/DEgITx/rats-search/commit/a2ffe5df6e1cab714d914b3dfd9a1da47996d07d)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 10 commits


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.8](https://github.com/DEgITx/rats-search/compare/v2.0.7...v2.0.8) (2026-02-01)


### 📚 Documentation


- Update CHANGELOG.md for v2.0.7([7287a01](https://github.com/DEgITx/rats-search/commit/7287a015c74afafd4a38bbf3bf694a8d01ca3f5a)) by @github-actions[bot]


### 🚀 Features


- **encryption:** Enable peers encryption([41ab7ec](https://github.com/DEgITx/rats-search/commit/41ab7ec8ffa39e973a342dc84ea844700adc7e98)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.7](https://github.com/DEgITx/rats-search/compare/v2.0.6...v2.0.7) (2026-01-31)


### 🐛 Bug Fixes


- **storage:** Fix p2p storage initialization([06a997d](https://github.com/DEgITx/rats-search/commit/06a997d9ae4a0419acf910c71fb9a4a97bf182ad)) by @DEgITx
- **vote:** Fix vote indication on vote page([bd08f1f](https://github.com/DEgITx/rats-search/commit/bd08f1f0b0e38c7abbc7f6b7b2d9b57139caf4a2)) by @DEgITx
- **feed:** Fix feed files getting([e5ed90a](https://github.com/DEgITx/rats-search/commit/e5ed90a294d663fb9b96dac7b7828cad0c3c324c)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.6([c0580be](https://github.com/DEgITx/rats-search/commit/c0580be72b5e3e2937785a3151fef1e0894f3097)) by @github-actions[bot]




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 3 commits


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.6](https://github.com/DEgITx/rats-search/compare/v2.0.5...v2.0.6) (2026-01-30)


### 🐛 Bug Fixes


- **p2p:** Fix discovery in parallel with spider([4fa02b6](https://github.com/DEgITx/rats-search/commit/4fa02b67c9a7af677ae1b07cb06c9ce5a7463535)) by @DEgITx
- **restore:** Fix naming restore of torrent download([5bb36bc](https://github.com/DEgITx/rats-search/commit/5bb36bcd6101859ea6412d3df52287a7b2656b5d)) by @DEgITx
- **downloading:** Fix cancel save session([4ae76ff](https://github.com/DEgITx/rats-search/commit/4ae76ff0d37ed2a2baa641b72c8672a507b65010)) by @DEgITx
- **torrent:** Cleanup drop torrent old method([1c20081](https://github.com/DEgITx/rats-search/commit/1c2008182ab637582d78998c357f0dbdfc5611d3)) by @DEgITx
- **download:** Fix resuming folder creation([e22951f](https://github.com/DEgITx/rats-search/commit/e22951fdd8f36a1576fa2055d84caeffca8d77ca)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.5([9db28b2](https://github.com/DEgITx/rats-search/commit/9db28b2972ead7ab2cc024349197f7d3eafd6f8b)) by @github-actions[bot]


### 🚀 Features


- **ui:** Files stretch allow([62dc885](https://github.com/DEgITx/rats-search/commit/62dc885480992492ff1bece34a3c8870543af2ae)) by @DEgITx
- **ui:** Allow file panel to be bigger and right panel to be scrollable([254fa14](https://github.com/DEgITx/rats-search/commit/254fa143186c3a26d57c6fca406592904833d001)) by @DEgITx
- **download:** Settings for default download path([39ddabd](https://github.com/DEgITx/rats-search/commit/39ddabd0d182ba683351594e4f5004facc42ace0)) by @DEgITx
- **download:** Resume directory support([bb8ac3b](https://github.com/DEgITx/rats-search/commit/bb8ac3b3eefae9941995827ba68c6ccb1a835c02)) by @DEgITx
- **torrent:** Create/open torrents([37a1369](https://github.com/DEgITx/rats-search/commit/37a1369887902ea9c283dd6b20201e3dcca7a584)) by @DEgITx
- **torrent:** Drag & drop support restored([49e58f0](https://github.com/DEgITx/rats-search/commit/49e58f009a6408eb0779eed6551c3e29173a3bf3)) by @DEgITx
- **download:** Fix download resume([1a683a2](https://github.com/DEgITx/rats-search/commit/1a683a26b9a30e0b24a1bc226ba53f91315f52c0)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 12 commits


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.5](https://github.com/DEgITx/rats-search/compare/v2.0.4...v2.0.5) (2026-01-29)


### 🐛 Bug Fixes


- **replication:** Fix missing files struct in replication procedure([98014b8](https://github.com/DEgITx/rats-search/commit/98014b8bb6b7b0d7cd5ac5959d17127348a77a5e)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.4([0648018](https://github.com/DEgITx/rats-search/commit/0648018dfbebcf1d7a48074a51958d6b950dad73)) by @github-actions[bot]




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.4](https://github.com/DEgITx/rats-search/compare/v2.0.3...v2.0.4) (2026-01-29)


### 🐛 Bug Fixes


- **connection:** Fix CLI ports override([d5b272c](https://github.com/DEgITx/rats-search/commit/d5b272cd2513304b0fbe64977acdf3454feb3cd8)) by @DEgITx
- **logs:** Increase log level for important logs([40e1c61](https://github.com/DEgITx/rats-search/commit/40e1c6182c1b039121ba34183cb9d7181b726ea5)) by @DEgITx
- **ui:** Fix closing application when it already hidden to tray([f1db0c7](https://github.com/DEgITx/rats-search/commit/f1db0c75476cd8d90dc5b6e2c0cf23907ba85435)) by @DEgITx
- **ui:** Fix downloading progress display on right panel([59aa890](https://github.com/DEgITx/rats-search/commit/59aa890c078e4a546ea624ceaba123185c3db730)) by @DEgITx
- **downloading:** Fix deadlock situation on status update([11fd84c](https://github.com/DEgITx/rats-search/commit/11fd84c7aaca7e3fe92fd75e3ed4646386ae5811)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.3([28829aa](https://github.com/DEgITx/rats-search/commit/28829aa0f4ae5dff6cdb06e2a4c381f5734de493)) by @github-actions[bot]


### 🚀 Features


- **ui:** Display files search from torrent restore #237([0f21f64](https://github.com/DEgITx/rats-search/commit/0f21f649b78d54adecc4c558324f463ac9913423)) by @DEgITx
- **ui:** Show tray notification only first time([def7030](https://github.com/DEgITx/rats-search/commit/def70305a9aa50869914f9060390c09d53f8126d)) by @DEgITx
- **ui:** Display downloading on details panel([d67c2de](https://github.com/DEgITx/rats-search/commit/d67c2de73e2c9e27a936da1b6863d352ec9f7231)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 8 commits


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.3](https://github.com/DEgITx/rats-search/compare/v2.0.2...v2.0.3) (2026-01-28)


### 🐛 Bug Fixes


- **ui:** Removed useless buttons with refresh([c150322](https://github.com/DEgITx/rats-search/commit/c150322cec4c9646597d4636ace8c5288e1457dd)) by @DEgITx
- **windows:** Disable debug window for desktop in Windows([c68422e](https://github.com/DEgITx/rats-search/commit/c68422e8727f69a1891fdcd7be16b8a2cb9d6b39)) by @DEgITx
- **top:** Fix overall torrents on screen([2db217d](https://github.com/DEgITx/rats-search/commit/2db217d74fa80e039e8457cd574d1dc1269e6cf3)) by @DEgITx
- **ui:** Fix styling of search element based on dark/light theme([520ed6a](https://github.com/DEgITx/rats-search/commit/520ed6a0c6dfb64a69809a461b6b0985ab7d19a0)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.2([ff070ed](https://github.com/DEgITx/rats-search/commit/ff070ed725bcbb18824daac600098037fa35b5fe)) by @github-actions[bot]


### 🚀 Features


- **ui:** Files panel separation to the bottom([5db9806](https://github.com/DEgITx/rats-search/commit/5db9806811769b2ddcbe2a9ec9bd8b336fdb5e42)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 5 commits


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.2](https://github.com/DEgITx/rats-search/compare/v2.0.1...v2.0.2) (2026-01-27)


### 🐛 Bug Fixes


- **linux:** Trying to fix Linux AppImage build([f337c6c](https://github.com/DEgITx/rats-search/commit/f337c6c4f77eb9fadbfd35a752a261224fbc67ad)) by @DEgITx
- **linux:** Fix build on some linux systems([16e1313](https://github.com/DEgITx/rats-search/commit/16e1313a1d046e5e37cec3726aa4ffd1c009a66b)) by @DEgITx
- **download:** Fix metadata download handling([436c6d4](https://github.com/DEgITx/rats-search/commit/436c6d4a094638df9f832e33b5e996c644915a0a)) by @DEgITx
- **log:** Logs limited for release version to reduce disk usage and space #234([a002218](https://github.com/DEgITx/rats-search/commit/a0022184df33108c6192e158e750b84ee2247f43)) by @DEgITx
- **ui:** Fix crash on startup with top torrent tab([4413287](https://github.com/DEgITx/rats-search/commit/44132873d09287be15b893d29667b2c5dfe7cfab)) by @DEgITx
- **ui:** Fix some styles components in light theme([14770a9](https://github.com/DEgITx/rats-search/commit/14770a952c6811e63f133936ae55e1c5fa1ae57b)) by @DEgITx


### 📚 Documentation


- Update CHANGELOG.md for v2.0.1([d71eec5](https://github.com/DEgITx/rats-search/commit/d71eec512bf46195b3f667e308a6a1f81fddcc27)) by @github-actions[bot]




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 6 commits


- [@github-actions[bot]](https://github.com/github-actions[bot]) — 1 commit


## [2.0.1](https://github.com/DEgITx/rats-search/compare/v2.0.0...v2.0.1) (2026-01-23)


### 🏗️ Build


- Add tsan/asan sanitizers support([5a485a3](https://github.com/DEgITx/rats-search/commit/5a485a3243814a38aff18e9f7d8feb9071116e25)) by @DEgITx


### 🐛 Bug Fixes


- **build:** Version push for bundle([a71ed66](https://github.com/DEgITx/rats-search/commit/a71ed66d06415039eee511166258559864d152cc)) by @DEgITx
- **settings:** Apply settings issues([0791d5d](https://github.com/DEgITx/rats-search/commit/0791d5dca670f1b79cb540f3cfd77071b88ea2d9)) by @DEgITx
- **db:** Bad SQL request for feed([18ff7eb](https://github.com/DEgITx/rats-search/commit/18ff7eb4adf11905cf6a67666b936e95b241e964)) by @DEgITx
- **ui:** Transparent menu([6d541fd](https://github.com/DEgITx/rats-search/commit/6d541fd184b58fe89508ed24b16efe847550e277)) by @DEgITx
- **ui:** Center about dialog text([1a05e2b](https://github.com/DEgITx/rats-search/commit/1a05e2b23d19d85290692660cc6ca94f8a156436)) by @DEgITx
- Data directory path([35c84a0](https://github.com/DEgITx/rats-search/commit/35c84a049d813dbe5ff6c108ab2baf7195d0ea0e)) by @DEgITx
- **ui:** Button style([2096fce](https://github.com/DEgITx/rats-search/commit/2096fcecf4ba50a2244ef2d35354dd3429f2727e)) by @DEgITx
- **ui:** Light mode background fix([684845f](https://github.com/DEgITx/rats-search/commit/684845fbfc6dc66a9e434fbbca4a31703788cabd)) by @DEgITx


### 👷 CI


- Add git-cliff configuration([3d038a8](https://github.com/DEgITx/rats-search/commit/3d038a86d6359cb7f5fbf15730cda62c682296d8)) by @DEgITx


### 📚 Documentation


- Add contributors section([c8bb255](https://github.com/DEgITx/rats-search/commit/c8bb2556fd598b2a6d64c64edb7340e7b8d49298)) by @DEgITx


### 📦 Miscellaneous


- Update submodules to latest([95a89f2](https://github.com/DEgITx/rats-search/commit/95a89f20951d39a7c256900d704336dcdabbc281)) by @DEgITx
- Update submodules to latest([1ec5942](https://github.com/DEgITx/rats-search/commit/1ec5942cac6fe6f152312762b2846e048871cfc1)) by @DEgITx


### 🚀 Features


- **updater:** Add pattern archives support([ab7188c](https://github.com/DEgITx/rats-search/commit/ab7188c8cb3687ca7c14469351ed40f2eea77b51)) by @DEgITx
- **ui:** Improve dark/light modes([fc77ce3](https://github.com/DEgITx/rats-search/commit/fc77ce3274aab95fe7f9254d710842ff3c65a241)) by @DEgITx
- Add checkmarks([75e16ba](https://github.com/DEgITx/rats-search/commit/75e16ba20ad902b173a3cd00c2a10c1e844f3d20)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 16 commits


## [2.0.0](https://github.com/DEgITx/rats-search/compare/v1.11.0...v2.0.0) (2026-01-21)


### 🐛 Bug Fixes


- **readme:** New feature about rest api([78c6df5](https://github.com/DEgITx/rats-search/commit/78c6df5557ea687fb1a2f0956e884acc77c370a1)) by @DEgITx
- **imports:** Fix checking submodules to prevent error on start([03f0ab7](https://github.com/DEgITx/rats-search/commit/03f0ab7a475b17d6ba2461a939558276bedbd135)) by @DEgITx
- **core:** Fix support of manticore 6.3 and more recent versions([7c0d5ca](https://github.com/DEgITx/rats-search/commit/7c0d5cadf859a6b5ec2c78c34c9a3fcb6ed14d71)) by @DEgITx
- **tests:** Fix windows passing([fedc9ed](https://github.com/DEgITx/rats-search/commit/fedc9edf19244d43a776e5773cfbd408bcf84e38)) by @DEgITx
- **docker:** Upgrade docker image([e5a219a](https://github.com/DEgITx/rats-search/commit/e5a219a98c5fbe6749394ecb703a5d09105f3714)) by @DEgITx


### 🚀 Features


- **core:** Dark mode basic feature support #187([320e169](https://github.com/DEgITx/rats-search/commit/320e169fccf12d6cbe52b58f186b671192285ea5)) by @DEgITx
- **config:** Better handling of config changes([280298c](https://github.com/DEgITx/rats-search/commit/280298caa3114c4582a02eba08afd2abf873e2d7)) by @DEgITx
- **db:** Update to manticore search 6.0.4([3cff91d](https://github.com/DEgITx/rats-search/commit/3cff91dbf1b63938c716dc95485f6553003d31ae)) by @DEgITx
- **p2p:** Publish manticore info([9d17fd2](https://github.com/DEgITx/rats-search/commit/9d17fd28d109098b2ae605bb7e566ed442815ee1)) by @DEgITx
- **rest:** Basic REST API support for servers as alternative to websocks([fad7c33](https://github.com/DEgITx/rats-search/commit/fad7c338c957a0b1f62ada9df25409e93cebef5d)) by @DEgITx
- **rest:** Queue for async messages /api/queue([f9ec0c2](https://github.com/DEgITx/rats-search/commit/f9ec0c2cbbf6db2f7bf1a4e91435556e5434ca0b)) by @DEgITx
- **core:** Update core libs([e07a02f](https://github.com/DEgITx/rats-search/commit/e07a02fe3267b3e801f98f7f17676bb6f517fd0c)) by @DEgITx
- **core:** Update deps([a0ce313](https://github.com/DEgITx/rats-search/commit/a0ce31350e5360aeb305850be5fcfa61645f0220)) by @DEgITx
- **core:** Upgrade to new deps([e933ccd](https://github.com/DEgITx/rats-search/commit/e933ccd9b38f589e78eac0712abc67d0a2945294)) by @DEgITx
- **ci:** Upgrade matrix to 16 and 20 node([c6fb486](https://github.com/DEgITx/rats-search/commit/c6fb4869ca589f5276fd99f704318d0a76b91cf0)) by @DEgITx
- **core:** Upgrade([bdc51d7](https://github.com/DEgITx/rats-search/commit/bdc51d72e8e9cacaa8dbebd60676fdc98bbf8fb2)) by @DEgITx
- **core:** Update core components([4142146](https://github.com/DEgITx/rats-search/commit/4142146c51e3d19cee79293b47586d7637277055)) by @DEgITx
- **core:** Upgrade core deps([cc47e55](https://github.com/DEgITx/rats-search/commit/cc47e550c1b2b76e45b735f8a8ff838728b25f42)) by @DEgITx
- **server:** Exit codes for rats server([43cac0c](https://github.com/DEgITx/rats-search/commit/43cac0cf8bed3bfefbf641b28c121cef669e878a)) by @DEgITx
- **test:** Fix crash of tests on unix systems([dadb07c](https://github.com/DEgITx/rats-search/commit/dadb07c52f2aa4a5f0c8e8ce1efa5147e0f7ce4f)) by @DEgITx
- **tests:** Special debug log print after tests failed([983d06d](https://github.com/DEgITx/rats-search/commit/983d06d1345a6510a1004a20770fb44325800123)) by @DEgITx
- **p2p:** New p2p protocol([c196f82](https://github.com/DEgITx/rats-search/commit/c196f82834cc7ecae052e98d2cf1931ad8117e7d)) by @DEgITx
- **translations:** Support de, es, fr translations([d59fda7](https://github.com/DEgITx/rats-search/commit/d59fda7e77d5f5528a2a50a511e49690a29232fa)) by @DEgITx
- **translations:** New translations hi, it, ja, pt([d6782a6](https://github.com/DEgITx/rats-search/commit/d6782a6b40d548a2b50ec31596aef33ced4fe810)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 24 commits


## [1.11.0](https://github.com/DEgITx/rats-search/compare/v1.10.0...v1.11.0) (2023-05-21)


### 🐛 Bug Fixes


- **ci:** Fix linux build([e2eed07](https://github.com/DEgITx/rats-search/commit/e2eed076de7216dd9c238c401e598a9c144f29c1)) by @DEgITx
- **torrent:** Fix adding torrent for new manticore version (duplicate field)([cebb462](https://github.com/DEgITx/rats-search/commit/cebb462c53a11dd2e57e3b9302b4b27338f42d29)) by @DEgITx
- **config:** Proper config debug([5679340](https://github.com/DEgITx/rats-search/commit/56793400389e73f2ce96f0eb4c4b5f91d539631c)) by @DEgITx


### 🚀 Features


- **core:** Update deps([4a3c98c](https://github.com/DEgITx/rats-search/commit/4a3c98c013f3c9c5079a4a9af3d293f0c3673e9f)) by @DEgITx
- **log:** Improve logging format([0ae124d](https://github.com/DEgITx/rats-search/commit/0ae124daadfa898f3c1bcc429653189ddaa828fd)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 5 commits


## [1.10.0](https://github.com/DEgITx/rats-search/compare/v1.9.0...v1.10.0) (2023-02-26)


### 🐛 Bug Fixes


- **db:** Fix replace values func with new manticore([f1119b5](https://github.com/DEgITx/rats-search/commit/f1119b55168035940c9f539b2a915a46fba083fa)) by @DEgITx
- **docker:** Update install command([b611e9f](https://github.com/DEgITx/rats-search/commit/b611e9fed35654b1a8b46d5df10b64baa02e7d39)) by @DEgITx
- **docker:** Fix build([de79321](https://github.com/DEgITx/rats-search/commit/de79321c5f3da9faa7af02c0e1e36de0b7a044d4)) by @DEgITx
- **docker:** More fixes([411db7b](https://github.com/DEgITx/rats-search/commit/411db7ba1f6f3dc4a28d5c55444b732d89111481)) by @DEgITx
- **docker:** Fixes([381d120](https://github.com/DEgITx/rats-search/commit/381d1204a7e278cb07f5724d2f5a091728e5d6af)) by @DEgITx


### 📚 Documentation


- **readme:** Added docker instruction #178([da62eda](https://github.com/DEgITx/rats-search/commit/da62edad2a642d6f9b92cfe0d999d00b91bc6bd7)) by @DEgITx


### 🚀 Features


- **core:** Update libs([8c3b5a1](https://github.com/DEgITx/rats-search/commit/8c3b5a1c1fda3bb92744fceed1ecc18a751e4b41)) by @DEgITx
- **core:** Update libs([c2330a5](https://github.com/DEgITx/rats-search/commit/c2330a5431cabca95fff49590cb5817652da0ca2)) by @DEgITx
- **win:** Switch to manticore 5.0.2([cf47d25](https://github.com/DEgITx/rats-search/commit/cf47d25d67c86ec767f8ecae1528a5b6f3d73acc)) by @DEgITx
- **db:** Changed config for manticore 5 support #176([156786b](https://github.com/DEgITx/rats-search/commit/156786b41c5cd838d1a44c4c308ea1c74479ca35)) by @DEgITx
- **macos:** Manticore5 for macos([2ce6056](https://github.com/DEgITx/rats-search/commit/2ce6056ecf6b97ac20bf0d2977e546dd6cd04333)) by @DEgITx
- **linux:** Manticore5([7ce5118](https://github.com/DEgITx/rats-search/commit/7ce5118f157076b45dfa006174a5438ed38d46d5)) by @DEgITx
- **db:** Update to manticore 6.0.2([2d975da](https://github.com/DEgITx/rats-search/commit/2d975da2c9621db69834de30515d80278b07a715)) by @DEgITx
- **macos:** Support of arm64 manticore([3e9ca46](https://github.com/DEgITx/rats-search/commit/3e9ca464c475d0be78512325860100e0256ef794)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 14 commits


## [1.9.0](https://github.com/DEgITx/rats-search/compare/v1.8.1...v1.9.0) (2022-08-24)


### 🐛 Bug Fixes


- **torrent:** Fix search by magnet([bf6b88a](https://github.com/DEgITx/rats-search/commit/bf6b88aaaacabf910815b7bd8e52ecb38a5cc4fb)) by @DEgITx
- **ci:** Restore installation on ci([5c269af](https://github.com/DEgITx/rats-search/commit/5c269af2cbfc3bd18ae40046d234b9a0161d66a2)) by @DEgITx
- **build:** Build only x64 packages for linux([8752809](https://github.com/DEgITx/rats-search/commit/8752809829289173cfc98b1502f1512bb5c79715)) by @DEgITx
- **ssl:** Ignore cert to fix blob loads([b40df5b](https://github.com/DEgITx/rats-search/commit/b40df5b2cd8e88aeb99672b88eb30bad1a0d6148)) by @DEgITx


### 🚀 Features


- **core:** Upgrade to electron 17([657d49f](https://github.com/DEgITx/rats-search/commit/657d49f453fcafef8b8f6251659ef938987f0bd7)) by @DEgITx
- **core:** Update electron([b57073e](https://github.com/DEgITx/rats-search/commit/b57073e026d893db5f0cdeb043e78ea576346b7d)) by @DEgITx
- **core:** Update deps([0e92916](https://github.com/DEgITx/rats-search/commit/0e9291616a3744717e9222e9e45009c013ce5b47)) by @DEgITx
- **core:** Update([4732266](https://github.com/DEgITx/rats-search/commit/473226651429e8301e5cac30fdf52572b1afb1b4)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 8 commits


## [1.8.1](https://github.com/DEgITx/rats-search/compare/v1.8.0...v1.8.1) (2021-09-22)


### 🐛 Bug Fixes


- **windows:** Missed library #154([e77b0e4](https://github.com/DEgITx/rats-search/commit/e77b0e4289d24338bf0eab4bd0e02472e284e86f)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


## [1.8.0](https://github.com/DEgITx/rats-search/compare/v1.7.1...v1.8.0) (2021-09-12)


### ⚡ Performance


- **db:** Optimize some tables to stored_only_fields to recrudesce memory usage of big databases #152([d65f214](https://github.com/DEgITx/rats-search/commit/d65f214d4d2ad32badb8cd477e8e5e3bbc5e96e8)) by @DEgITx


### 🐛 Bug Fixes


- **linux:** Add execute right to searchd.v2 #152([ae6fabc](https://github.com/DEgITx/rats-search/commit/ae6fabc82f4c527691da5c5914c22201b78627b3)) by @DEgITx
- **linux:** Fix convertation of db under linux system #152([7225af8](https://github.com/DEgITx/rats-search/commit/7225af81d0aa63818fde87867c26736bccdd0564)) by @DEgITx
- **docker:** Moved to 16 version([83a78c4](https://github.com/DEgITx/rats-search/commit/83a78c4e6c1366713613e8e99eb24909eb725a24)) by @DEgITx
- **db:** Moving content type to uint values([f716610](https://github.com/DEgITx/rats-search/commit/f7166103dfe68649de1027bc6e5f47de0f57376a)) by @DEgITx
- **db:** Converting db to version 8([177f8d2](https://github.com/DEgITx/rats-search/commit/177f8d2f110f3a1893dfb5a449cc20adba632a5e)) by @DEgITx


### 📚 Documentation


- **README:** Add information about web-version([9e3870f](https://github.com/DEgITx/rats-search/commit/9e3870ff440e29f98f1af781425a2c786e3aaed0)) by @DEgITx
- **README:** Add desktop section([f3e08b5](https://github.com/DEgITx/rats-search/commit/f3e08b5aa40fcda1bc7502e7335dbd2661b63db3)) by @DEgITx


### 🚀 Features


- **log:** Using tagslog([fa112ff](https://github.com/DEgITx/rats-search/commit/fa112ffd9c5a4753a2e553789ba7f6f931a14466)) by @DEgITx
- **core:** Upgrade to electron 16([30a6901](https://github.com/DEgITx/rats-search/commit/30a69010771eec0c1ae9bb1a8993688973b8b008)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 10 commits


## [1.7.1](https://github.com/DEgITx/rats-search/compare/v1.7.0...v1.7.1) (2021-07-12)


### 🐛 Bug Fixes


- **core:** Update to new version([5a8ee5b](https://github.com/DEgITx/rats-search/commit/5a8ee5bd5245f2829a80f817a7e16df9007bd51b)) by @DEgITx
- **web:** Fixed compilation with create-torrent #148([bceb070](https://github.com/DEgITx/rats-search/commit/bceb070b74412420b46b44b6dc19e881dcdbe237)) by @DEgITx
- **ui:** Fix blank screen on app start #147([45bf73d](https://github.com/DEgITx/rats-search/commit/45bf73d012c659fb751c492c447e751753ac6c21)) by @DEgITx


### 🚀 Features


- **server:** Missing rats.log functionality restored #145([e306c8d](https://github.com/DEgITx/rats-search/commit/e306c8d46c3bb3fea3ce2964313d3c07ea102d2c)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 4 commits


## [1.7.0](https://github.com/DEgITx/rats-search/compare/v1.6.0...v1.7.0) (2021-07-11)


### 🐛 Bug Fixes


- **db:** Fix order in some cases for future db structure([b4c2627](https://github.com/DEgITx/rats-search/commit/b4c2627ed50a5c8cf4e5d6f12e8e7af365ec517c)) by @DEgITx
- **tests:** Fix test on new db engine([52a448e](https://github.com/DEgITx/rats-search/commit/52a448ef0989c4c1231d2ca112300ad4be4c2301)) by @DEgITx
- **db:** Rename deprecated option([2ec7ad0](https://github.com/DEgITx/rats-search/commit/2ec7ad0501f8f3ed0f8903a89d3a45f17fdd0bc8)) by @DEgITx
- **ui:** Fix url for #141([0aaff50](https://github.com/DEgITx/rats-search/commit/0aaff5033329cf96a82c8e80a049c7deb0eb8f58)) by @DEgITx
- **donate:** Change patreon to opencollective([1686f3e](https://github.com/DEgITx/rats-search/commit/1686f3e0287364751344bbe4a5181be3289ec8c7)) by @DEgITx
- **ui:** Fix generate button size in ui([3e4e26f](https://github.com/DEgITx/rats-search/commit/3e4e26fa1230cd0bb1cc4b4bcfba7729787f5bfa)) by @DEgITx


### 🚀 Features


- **tests:** Support custom test to run([0e79ce4](https://github.com/DEgITx/rats-search/commit/0e79ce42ca3bfbb740619896d5211ca097b1a669)) by @DEgITx
- **db:** New db patch([8df02cb](https://github.com/DEgITx/rats-search/commit/8df02cb296d2ca5b60df0b5e250f0eaeef74c1b7)) by @DEgITx
- **db:** Switch to new manticore([8e4940e](https://github.com/DEgITx/rats-search/commit/8e4940ec521da6c3aa51e7466320e78ae028b154)) by @DEgITx
- **ui:** Click on torrent size opens torrent files([79f23d0](https://github.com/DEgITx/rats-search/commit/79f23d00bdac05d3e7fb9ca5edc8641ae0fda376)) by @DEgITx
- **formats:** Avif to formats([777cd2b](https://github.com/DEgITx/rats-search/commit/777cd2bd6257c440ac59817a95a113772426e388)) by @DEgITx
- **ui:** Show torrent pie of the content([2029c14](https://github.com/DEgITx/rats-search/commit/2029c142b7c9fd283ee0f8eccb5f8fa5faf7ba6e)) by @DEgITx
- **p2p:** New bootstrap pear, also fix problem with bootstrap timeout([0325d71](https://github.com/DEgITx/rats-search/commit/0325d714a5f5342b46ee5548ace863c3abc0887c)) by @DEgITx
- **discord:** Enable webhook for discord channel support([d772b7f](https://github.com/DEgITx/rats-search/commit/d772b7f9c45fda29c95e4af2668142e5ef970a4e)) by @DEgITx
- **torrent:** Generation of torrent functionality([9699f1e](https://github.com/DEgITx/rats-search/commit/9699f1eb7f3a4e74a07438d0d7b17baf4e4166dd)) by @DEgITx
- **generate:** Add generated torrent to search list after generation([b90de1d](https://github.com/DEgITx/rats-search/commit/b90de1dfc06a89bbff3c420041956dc3d1ca3365)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 16 commits


## [1.6.0](https://github.com/DEgITx/rats-search/compare/v1.5.0...v1.6.0) (2021-06-10)


### 🐛 Bug Fixes


- **download:** Fix download status in recent torrents([8b1fc51](https://github.com/DEgITx/rats-search/commit/8b1fc513ad70523aa3c29b202af02b6d1ec1dee9)) by @DEgITx
- **search:** Double search on DHT([619b3cc](https://github.com/DEgITx/rats-search/commit/619b3ccd32f12e716d8261f9a2a011b9cce3affa)) by @DEgITx
- **search:** Improve search on magnet queries([9d684b8](https://github.com/DEgITx/rats-search/commit/9d684b89a697b51c126263b8ad1f800636890ab8)) by @DEgITx
- **search:** Fix slow query wrong remote response on search([5a712f7](https://github.com/DEgITx/rats-search/commit/5a712f77b5a0da9937905a7d9e5de614c9960d63)) by @DEgITx
- **db:** First run start fix #142([48cc019](https://github.com/DEgITx/rats-search/commit/48cc01953c4e437d85ce80647bbfcf8ad32a475f)) by @DEgITx
- **build:** Fix build under windows([af91c9c](https://github.com/DEgITx/rats-search/commit/af91c9c294944e3a7ce9681fd032585a47f5e97d)) by @DEgITx
- **changelog:** Fix changelog show([48701df](https://github.com/DEgITx/rats-search/commit/48701df4c8eab964f0bbb9c2c0139681b097648d)) by @DEgITx


### 🚀 Features


- **test:** Details test([ac7e3cf](https://github.com/DEgITx/rats-search/commit/ac7e3cfe9f0ddcc146c479a7344065535a64fb76)) by @DEgITx
- **core:** Update components([9804ee7](https://github.com/DEgITx/rats-search/commit/9804ee7c6cbb3028f86fb262db3cf85512704731)) by @DEgITx
- **core:** Update all deps([9da27b5](https://github.com/DEgITx/rats-search/commit/9da27b5f22acc09e4560b882a24ca525ef102ebc)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 10 commits


## [1.5.0](https://github.com/DEgITx/rats-search/compare/v1.4.2...v1.5.0) (2021-01-14)


### 🐛 Bug Fixes


- **server:** Error handling for server #112([05a712e](https://github.com/DEgITx/rats-search/commit/05a712e6766819f68e7321d867025e42a589f776)) by @DEgITx
- **relay:** Ignore relays errors #112([be81f81](https://github.com/DEgITx/rats-search/commit/be81f810327b6ce7f44b22c2f5534e7170f67f95)) by @DEgITx
- **gui:** Connection status fix detection in some situations([2e05bc9](https://github.com/DEgITx/rats-search/commit/2e05bc9e682396612b30ee1aa979b381b44c4b9c)) by @DEgITx
- **relay:** Ignore some situations when relay not needed([ab5303e](https://github.com/DEgITx/rats-search/commit/ab5303ecaec616628cd9aa7fde63289bab3f3b3c)) by @DEgITx
- **tests:** Fix tests on new api([4b763c9](https://github.com/DEgITx/rats-search/commit/4b763c937762858afbf6014d9b5049c356931920)) by @DEgITx
- **changelog:** Fix changelog on new electron([8522f00](https://github.com/DEgITx/rats-search/commit/8522f0068593d710c05d0110240598f20fcf3af0)) by @DEgITx
- **web:** Fix web version([7c3ad12](https://github.com/DEgITx/rats-search/commit/7c3ad128191e1e12c6c2a9ca86ddcd9219e54376)) by @DEgITx
- **tests:** Fix timeout testing([1c8b3a3](https://github.com/DEgITx/rats-search/commit/1c8b3a36a2eae586cfd07c09d82dafe56ae3aa65)) by @DEgITx


### 🚀 Features


- **deps:** Switch to new electron([e3c20a6](https://github.com/DEgITx/rats-search/commit/e3c20a6fe2ae8bff5b554d852fe32796581df423)) by @DEgITx
- **update:** Update everything to new versions([95fc935](https://github.com/DEgITx/rats-search/commit/95fc9354385e1eb3842e5c38fe7842c581a28c36)) by @DEgITx
- **relay:** Bootstrap timeout([39d12f1](https://github.com/DEgITx/rats-search/commit/39d12f1d1ab11e200a71f671cb71cfc0b57e9769)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 11 commits


## [1.4.2](https://github.com/DEgITx/rats-search/compare/v1.4.1...v1.4.2) (2020-01-04)


### 🐛 Bug Fixes


- **feed:** Fix feed error #111([1316839](https://github.com/DEgITx/rats-search/commit/13168393ba64e2d93ff261f7950ed73576425af3)) by @DEgITx
- **log:** Exceptions log handling([5260d51](https://github.com/DEgITx/rats-search/commit/5260d514abe547f0988259af332f4c123b139b9d)) by @DEgITx
- **test:** Fix tests([9d191f6](https://github.com/DEgITx/rats-search/commit/9d191f665cca1f6fc3ba4c929fcd9c679071be96)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 3 commits


## [1.4.1](https://github.com/DEgITx/rats-search/compare/v1.4.0...v1.4.1) (2019-11-10)


### 🐛 Bug Fixes


- **tests:** Fix tests on new version([5ede693](https://github.com/DEgITx/rats-search/commit/5ede6935acdf1aa13eeeb7176b41448c945802a1)) by @DEgITx
- **download:** Fix download to directory after update([f121c11](https://github.com/DEgITx/rats-search/commit/f121c118a310e797f4e4155ef55365a4dad673bb)) by @DEgITx
- **settings:** Fix regression with folder selection #101([40e7aaa](https://github.com/DEgITx/rats-search/commit/40e7aaa2dbddb0b7a2f7a6ad2cc8e6946b900831)) by @DEgITx
- **gui:** Fix inresaction of search panel with other elements #102([aafc245](https://github.com/DEgITx/rats-search/commit/aafc245688e3b7953b86ae71e176fa95bd19c94c)) by @DEgITx
- **download:** Fix search for double download cases([d94656a](https://github.com/DEgITx/rats-search/commit/d94656a3472f8dadacf746004b87c06dd697fac6)) by @DEgITx
- **windows:** Fix some startup problem under windows platform([177d6ff](https://github.com/DEgITx/rats-search/commit/177d6ffc1a1c08cbfec9197d19726d96e59ce18e)) by @DEgITx


### 🧪 Testing


- **download:** Testing file folder download([bbc68d7](https://github.com/DEgITx/rats-search/commit/bbc68d73f456985b16384b22eb213d31ff5be0cd)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 7 commits


## [1.4.0](https://github.com/DEgITx/rats-search/compare/v1.2.2...v1.4.0) (2019-09-28)


### 🐛 Bug Fixes


- **rutr:** Test fix([e281163](https://github.com/DEgITx/rats-search/commit/e281163a1881edc12f5eb1081d9c560e8ea8e4c1)) by @DEgITx
- **search:** Fix error on some cases of torrent opening([faabeed](https://github.com/DEgITx/rats-search/commit/faabeed246ea3d763bbe74b2ae0e43a429c753c0)) by @DEgITx


### 🚀 Features


- **app:** Start app in minimized look([8daaa63](https://github.com/DEgITx/rats-search/commit/8daaa63b737efc4914cc9232c0f7e70ca3fdb46e)) by @DEgITx
- **deps:** Core update([e575e3f](https://github.com/DEgITx/rats-search/commit/e575e3f7c82d9ddd9c64c4cc54222dcf7a76b654)) by @DEgITx
- **feed:** Auto-loading feed list([5ccdbc0](https://github.com/DEgITx/rats-search/commit/5ccdbc01fead0d60f428ea01b51dda127a8fb3b7)) by @DEgITx
- **log:** Web/desktop log message([ca6f499](https://github.com/DEgITx/rats-search/commit/ca6f49932c43a1a112c4e82acf1fdfcfef195703)) by @DEgITx
- **gui:** Tip about torrent drops([bac310d](https://github.com/DEgITx/rats-search/commit/bac310d76685614f6d42b1540c4830feace39b26)) by @DEgITx
- **core:** Core update([52e9ba1](https://github.com/DEgITx/rats-search/commit/52e9ba1a4d3cf1281b23d2c7b251204f18320a7f)) by @DEgITx
- **drop:** Support drop torrent file from web version #99([d4f390d](https://github.com/DEgITx/rats-search/commit/d4f390dda712046da2c5faa8ad558c7770806ad2)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 9 commits


## [1.2.2](https://github.com/DEgITx/rats-search/compare/v1.2.1...v1.2.2) (2019-04-03)


### 🐛 Bug Fixes


- **p2p:** Additional myself check([829a12b](https://github.com/DEgITx/rats-search/commit/829a12be35c82c6586a2315c955037aa5895146b)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


## [1.2.1](https://github.com/DEgITx/rats-search/compare/v1.2.0...v1.2.1) (2019-04-03)


### 🐛 Bug Fixes


- **relay:** Priority to relays peers([620a6c0](https://github.com/DEgITx/rats-search/commit/620a6c0c5e181557c34552973f244d1e515e60ec)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


## [1.2.0](https://github.com/DEgITx/rats-search/compare/v1.1.4...v1.2.0) (2019-03-31)


### 🐛 Bug Fixes


- **p2p:** Additional check of dublicated peers([726afd7](https://github.com/DEgITx/rats-search/commit/726afd750c23be5ed69a02fff62c2c2f1fe7354f)) by @DEgITx
- **core:** Fix sometimes calc of torrents sizes([f255b87](https://github.com/DEgITx/rats-search/commit/f255b8770a78ec765755fb3ebdc092e4b531b54d)) by @DEgITx


### 📚 Documentation


- **linux:** Some centos doc([8a5a302](https://github.com/DEgITx/rats-search/commit/8a5a30211af7eb1d78574a0038c17906a2c77cfe)) by @DEgITx


### 🚀 Features


- **p2p:** Replaced old ssh relay to new relay mechanism (improved p2p network stablity over NAT).([df1ebd1](https://github.com/DEgITx/rats-search/commit/df1ebd1e036629486c5bb5fd790990d96a66b4b1)) by @DEgITx


### 🧪 Testing


- **1337x:** Disabled test for now([9dc684a](https://github.com/DEgITx/rats-search/commit/9dc684a0978354d2bcce6102d46ea171989b0416)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 5 commits


## [1.1.4](https://github.com/DEgITx/rats-search/compare/v1.1.3...v1.1.4) (2019-02-19)


### 🐛 Bug Fixes


- **p2p:** Break backward p2p compability < 1.1.0([32ceb6d](https://github.com/DEgITx/rats-search/commit/32ceb6de6e97f916630c4bf1e66e3e4862cd128a)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


## [1.1.3](https://github.com/DEgITx/rats-search/compare/v1.1.2...v1.1.3) (2019-01-31)


### ⚡ Performance


- **memory:** Fix memory leak on gui size([60f3052](https://github.com/DEgITx/rats-search/commit/60f3052b31d8f60af6cd1060838a103098b258b1)) by @DEgITx


### 🐛 Bug Fixes


- **updater:** Linux update format was broken([c7041cc](https://github.com/DEgITx/rats-search/commit/c7041cc4968f6533749d96e1918f9a4b3449aeec)) by @DEgITx
- **test:** Less agressive start timeout([fc889db](https://github.com/DEgITx/rats-search/commit/fc889db4f7817f54f3723da31ca1df299b2903eb)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 3 commits


## [1.1.2](https://github.com/DEgITx/rats-search/compare/v1.1.1...v1.1.2) (2019-01-28)


### 🐛 Bug Fixes


- **bootstrap:** Fix preventing bootstrap saving #74 #69([6e79ae4](https://github.com/DEgITx/rats-search/commit/6e79ae4db3aa52294374d2c656277cb9597804f2)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


## [1.1.1](https://github.com/DEgITx/rats-search/compare/v1.1.0...v1.1.1) (2019-01-26)


### 🐛 Bug Fixes


- **portative:** Fix updater check #71([e7b8533](https://github.com/DEgITx/rats-search/commit/e7b85335ed09265e2ea09b1bab7401c3e08b9131)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


## [1.1.0](https://github.com/DEgITx/rats-search/compare/v1.0.0...v1.1.0) (2019-01-26)


### ⚡ Performance


- **start:** Simplify some init statistic calls([57883b0](https://github.com/DEgITx/rats-search/commit/57883b0ba42e6519353b0d7b7eb271f7d8768e94)) by @DEgITx


### 🐛 Bug Fixes


- **p2p:** Fix files in db count on new architecture([9ed0ad9](https://github.com/DEgITx/rats-search/commit/9ed0ad9683b87ef9e41998dcd9a3179cb19ffeab)) by @DEgITx
- **build:** Fix production build after upgrade to new engine([7fb5c9a](https://github.com/DEgITx/rats-search/commit/7fb5c9a500367b10631cdfdcacdf3d2d4cb9035b)) by @DEgITx
- **tests:** Fix check of starting download([9453ab8](https://github.com/DEgITx/rats-search/commit/9453ab8ba8c1281be6380488a7795341b7c76cac)) by @DEgITx
- **arm:** Ignore relay on arm #66([aa60148](https://github.com/DEgITx/rats-search/commit/aa6014862d023be1d82ab7d23f0fed2d404f9cce)) by @DEgITx
- **tests:** More timeout time for some strategies([8dd8b37](https://github.com/DEgITx/rats-search/commit/8dd8b3769c2928fd8e72a43eb4eeaf61fb4cb11c)) by @DEgITx
- **p2p:** Potencial problem with some p2p decoding messages([159bb10](https://github.com/DEgITx/rats-search/commit/159bb10f6ad660e915928a65e6623d9fb25132ba)) by @DEgITx
- **1337x:** Proper poster in some cases([db3c6fa](https://github.com/DEgITx/rats-search/commit/db3c6faab29f437c74085bee25ca3aec24c6ad40)) by @DEgITx


### 📚 Documentation


- **trackers:** Documentation about how write own strategy([b7c0105](https://github.com/DEgITx/rats-search/commit/b7c0105cfca3be4d39843c0c08b022d1eab92afa)) by @DEgITx


### 🚀 Features


- **strategies:** Autoload trackers strategies([23e2871](https://github.com/DEgITx/rats-search/commit/23e28716695137880415dfc2519951cd35fa1881)) by @DEgITx
- **core:** Updated to new browser engine([a43f743](https://github.com/DEgITx/rats-search/commit/a43f7431ddade07d768bd0765905a4d6f6245357)) by @DEgITx
- **strategies:** 1337 strategie([44a800d](https://github.com/DEgITx/rats-search/commit/44a800dbbb5ae684ec1617721eac27fbc56ab54c)) by @DEgITx
- **arm:** Added testing arm support([c78e559](https://github.com/DEgITx/rats-search/commit/c78e5597d2c80764976972370c31cb9b4ae557f3)) by @DEgITx
- **ui:** Settings tabs([dd982da](https://github.com/DEgITx/rats-search/commit/dd982da6b1abfafe3cd9e9cce12f2bca82715980)) by @DEgITx


### 🧪 Testing


- **downloading:** Add downloading test([7277ffb](https://github.com/DEgITx/rats-search/commit/7277ffba8f5389afafd3bd04306f137fe51fad05)) by @DEgITx
- **downloading:** Add downloading tab & some buttons tests([491b99f](https://github.com/DEgITx/rats-search/commit/491b99fd088ce11ff692dc0d6c3934c4120da35b)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 16 commits


## [1.0.0](https://github.com/DEgITx/rats-search/compare/v0.30.1...v1.0.0) (2018-12-02)


### ⚡ Performance


- **architecture:** Big performance improvements over big databases and files highlight in search. ([6afe857](https://github.com/DEgITx/rats-search/commit/6afe85798a2111c53081933fc4ea370814cdac7d)) by @DEgITx


### 🐛 Bug Fixes


- **feed:** Rating also accumulate for times over([564a973](https://github.com/DEgITx/rats-search/commit/564a973adff6d6524d5a6cae1e8eb5645d6c593a)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 2 commits


## [0.30.1](https://github.com/DEgITx/rats-search/compare/v0.30.0...v0.30.1) (2018-10-17)


### 🐛 Bug Fixes


- **windows:** Fix starting under some path'es([ed67c4a](https://github.com/DEgITx/rats-search/commit/ed67c4a95c45c5034e296ffa3f554c246b7ebcef)) by @DEgITx
- **config:** Fully disable p2p activity if option with p2p search disabled([659a2aa](https://github.com/DEgITx/rats-search/commit/659a2aa002715d7c68b6628c0695fe84cca1ec30)) by @DEgITx
- **downloading:** Fix finishing files in some cases([03230a0](https://github.com/DEgITx/rats-search/commit/03230a0ff49d7e21ff97021e6444510094087316)) by @DEgITx
- **downloading:** Fix continue downloading on selection([97f184c](https://github.com/DEgITx/rats-search/commit/97f184cd9c0e33ee15b8b31cfd941889bfd60a76)) by @DEgITx


### 📚 Documentation


- **readme:** Add new features in readme, removed old one about data transfer that mean something other (will be reformulated)([acd549e](https://github.com/DEgITx/rats-search/commit/acd549ea8f620c2c7ce4866b8f7514a689426619)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 5 commits


## [0.30.0](https://github.com/DEgITx/rats-search/compare/v0.29.3...v0.30.0) (2018-09-02)


### 🐛 Bug Fixes


- **top:** Fix top tab redraw bug regression in v0.29.3([7bb61e1](https://github.com/DEgITx/rats-search/commit/7bb61e100f1f9d6a8246b13171ed4402e73df68c)) by @DEgITx
- **gui:** Fix some warnings([2773db3](https://github.com/DEgITx/rats-search/commit/2773db3949bc6dd79dab63ed88944ddd99590faa)) by @DEgITx
- **tests:** Rutor test([851391e](https://github.com/DEgITx/rats-search/commit/851391e187b54f3853131e74905af34a8f6ffe32)) by @DEgITx
- **rutor:** Fix problems with dir creation([1b30898](https://github.com/DEgITx/rats-search/commit/1b308981dd91fa89820574de58c0fbc9204760b9)) by @DEgITx
- **feed:** Temporary hide adult torrents from feed([0e9ae95](https://github.com/DEgITx/rats-search/commit/0e9ae9584f306e3fa87d81662454dd058066eb33)) by @DEgITx
- **transfer:** Fix multiple transfer requests([7098bdd](https://github.com/DEgITx/rats-search/commit/7098bdd0b3581a2177a32fa93ee1d2955d51e1ea)) by @DEgITx
- **rutor:** Little fix([720946d](https://github.com/DEgITx/rats-search/commit/720946d518742343b7e52e1c57ebeddbbefae583)) by @DEgITx
- **rutor:** Fix x rutor file update([f9d2c06](https://github.com/DEgITx/rats-search/commit/f9d2c06ab6702af3ededac39d40af197ba1d23d7)) by @DEgITx


### 🚀 Features


- **downloading:** Ability to choose files which you wanna download([0b329b8](https://github.com/DEgITx/rats-search/commit/0b329b83287dfdb7bcc1555252e14c697d7e6d6d)) by @DEgITx
- **downloading:** Choose dir on new download([4c2ff46](https://github.com/DEgITx/rats-search/commit/4c2ff465f26670b79568886572c728049d1e2927)) by @DEgITx
- **search:** Search among info indexes([ff28e70](https://github.com/DEgITx/rats-search/commit/ff28e70251500c79061c1b6c0d75651569489545)) by @DEgITx
- **trackers:** Closing ability([0fe31cb](https://github.com/DEgITx/rats-search/commit/0fe31cbb6e7f9910dbd1927bea5581a5a4c5c113)) by @DEgITx
- **p2p:** File transfer p2p feature([3e6ac4c](https://github.com/DEgITx/rats-search/commit/3e6ac4c00dbd2a50ac5baef35c25e1eeac24184a)) by @DEgITx
- **p2p:** Folder transfer feature([8ee277e](https://github.com/DEgITx/rats-search/commit/8ee277e764e3ca26fe7713ebd9494daad6a870ea)) by @DEgITx
- **rutor:** Rutor strategie([73fd698](https://github.com/DEgITx/rats-search/commit/73fd69802097e33524a3af13e8d88db614469aa6)) by @DEgITx
- **transfer:** Download everything to tmp dir to prevent unfinish transfers([4334463](https://github.com/DEgITx/rats-search/commit/4334463e3c8f2e04256691365ce4f71cfb084002)) by @DEgITx
- **rutor:** Category rutor([429263e](https://github.com/DEgITx/rats-search/commit/429263e61e8590a9a74bb189c6f3d45c1975ad64)) by @DEgITx
- **rutor:** Visual gui display([a982a2c](https://github.com/DEgITx/rats-search/commit/a982a2cf9d6850744b356ba354fa05cd245c64ac)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 18 commits


## [0.29.3](https://github.com/DEgITx/rats-search/compare/v0.29.2...v0.29.3) (2018-08-26)


### ⚡ Performance


- **top:** Opmization on top load([1e09243](https://github.com/DEgITx/rats-search/commit/1e09243756333b87cd5682bcf148a45e537a11cb)) by @DEgITx
- **top:** Fix unnecessary drawing on top page([ff4b98b](https://github.com/DEgITx/rats-search/commit/ff4b98bd1f9f9ddf4255b721621bc5f1c8d4e193)) by @DEgITx


### 🐛 Bug Fixes


- **gui:** Fix memory leak on events handling([349eede](https://github.com/DEgITx/rats-search/commit/349eedee0a18dd3b4eccf492567ab95a1ca7fdf5)) by @DEgITx
- **log:** Display render log in main log([1acc6fa](https://github.com/DEgITx/rats-search/commit/1acc6fafb4c615d86200a121aabed4f17ef63e68)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 4 commits


## [0.29.2](https://github.com/DEgITx/rats-search/compare/v0.29.1...v0.29.2) (2018-08-20)


### 🐛 Bug Fixes


- **feed:** Fix check adding record on info table([4dfd5cd](https://github.com/DEgITx/rats-search/commit/4dfd5cd15a0923203503416e9b877acc89e36530)) by @DEgITx
- **nyaa:** Fix errors on nyaa([91fcc5d](https://github.com/DEgITx/rats-search/commit/91fcc5d68e9e4455669be943a9cc282abf48140c)) by @DEgITx
- **translations:** Fix translations change on server version([e111fea](https://github.com/DEgITx/rats-search/commit/e111fea12a3f09be49db22f6f93cca39edcacfd5)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 3 commits


## [0.29.1](https://github.com/DEgITx/rats-search/compare/v0.29.0...v0.29.1) (2018-08-19)


### 🐛 Bug Fixes


- **gui:** Display feed fix([cb7d90b](https://github.com/DEgITx/rats-search/commit/cb7d90b2e2253951ea7cb357cfcf38ceb3a9b825)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


## [0.29.0](https://github.com/DEgITx/rats-search/compare/v0.28.0...v0.29.0) (2018-08-18)


### 🐛 Bug Fixes


- **ssh:** Prevent ssh relay startup on exit([5cf5bea](https://github.com/DEgITx/rats-search/commit/5cf5bea884c0d22e2d5f66e67e3cd3c8ed8d4fea)) by @DEgITx
- **db:** Fix external sphinx openning check in some cases #57([34cdc7f](https://github.com/DEgITx/rats-search/commit/34cdc7fb0999b175b0263ed844175bcf1facca46)) by @DEgITx
- **db:** Broke remote connection (security purposes)([dba7162](https://github.com/DEgITx/rats-search/commit/dba71624afe9f3e4e339de580a91cd20f2a9ea70)) by @DEgITx
- **linux:** Fix closing on linux in some cases([a423087](https://github.com/DEgITx/rats-search/commit/a4230874f5a44c63c81bb13602d459e006b69d8e)) by @DEgITx
- **closing:** Another part of closing fixes([e3fd5a4](https://github.com/DEgITx/rats-search/commit/e3fd5a4c844215b60cb0b39886687303eaea53f9)) by @DEgITx
- **rutracker:** Rutracker id feat([1d76939](https://github.com/DEgITx/rats-search/commit/1d76939e26a32fcea0ccf261efff773b46387e29)) by @DEgITx
- **trackers:** Fix updating trackers on page open([34981c3](https://github.com/DEgITx/rats-search/commit/34981c36093a77117c92c4fda15306fc0883d3f2)) by @DEgITx
- **rutracker:** Encoding fix([266c103](https://github.com/DEgITx/rats-search/commit/266c103321c5b6c2f850ff75d607c8c37a2a8bdb)) by @DEgITx
- **rutracker:** Fix first post([f8fe2fc](https://github.com/DEgITx/rats-search/commit/f8fe2fc3598a401a1dc759fee6bb7063f61f9356)) by @DEgITx
- **gui:** Fix new window on middle clicks and open external on link clicks([8330d4c](https://github.com/DEgITx/rats-search/commit/8330d4c2ad97f72f8496267b3c2155aa80adeb5e)) by @DEgITx
- **replication:** Fix replication from object info([14a109d](https://github.com/DEgITx/rats-search/commit/14a109d5d76e7c0187ea14a7d59cf5a78664f87e)) by @DEgITx
- **nyaa:** Nyaa test fix([f5bacca](https://github.com/DEgITx/rats-search/commit/f5bacca85572c107c59ef6d9df40d7d85c417bb1)) by @DEgITx


### 🚀 Features


- **tests:** Search test([ae7b6fe](https://github.com/DEgITx/rats-search/commit/ae7b6fe793ef9ddd1f3b243ea061b1f840bb2654)) by @DEgITx
- **drop:** Support torrents folder drag and drop with recursive scan torrents files([82e7764](https://github.com/DEgITx/rats-search/commit/82e77640d685624ca36a0b8cca4b98a5d62d992e)) by @DEgITx
- **search:** Support japanese, korean, chinese propper search([08d6672](https://github.com/DEgITx/rats-search/commit/08d6672eb040c8b5c26fc7820181b15b49f24f32)) by @DEgITx
- **trackers:** Basic trackers integration([317cfdd](https://github.com/DEgITx/rats-search/commit/317cfdd3fb707d94d0089e66990d2cc8133b05e7)) by @DEgITx
- **trackers:** Merge trackers info([17371bd](https://github.com/DEgITx/rats-search/commit/17371bd8ff95a8675fbb3e63ab7420c6519ced59)) by @DEgITx
- **gui:** Trackers basic display([7cf5c17](https://github.com/DEgITx/rats-search/commit/7cf5c17fe4f6187f081b315657622205e18699b2)) by @DEgITx
- **trackers:** Display descriptions([cd9a88a](https://github.com/DEgITx/rats-search/commit/cd9a88a608ff906fc820f34291f3ca21f64ef672)) by @DEgITx
- **rutracker:** Category and torrent name from rutracker([466d10c](https://github.com/DEgITx/rats-search/commit/466d10ccd5afcdeb17ac6cb2449a6056800af244)) by @DEgITx
- **config:** Add config to disable trackers([6531b53](https://github.com/DEgITx/rats-search/commit/6531b5315f85e6b73e95946c5f75e31597fea3cb)) by @DEgITx
- **udp-trackers:** More alternative([d522a53](https://github.com/DEgITx/rats-search/commit/d522a53988a16ed8cfd08742c894547aa7372eee)) by @DEgITx


### 🧪 Testing


- **search:** Description to asserts([0cd43d6](https://github.com/DEgITx/rats-search/commit/0cd43d674da5802425a3cda56b0689524beb1c2c)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 23 commits


## [0.28.0](https://github.com/DEgITx/rats-search/compare/v0.27.0...v0.28.0) (2018-08-07)


### ⚡ Performance


- **replication:** Replicate number accordion to cpu usage([35f7d33](https://github.com/DEgITx/rats-search/commit/35f7d33e8f2e028c74dcd91646eb6cfa5589dfc2)) by @DEgITx
- **torrents:** Ability to disable integrity check on torrents adding torrents #47([e7b035a](https://github.com/DEgITx/rats-search/commit/e7b035a1a8a630db8ff06db40a8062db43f0023e)) by @DEgITx


### 🐛 Bug Fixes


- **log:** Color messages (part 1)([5bdab1b](https://github.com/DEgITx/rats-search/commit/5bdab1b516aa32264e1e1da9434f0a3d41788efd)) by @DEgITx
- **log:** Color log (part 2)([bf62a2d](https://github.com/DEgITx/rats-search/commit/bf62a2de16a321d770fba59776d83d9ae6c1aa05)) by @DEgITx
- **log:** Color log (part 3)([9981c97](https://github.com/DEgITx/rats-search/commit/9981c9706536a1c714affe4f73c574f27f978f96)) by @DEgITx
- **server:** Color log server fix([725632e](https://github.com/DEgITx/rats-search/commit/725632e70950009c66d834cc62cb26df0858e9ec)) by @DEgITx
- **db:** Under mac and linux using alternative pool mechanism([297baac](https://github.com/DEgITx/rats-search/commit/297baac3d335a618460d082be1e166e04edccd7a)) by @DEgITx
- **closing:** Fix errors on closing([463eb7c](https://github.com/DEgITx/rats-search/commit/463eb7cce7c0cad68c4ff06b167c4f81d1e5f950)) by @DEgITx


### 🚀 Features


- **log:** Color log([aa68200](https://github.com/DEgITx/rats-search/commit/aa68200bc585378c9c6736b0d0dd1de8b3472d17)) by @DEgITx
- **log:** Error display with separate color #31([ddfd434](https://github.com/DEgITx/rats-search/commit/ddfd43498ddacfbf75fd349e30afec518781146e)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 10 commits


## [0.27.0](https://github.com/DEgITx/rats-search/compare/v0.26.2...v0.27.0) (2018-08-06)


### ⚡ Performance


- **replication:** Replication thread optimization([e777757](https://github.com/DEgITx/rats-search/commit/e77775794fda049c88251092b782bcf3bb6ca404)) by @DEgITx


### 🐛 Bug Fixes


- **server:** Fix exit on server version #54 #52([e420804](https://github.com/DEgITx/rats-search/commit/e42080469739431ed73a19284d3e5aab2a45d68f)) by @DEgITx
- **translations:** Hash translation([1cec380](https://github.com/DEgITx/rats-search/commit/1cec38020264ef0a9e71a0a25f41cecb6cee1857)) by @DEgITx
- **gui:** Top tabs text overlap([c2da0c3](https://github.com/DEgITx/rats-search/commit/c2da0c304728f3eff3bf9c43af3a91f731f47c49)) by @DEgITx
- **linux:** Fix closing on linux([2631662](https://github.com/DEgITx/rats-search/commit/2631662168f76fca2299edf074a7192f8e85403d)) by @DEgITx
- **linux:** Fix console control after exit([eb5d7d1](https://github.com/DEgITx/rats-search/commit/eb5d7d1594c7ac8bc99e931fb3bc72954649a1a2)) by @DEgITx
- **scanner:** Fix enconding names in some cases #55([28c07fc](https://github.com/DEgITx/rats-search/commit/28c07fc68939e012247de4ce69065ab72324ccb5)) by @DEgITx
- **background:** One closing pattern([0f450d3](https://github.com/DEgITx/rats-search/commit/0f450d3133b538b072a899b5396343dfff375c69)) by @DEgITx
- **closing:** Window can be closing on event([9d5667e](https://github.com/DEgITx/rats-search/commit/9d5667eb0045f7a0242631dfbd52ba2fdc431290)) by @DEgITx
- **macos:** Fix crashes under Mac OS X([45af9bd](https://github.com/DEgITx/rats-search/commit/45af9bddeaf62a6d804879eedddd3ad1fd9e15b9)) by @DEgITx
- **macos:** Stabilization with connection pool([25a3d8b](https://github.com/DEgITx/rats-search/commit/25a3d8b6dd86ec2b4d141b2567b557825d62cc9d)) by @DEgITx


### 📚 Documentation


- **manual:** Updated usage guide([67badfb](https://github.com/DEgITx/rats-search/commit/67badfbe75ae28478ecd072fa424abb545299992)) by @DEgITx


### 🚀 Features


- **cleaning:** Fix cleaning checking and removing torrents (also display cleaning status in more details) #52([e58ffa4](https://github.com/DEgITx/rats-search/commit/e58ffa4531dd5249b564bf59936d7db165eeec67)) by @DEgITx
- **search:** Hash/magnet search support in db([b48ac7f](https://github.com/DEgITx/rats-search/commit/b48ac7f97364d278dd7d5681bc16232012401769)) by @DEgITx
- **search:** Add remote torrents in db via dht and search requests([c6bef2f](https://github.com/DEgITx/rats-search/commit/c6bef2f94ab851c4f260a56b530e0c07a011af68)) by @DEgITx
- **torrents:** Add support for dropping torrent to base just with window([1442a65](https://github.com/DEgITx/rats-search/commit/1442a65c6e6bb3798c666745f60845c0b3dfd092)) by @DEgITx
- **rutracker:** Rutracker basic integratioin([bf23587](https://github.com/DEgITx/rats-search/commit/bf23587211bebc988f8d24cd2ed2846bc34c5a4c)) by @DEgITx
- **nyaa:** Support of nyaa torrents([ffd0643](https://github.com/DEgITx/rats-search/commit/ffd0643c0b1e593115e555a48321fa3d665d7c59)) by @DEgITx
- **closing:** Fast window closing/hiding([585b683](https://github.com/DEgITx/rats-search/commit/585b68338052ed7f7913779e949617a9d4dab6c2)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 19 commits


## [0.26.2](https://github.com/DEgITx/rats-search/compare/v0.26.1...v0.26.2) (2018-07-22)


### 🐛 Bug Fixes


- **feed:** Fix store unsync case([08fbca6](https://github.com/DEgITx/rats-search/commit/08fbca648a0cd7a63cf451acbb6436f6f09a9a66)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


## [0.26.1](https://github.com/DEgITx/rats-search/compare/v0.26.0...v0.26.1) (2018-07-22)


### 🐛 Bug Fixes


- **filters:** Fix filter cleanup on categories #51([cb4787b](https://github.com/DEgITx/rats-search/commit/cb4787b2c5f5ad3b25579f9d6fcb407f33f26b25)) by @DEgITx
- **feed:** Keep feed alive on down votes([f52d789](https://github.com/DEgITx/rats-search/commit/f52d7891f38593b253aa9b04cb4d96f5fa31d744)) by @DEgITx
- **feed:** Fix fileList coruption of feed replication([2d41bac](https://github.com/DEgITx/rats-search/commit/2d41bacf9b8fa26f596a2dfa442dbf610c88f097)) by @DEgITx
- **feed:** Replicate all feed record on new feed([7e08d70](https://github.com/DEgITx/rats-search/commit/7e08d70d56d02be73c8e8618b96351f5bbd81fca)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 4 commits


## [0.26.0](https://github.com/DEgITx/rats-search/compare/v0.25.0...v0.26.0) (2018-07-20)


### 🐛 Bug Fixes


- **feed:** Exchage feeds with same size but more new([454d65d](https://github.com/DEgITx/rats-search/commit/454d65d6235d12a86ba9f3fc1fddfe659f9e1931)) by @DEgITx


### 🚀 Features


- **gui:** Remember scroll on back #13([246a7a9](https://github.com/DEgITx/rats-search/commit/246a7a979798930c6186947bf11ea760dae8d576)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 2 commits


## [0.25.0](https://github.com/DEgITx/rats-search/compare/v0.24.0...v0.25.0) (2018-07-19)


### ⚡ Performance


- **torrents:** Optimization files saving([16aa6d7](https://github.com/DEgITx/rats-search/commit/16aa6d7e62a9b40ea66f335d7460e42c727477e0)) by @DEgITx


### 🐛 Bug Fixes


- **scroll:** Fix jolting on menu clicks([376ddc7](https://github.com/DEgITx/rats-search/commit/376ddc7678a1fb2f82386d442978e72307cdb0a8)) by @DEgITx
- **feed:** Fix feed synchronization([0c16bea](https://github.com/DEgITx/rats-search/commit/0c16bead45203306443b0b24e1b059ce67dac392)) by @DEgITx
- **bootstrap:** Parallel bootstrap saving([5a2a6a5](https://github.com/DEgITx/rats-search/commit/5a2a6a5db5d26bea5e772847e2266f3a27391f1f)) by @DEgITx
- **web:** Set default nodejs version 8 for web version([4b16e2d](https://github.com/DEgITx/rats-search/commit/4b16e2d065e829ef0c622287a3bd48b97eaf12bb)) by @DEgITx
- **network:** Fix network availability detection #45([87bbb4f](https://github.com/DEgITx/rats-search/commit/87bbb4fbe4a2e75fc80faf8e346a9f9e9c17f334)) by @DEgITx
- **feed:** Feed error ordering resolved([b5a775e](https://github.com/DEgITx/rats-search/commit/b5a775e7d539aeeb983a1eff854beb9bac375fcb)) by @DEgITx


### 📚 Documentation


- **readme:** Add support link([2f9e393](https://github.com/DEgITx/rats-search/commit/2f9e393ad8fcd644cc704ab3422a3e033d4b08d3)) by @DEgITx


### 📦 Miscellaneous


- **log:** Add node version log([c2c0977](https://github.com/DEgITx/rats-search/commit/c2c0977561e33620c4b54933db1adba0ffb771cb)) by @DEgITx


### 🚀 Features


- **scroll:** Smaller scrollbar for more comport view([b9885b3](https://github.com/DEgITx/rats-search/commit/b9885b3cb0e406ea24f058978275a7e14b8873c3)) by @DEgITx
- **filters:** Add size filter([0727da3](https://github.com/DEgITx/rats-search/commit/0727da32367b2d5591f5680eeec17aafd04a3aaa)) by @DEgITx
- **filters:** Content type filters #49([8792da8](https://github.com/DEgITx/rats-search/commit/8792da82d53e16d49ebcbe7301151529af9cf1be)) by @DEgITx
- **p2p:** Information about feed([bd8921c](https://github.com/DEgITx/rats-search/commit/bd8921c18666262ad6478a96e06cf0b6aaa562e9)) by @DEgITx
- **portative:** Display message about update in portative version #41([66fea26](https://github.com/DEgITx/rats-search/commit/66fea269c68a10c7588b872bc6d082b84698318f)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 14 commits


## [0.24.0](https://github.com/DEgITx/rats-search/compare/v0.23.0...v0.24.0) (2018-07-13)


### 🐛 Bug Fixes


- **ignore:** Ignore more server files([7799f1f](https://github.com/DEgITx/rats-search/commit/7799f1fd21d7252173e5c3afb2e7bf88612079ea)) by @DEgITx
- **patch:** Fix memory issue([064c93c](https://github.com/DEgITx/rats-search/commit/064c93c17616b665c848b26ea222cd9eac8fe035)) by @DEgITx
- **menu:** New page struct update in menu list([a44769d](https://github.com/DEgITx/rats-search/commit/a44769d350e078b4a61d785e4a607ed79a7ef917)) by @DEgITx
- **updater:** Fix errors messages on updater if no internet connection #45([67b0215](https://github.com/DEgITx/rats-search/commit/67b0215c9375b27614b9300495040e22dfa3a9f5)) by @DEgITx
- **db:** Fix startup on broken db after hardware shutdown #43([1f7ff44](https://github.com/DEgITx/rats-search/commit/1f7ff446cc463c9c9c369b44405bf5ae6347280c)) by @DEgITx


### 📚 Documentation


- **readme:** Little translation guide([9e2986d](https://github.com/DEgITx/rats-search/commit/9e2986d993f18dd13c03a3ec99d89800d569ccaf)) by @DEgITx


### 📦 Miscellaneous


- **readme:** Add new readme screens and test([d6fc200](https://github.com/DEgITx/rats-search/commit/d6fc2004cd80b17046a230e0c244ecc8fe017420)) by @DEgITx


### 🚀 Features


- **search:** Part words search feature([8adcdac](https://github.com/DEgITx/rats-search/commit/8adcdac6f4660d6702feca0091efe705b528d6c9)) by @DEgITx
- **p2p:** New optimized peer exchange algorithm([67ec475](https://github.com/DEgITx/rats-search/commit/67ec475a3a752f9f797c1a99090fbbf044fead2f)) by @DEgITx
- **bootstrap:** Always load bootstrap peers when no peers situation (not only on startup)([fce0d0b](https://github.com/DEgITx/rats-search/commit/fce0d0b275af3f6c023b6d2a1e9a96841c74f5cb)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 10 commits


## [0.23.0](https://github.com/DEgITx/rats-search/compare/v0.22.0...v0.23.0) (2018-07-02)


### 🐛 Bug Fixes


- **db:** Fix broken connections in some cases #37([0d9bb12](https://github.com/DEgITx/rats-search/commit/0d9bb12fe89d17634d652a31f892aee61645a391)) by @DEgITx
- **header:** Replaced button places in top header([a3abb2f](https://github.com/DEgITx/rats-search/commit/a3abb2f49936787f190f2cc3ea17080e4fbe112c)) by @DEgITx
- **gui:** Padding on some long torrent names([ec861e1](https://github.com/DEgITx/rats-search/commit/ec861e1f0a53f27119367a0a3463eb827bdb70f1)) by @DEgITx
- **downloading:** Always show downloading on list even if download not started([6bd4d69](https://github.com/DEgITx/rats-search/commit/6bd4d69e4cc610a1399836479492698dc0ee1625)) by @DEgITx
- **downloading:** Fix state on some torrent cases([ea07872](https://github.com/DEgITx/rats-search/commit/ea07872137a7caa17c80663680a334c602c03cbb)) by @DEgITx
- **downloading:** Fix recheck progress indication on start and finish([1a23973](https://github.com/DEgITx/rats-search/commit/1a23973ba2039127270e93f917535b0eb3f7db90)) by @DEgITx
- **feed:** Keep downloading values seperate([7592612](https://github.com/DEgITx/rats-search/commit/7592612a3d46037fab8bf2df0f34d0921e4304d5)) by @DEgITx
- **replication:** Fix replication on some cases of downloading([72828b8](https://github.com/DEgITx/rats-search/commit/72828b860f778ff85adeae80bb5418b57b07438a)) by @DEgITx
- **tests:** Simplify tests([8dc7cfe](https://github.com/DEgITx/rats-search/commit/8dc7cfecf9667a47521e685c3920e59233186127)) by @DEgITx
- **db:** Fix connection limitation([db7217f](https://github.com/DEgITx/rats-search/commit/db7217ff0dc87602ac2241ea2d931493e16ae13f)) by @DEgITx
- **tests:** Fix intersect test case([352213a](https://github.com/DEgITx/rats-search/commit/352213ae0e7753e869eb6d8dca69a3b003984658)) by @DEgITx


### 🚀 Features


- **downloading:** Download speed([c321286](https://github.com/DEgITx/rats-search/commit/c321286d6ce38fdd18c47d87d15f509f035fa3c0)) by @DEgITx
- **downloading:** Save download session support([0d81364](https://github.com/DEgITx/rats-search/commit/0d81364ca82268b43833313a583b350466a2fdc2)) by @DEgITx
- **downloading:** Removing after download ability([d1d816e](https://github.com/DEgITx/rats-search/commit/d1d816ed030e1e8e214444fa23d1398d788fe502)) by @DEgITx
- **downloading:** Tooltips on torrent element([545f772](https://github.com/DEgITx/rats-search/commit/545f772918e08f31bf089fb66f8784a9af14b84c)) by @DEgITx
- **downloading:** Torrent pause feature([070119c](https://github.com/DEgITx/rats-search/commit/070119c352727ed0d18245c9f62d370624696ec0)) by @DEgITx
- **config:** P2p replication client and server separate for performance optimization([f8c6a40](https://github.com/DEgITx/rats-search/commit/f8c6a401bca9893c3bf8e97add9833bf4485ca60)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 17 commits


## [0.22.0](https://github.com/DEgITx/rats-search/compare/v0.21.1...v0.22.0) (2018-06-24)


### 🐛 Bug Fixes


- **p2p:** P2p proper closing([5efb309](https://github.com/DEgITx/rats-search/commit/5efb309dc1912bf77e5cb95cee5c8ba399300971)) by @DEgITx
- **replication:** Fix partitial torrent adding([3515745](https://github.com/DEgITx/rats-search/commit/35157457c7d1e13e8e98093735a29cf5cd484c90)) by @DEgITx
- **closing:** More proper closing without errors([16164c2](https://github.com/DEgITx/rats-search/commit/16164c25d87d994eff74b02e6e3f01c25e5d1600)) by @DEgITx
- **linux:** Fix AppImage build #35([86692ee](https://github.com/DEgITx/rats-search/commit/86692ee857ca3cfb8a0ecdb32f9b230ae4a54a1d)) by @DEgITx


### 🚀 Features


- **debug:** Debug rejections([ead88b0](https://github.com/DEgITx/rats-search/commit/ead88b0a037253a02fd47d3f4bae88adc315ec76)) by @DEgITx
- **gui:** Tips over some buttons([8868bae](https://github.com/DEgITx/rats-search/commit/8868bae97d9f6fc6958bf06b714438319cddebcd)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 6 commits


## [0.21.1](https://github.com/DEgITx/rats-search/compare/v0.21.0...v0.21.1) (2018-06-19)


### 🐛 Bug Fixes


- **download:** Crash on download page click in some cases #34([44efa61](https://github.com/DEgITx/rats-search/commit/44efa6138e79ff06beb510be0c051191b74dc2ba)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


## [0.21.0](https://github.com/DEgITx/rats-search/compare/v0.20.1...v0.21.0) (2018-06-18)


### Translations


- **ru:** Some new tr([26669cf](https://github.com/DEgITx/rats-search/commit/26669cf7643a3d547e3f2b67000f706b1e992571)) by @DEgITx


### ⚡ Performance


- **db:** Little faster cycles over small requests([859770b](https://github.com/DEgITx/rats-search/commit/859770bdcfb627af60b46da9cf60793f4c54a7ec)) by @DEgITx


### 🐛 Bug Fixes


- **translations-ru:** Fix some messages (thanks to Viktor Lukash)([f735565](https://github.com/DEgITx/rats-search/commit/f7355657681c3ce8485458365e66051bd055135b)) by @DEgITx
- **trackers:** Fix tracker response error([e6276f0](https://github.com/DEgITx/rats-search/commit/e6276f06c2a10c18a4796addf4a5636b459d6253)) by @DEgITx
- **spider:** Fix compiling error on start([ea787ec](https://github.com/DEgITx/rats-search/commit/ea787ec358bb3a8c2a463cd3f7146dd70f8a0600)) by @DEgITx
- **store:** Fix limitation on records sync([7772129](https://github.com/DEgITx/rats-search/commit/77721293e44db316f0c0dcff5f20fbe16b24ace7)) by @DEgITx
- **feed:** Fix feed build([88c3b3e](https://github.com/DEgITx/rats-search/commit/88c3b3e3dbb687e8cedf309676fb8be9a85a3f95)) by @DEgITx
- **vote:** Ignore voting if already vote([88a0771](https://github.com/DEgITx/rats-search/commit/88a0771579458113dd00d815bf8dc8ee61f326f3)) by @DEgITx
- **store:** Fix potential out of records on find request([9502e53](https://github.com/DEgITx/rats-search/commit/9502e53fc1e2613d3aa7a067059ac9328ff9b4bc)) by @DEgITx
- **feed:** Always insert new feed item([e0f4319](https://github.com/DEgITx/rats-search/commit/e0f43191fb4230bb8e69e150ef8db0afeb7da7ef)) by @DEgITx
- **feed:** Fix p2p load error([545a067](https://github.com/DEgITx/rats-search/commit/545a0676c0a20503233c30226c3a0280d52f768d)) by @DEgITx
- **gui:** Fix potencial false window load([94218b7](https://github.com/DEgITx/rats-search/commit/94218b7b74114d2a060511cb7fa53ca525be74cb)) by @DEgITx
- **gui:** Little fixes([06e54fc](https://github.com/DEgITx/rats-search/commit/06e54fca0f1cf227cfef8e8e5926360de88c427a)) by @DEgITx
- **gui:** Always back to main from torrent page, even torrent is loading([2d653ca](https://github.com/DEgITx/rats-search/commit/2d653caaa71222c396f92e0a3b9693de3a1d0023)) by @DEgITx
- **feed:** Type in adding feed entity([5250b0f](https://github.com/DEgITx/rats-search/commit/5250b0f86c3392b2574b9a65b498b4f1ec19fd04)) by @DEgITx
- **feed:** Fix feed live update([bed8b5b](https://github.com/DEgITx/rats-search/commit/bed8b5b51b8b3baf0634a89236dcbb49ce899c86)) by @DEgITx


### 🚀 Features


- **translations-ua:** Ukraine translation (thanks to Viktor Lukash)([6decf60](https://github.com/DEgITx/rats-search/commit/6decf602d356c4db3f1754159f11070bca089435)) by @DEgITx
- **server:** Update dependecy server module babel installation #33([ae8b84f](https://github.com/DEgITx/rats-search/commit/ae8b84fed6c826f1e2738dae57a3768243cb7a56)) by @DEgITx
- **tray:** Tray hide on closing #32([50a95ae](https://github.com/DEgITx/rats-search/commit/50a95aeb721142abc29e008f953c515ed5086364)) by @DEgITx
- **feed:** Feed widget support([027be8c](https://github.com/DEgITx/rats-search/commit/027be8c3f397ae90e23d925088dd685a1016a533)) by @DEgITx
- **gui:** Display rating in torrent line (gui part)([a9c2708](https://github.com/DEgITx/rats-search/commit/a9c27083dad272f24df9d425a91f5eaa6e1bf2d8)) by @DEgITx
- **vote:** Rating update on torrent request([51caccb](https://github.com/DEgITx/rats-search/commit/51caccbce476b0dd4f7826eacbb291a31596197d)) by @DEgITx
- **p2p:** P2p feed([dd8dc7b](https://github.com/DEgITx/rats-search/commit/dd8dc7b4189cb46122fd6fa26c033418387425a6)) by @DEgITx
- **feed:** Replicate remote feed from peers([5dfc4dd](https://github.com/DEgITx/rats-search/commit/5dfc4ddc384bb58c3df1aec8c329886cfa73cb68)) by @DEgITx
- **activity:** Activity page / feed separate([f09c6ab](https://github.com/DEgITx/rats-search/commit/f09c6ab43f16e6f65fc7cd38bb5493c31d05834b)) by @DEgITx
- **search:** New search navigation([da6093a](https://github.com/DEgITx/rats-search/commit/da6093ad4ab2bbcbd5d4fcee7fa36af71795e76a)) by @DEgITx
- **store:** Live sync store records on connection to every peer([19380db](https://github.com/DEgITx/rats-search/commit/19380dbd9f18ee092ddedb627d8313c45eb58831)) by @DEgITx


### 🧪 Testing


- **bigtable:** Test for bigtable script([1fbc889](https://github.com/DEgITx/rats-search/commit/1fbc889011abd9ec1d965dec30fef89ad3c6c3ae)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 28 commits


## [0.20.1](https://github.com/DEgITx/rats-search/compare/v0.20.0...v0.20.1) (2018-05-20)


### 🐛 Bug Fixes


- **header:** Fix panel stick([52cdf93](https://github.com/DEgITx/rats-search/commit/52cdf93942d32f5f7b376ce58e5184ceaf85a261)) by @DEgITx
- **changelog:** Fix changelog display([db68f8b](https://github.com/DEgITx/rats-search/commit/db68f8b92618f16cf009402d891b039344e4b201)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 2 commits


## [0.20.0](https://github.com/DEgITx/rats-search/compare/v0.19.2...v0.20.0) (2018-05-20)


### 🐛 Bug Fixes


- **search:** Fix search more button indication on peer search([f909fb5](https://github.com/DEgITx/rats-search/commit/f909fb5aaa5abc1e510fb5553bf339c2074e37f9)) by @DEgITx
- **log:** Clear query log on every start([92fdd54](https://github.com/DEgITx/rats-search/commit/92fdd54ffa5d38a0979fe89381d156bb303888ae)) by @DEgITx
- **header:** Fix advanced search([8423f19](https://github.com/DEgITx/rats-search/commit/8423f19a745e14ea7d1809d3af41d22d2a25daf6)) by @DEgITx
- **header:** More enchantment over header([0fe7ee2](https://github.com/DEgITx/rats-search/commit/0fe7ee2a1fc77d58153816591787c06f2b3dcd5b)) by @DEgITx
- **header:** Fix header wrap([643cfaf](https://github.com/DEgITx/rats-search/commit/643cfaf2e1025179bd2b9ceea8cc1da2f8fdb6b0)) by @DEgITx
- **header:** Normal error padding on search panel([80fea92](https://github.com/DEgITx/rats-search/commit/80fea92cbbc764274d3ce06180a9b20917554aad)) by @DEgITx
- **cicleci:** Fix build on cicleci([fa7e04d](https://github.com/DEgITx/rats-search/commit/fa7e04d05676609afffe2cdd7b9feabc1243138d)) by @DEgITx
- **top:** Bold like styling on tab chose([090a34f](https://github.com/DEgITx/rats-search/commit/090a34fb8799062824256a23f5d1756fc9a98562)) by @DEgITx
- **translations:** Init app translations([d3edbb6](https://github.com/DEgITx/rats-search/commit/d3edbb60fae39e7748cedbc643b638fec8dd3c42)) by @DEgITx
- **translations:** Checked language at menu([127192b](https://github.com/DEgITx/rats-search/commit/127192b14df13b099d9ca142dd63f839a6a6d21d)) by @DEgITx
- **translations:** More translations([d94e609](https://github.com/DEgITx/rats-search/commit/d94e609b9a5862d088c0e601fbbb7b9433062ac2)) by @DEgITx
- **search:** Fix container width([b1fceb5](https://github.com/DEgITx/rats-search/commit/b1fceb5450807a76191cc599d5748395a7998dc4)) by @DEgITx


### 🚀 Features


- **search:** Moved search to header (now search are always on)([31a6a44](https://github.com/DEgITx/rats-search/commit/31a6a44184d6327f47d2b9ec5f3a391bc974dfaf)) by @DEgITx
- **p2p:** PeerId protocol handshake([524b4d9](https://github.com/DEgITx/rats-search/commit/524b4d9601fd32f1122a7899998f463b5c004ee2)) by @DEgITx
- **header:** Statistic panel interface([666f4c1](https://github.com/DEgITx/rats-search/commit/666f4c11a05b957fa9d1de217e634891664c68c8)) by @DEgITx
- **top:** Top over different time over all types #26([7ba5aab](https://github.com/DEgITx/rats-search/commit/7ba5aab9a2fc1f5eaea659d9a3de2e08c207bd02)) by @DEgITx
- **core:** Dependency update([4bc63e3](https://github.com/DEgITx/rats-search/commit/4bc63e3b8c22db0f02205232ffb851fa023dc68f)) by @DEgITx
- **core:** More deps update([f882f6b](https://github.com/DEgITx/rats-search/commit/f882f6b1aabdac606bbb8090ff2429fa58274553)) by @DEgITx
- **p2p:** Mapping all peers dicts([60b5e8d](https://github.com/DEgITx/rats-search/commit/60b5e8d8b0d0555fc8284c735e78dbece1fc5f64)) by @DEgITx
- **filtering:** Negative name filter([21dedba](https://github.com/DEgITx/rats-search/commit/21dedba555d700d84ae56c683c9490644075a68e)) by @DEgITx
- **translations:** Basic translations support([feff322](https://github.com/DEgITx/rats-search/commit/feff3225920312e7360a7bfb603171b8728a865a)) by @DEgITx
- **translations:** Generate words on translation([0d39bbd](https://github.com/DEgITx/rats-search/commit/0d39bbd9c35f458ac17f52a49a2c103d84ea240b)) by @DEgITx
- **translations:** Menu translations([3056779](https://github.com/DEgITx/rats-search/commit/305677989535a9e7f3a875b303bdbff5986d1cbe)) by @DEgITx
- **translations-ru:** Translation to russian (part 1)([7f172cf](https://github.com/DEgITx/rats-search/commit/7f172cf6be5f38358a7aac8f522d917197110d04)) by @DEgITx
- **translations-ru:** More translations (part 2)([c341130](https://github.com/DEgITx/rats-search/commit/c341130da1fd245fe6ce8b863557a17203298472)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 25 commits


## [0.19.2](https://github.com/DEgITx/rats-search/compare/v0.19.1...v0.19.2) (2018-05-04)


### 🐛 Bug Fixes


- **search:** Error on sorting files in search([6a59744](https://github.com/DEgITx/rats-search/commit/6a59744f73b29142796ce1c127efa534f6283ee2)) by @DEgITx
- **linux:** Fix patching on some systems without electron([2d2a3ad](https://github.com/DEgITx/rats-search/commit/2d2a3ad3ca33debfc8e23adad6f8c76e1fa56af4)) by @DEgITx
- **trackers:** Fix possible trackers error on scrape([611e390](https://github.com/DEgITx/rats-search/commit/611e3908526c8c3d1786ae9ed16ba7689d7e01fb)) by @DEgITx


### 📚 Documentation


- **alpine:** Docs about alpine prebuild binaries([1dac470](https://github.com/DEgITx/rats-search/commit/1dac4707b3c7206a0b4b0ce8b2d1b029b2d58128)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 4 commits


## [0.19.1](https://github.com/DEgITx/rats-search/compare/v0.19.0...v0.19.1) (2018-04-30)


### 🐛 Bug Fixes


- **donate:** More relevant donate view, display also patreon donate :)([f3e5edf](https://github.com/DEgITx/rats-search/commit/f3e5edf6ec3dd204b9ae9844a1a397f338169bb4)) by @DEgITx


### 📚 Documentation


- **server:** Documentation about server configuration([3eb898d](https://github.com/DEgITx/rats-search/commit/3eb898db9b496693904d60acb038dcbf6af4a97e)) by @DEgITx
- **readme:** More relevant information about features([3c825e2](https://github.com/DEgITx/rats-search/commit/3c825e2846925653c2358e0cc60989f3bec684c1)) by @DEgITx
- **imgs:** Updated screenshots on usage manual([61d35e0](https://github.com/DEgITx/rats-search/commit/61d35e03fb5f2b37fa6ab5270e7046a3fc5b609f)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 4 commits


## [0.19.0](https://github.com/DEgITx/rats-search/compare/v0.18.0...v0.19.0) (2018-04-26)


### ⚡ Performance


- **top:** Less cpu usage on top page update([fce7fe4](https://github.com/DEgITx/rats-search/commit/fce7fe43691de4abba399747cc00f9dbbb5840b8)) by @DEgITx
- **peerdb:** Faster store sync([0768e8d](https://github.com/DEgITx/rats-search/commit/0768e8d3e054970af028eed0d8db2512a3789f0b)) by @DEgITx


### 🐛 Bug Fixes


- **header:** More pleasant animation([410c827](https://github.com/DEgITx/rats-search/commit/410c827905ec41a6905d9e1e940d2db38fa30657)) by @DEgITx
- **peerdb:** Transfer data fix([0aa2809](https://github.com/DEgITx/rats-search/commit/0aa2809f97f4fe1d0a53728d4d599148613adf3b))
- **votes:** Actual votes display works now([5e02b0d](https://github.com/DEgITx/rats-search/commit/5e02b0df73e67d8994df80a843965be19e2ea22c)) by @DEgITx
- **vote:** Proper vote data sequencing([f233100](https://github.com/DEgITx/rats-search/commit/f23310065d3bb79cdf25da3fd27062f1a4228ce1)) by @DEgITx
- **store:** Store on all peers([5773fa7](https://github.com/DEgITx/rats-search/commit/5773fa7408c304eba1c2eed3510f302cf456a5be)) by @DEgITx
- **search:** Show types of torrents on files search fix([e5e9aa2](https://github.com/DEgITx/rats-search/commit/e5e9aa2a889667e24bb5a81381db8f6177dc234b)) by @DEgITx
- **search:** Fix some unsafe results in safe search([1465dc9](https://github.com/DEgITx/rats-search/commit/1465dc9677d0491ff043c991641432557629a436)) by @DEgITx
- **content:** Basic adult filtration([aae2ddf](https://github.com/DEgITx/rats-search/commit/aae2ddf458247edf0997e30b1da640bea81f3232)) by @DEgITx


### 🚀 Features


- **header:** Fixed header on main window([5ae597f](https://github.com/DEgITx/rats-search/commit/5ae597f35a2ba27d4e7f7876dc416e2b0813175c)) by @DEgITx
- **peerDB:** Store on peers feature([5407ef8](https://github.com/DEgITx/rats-search/commit/5407ef8c59eb9fa42f1def2d74dbc2e4f72fe60d)) by @DEgITx
- **vote:** Restored voting (now working over p2p)([c8e37ae](https://github.com/DEgITx/rats-search/commit/c8e37ae4b2208e1635dc14ee700767e162fd356a)) by @DEgITx
- **replication:** Replicate torrent on open([34e9641](https://github.com/DEgITx/rats-search/commit/34e964126b8829fcbb3292de742c2132b07d55ba)) by @DEgITx
- **filters:** Some examples of regex #25([3c6e959](https://github.com/DEgITx/rats-search/commit/3c6e959d499808ecc2e815f9ac62c1f17eea711d)) by @DEgITx
- **vote:** Replicate torrent on voting([9593440](https://github.com/DEgITx/rats-search/commit/959344080ee467010b5c954d452cb8e8be33c64f)) by @DEgITx
- **search:** Separate adult remote content on search([81bd30c](https://github.com/DEgITx/rats-search/commit/81bd30c8a358dcc8c854578352ee1a56065322b4)) by @DEgITx
- **filters:** Adult filter([89a0e86](https://github.com/DEgITx/rats-search/commit/89a0e8689d86d4c131f4d3d61501f51c4c4bf86d)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 17 commits


## [0.18.0](https://github.com/DEgITx/rats-search/compare/v0.17.1...v0.18.0) (2018-04-07)


### 🐛 Bug Fixes


- **server:** Restored server messages([a2ade1c](https://github.com/DEgITx/rats-search/commit/a2ade1cd68a4a3b93dc7f6a04773dbc35d3d4bd1)) by @DEgITx
- **server:** Little makeup for server version([3b49d52](https://github.com/DEgITx/rats-search/commit/3b49d52afe4aef9aea6251a0c824cd5000c4f8c7)) by @DEgITx


### 📚 Documentation


- **server:** Web version setup docs([e0f7df9](https://github.com/DEgITx/rats-search/commit/e0f7df9706cb90be152b7a7f967512833001f841)) by @DEgITx


### 🔧 Refactor


- **closing:** Refactoring saving peers on closing([5daf9f5](https://github.com/DEgITx/rats-search/commit/5daf9f5c65a399fecd68abae7a96fdf94329d9e4)) by @DEgITx


### 🚀 Features


- **search:** Show current request with text #23([bd47d43](https://github.com/DEgITx/rats-search/commit/bd47d432545fa5a30ca701b0954918d651d647e7)) by @DEgITx
- **p2p:** Random peer exchange([fac6736](https://github.com/DEgITx/rats-search/commit/fac6736710b9b80582af5d086b3f66acb6a1f8ad)) by @DEgITx
- **server:** Seperate server part and ability to run server without electron([e244fab](https://github.com/DEgITx/rats-search/commit/e244fabd2c8c6b87464793e7991e444942ab2a7e)) by @DEgITx
- **web:** Web version available again([e25e8f0](https://github.com/DEgITx/rats-search/commit/e25e8f05a4f8752f08cd40429a293d6942a8abab)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 8 commits


## [0.17.1](https://github.com/DEgITx/rats-search/compare/v0.17.0...v0.17.1) (2018-04-04)


### 🐛 Bug Fixes


- **db:** Fix search index on replication([000947a](https://github.com/DEgITx/rats-search/commit/000947a6c2de413c87252ad5a786635a76d769f6)) by @DEgITx
- **filter:** Cleaning fix on big db([e581d95](https://github.com/DEgITx/rats-search/commit/e581d95194724d9b19407ec9091d201d09661f58)) by @DEgITx
- **db:** Resolving old database indexes([7a00f5e](https://github.com/DEgITx/rats-search/commit/7a00f5e2d1d2004a76d3d2eae6c96f6ddd913b38)) by @DEgITx
- **db:** Optimization after patching([74f1686](https://github.com/DEgITx/rats-search/commit/74f168695e6d0b759bfc75a99477d0b19eb115b5)) by @DEgITx
- **db:** Ignore patching already created db([0df2023](https://github.com/DEgITx/rats-search/commit/0df20237bb814af5e26cc3141e93972e55c6be12)) by @DEgITx
- **tests:** Fixed patching on tests([bcb8fa5](https://github.com/DEgITx/rats-search/commit/bcb8fa564d2b3bd8146c8859b89fa539b39f5811)) by @DEgITx


### 📚 Documentation


- **readme:** Display arch in readme([3b40e75](https://github.com/DEgITx/rats-search/commit/3b40e755dabd0e7fa21470bee214279bdfdcb1df)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 7 commits


## [0.17.0](https://github.com/DEgITx/rats-search/compare/v0.16.2...v0.17.0) (2018-04-01)


### 🐛 Bug Fixes


- **p2p:** Save only existing peers([1c89e3b](https://github.com/DEgITx/rats-search/commit/1c89e3b5eabbc4459bf8ea7e47ff988a98f1642c)) by @DEgITx
- **peers:** Fix display peers count in gui([99c9c28](https://github.com/DEgITx/rats-search/commit/99c9c281646e5e057051583079fd65c8a9fd24cf)) by @DEgITx
- **tests:** Fix quit timeout([4837293](https://github.com/DEgITx/rats-search/commit/4837293af4a1e68a697ad281c9d2d2590a4fc8c6)) by @DEgITx
- **tests:** Fix net code on test env([534999d](https://github.com/DEgITx/rats-search/commit/534999d1cb058619ca88d5038045f38999fc0b25))


### 🚀 Features


- **bootstrap:** Reserved p2p peers server([bda58b1](https://github.com/DEgITx/rats-search/commit/bda58b135cfa505ade8fd08ef2201268d3e08a26)) by @DEgITx
- **filter:** Torrents filters (basic maxFiles filter)([5348d1f](https://github.com/DEgITx/rats-search/commit/5348d1f88f4581440fada516b697165d6e51b8ee)) by @DEgITx
- **filter:** Naming filtering #17([99d9004](https://github.com/DEgITx/rats-search/commit/99d90049066979705badf08ce013a590c2b4d21c)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 6 commits


## [0.16.2](https://github.com/DEgITx/rats-search/compare/v0.16.1...v0.16.2) (2018-03-31)


### 🐛 Bug Fixes


- **replication:** Show correct torrent types on replication([6215c0e](https://github.com/DEgITx/rats-search/commit/6215c0ed60a61fc890d51b43a4b6d46b84202af3)) by @DEgITx


### 🔧 Refactor


- **faketors:** Removed old fake torrents code([884f93d](https://github.com/DEgITx/rats-search/commit/884f93d799698119edda4d00a27b259f6acf7aa6)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 2 commits


## [0.16.1](https://github.com/DEgITx/rats-search/compare/v0.16.0...v0.16.1) (2018-03-30)


### 🐛 Bug Fixes


- **p2p:** Fix torrents count([eec2412](https://github.com/DEgITx/rats-search/commit/eec241291b8548a1106ff672bfad9511cef633a0)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


## [0.16.0](https://github.com/DEgITx/rats-search/compare/v0.15.2...v0.16.0) (2018-03-30)


### 🐛 Bug Fixes


- **linux:** 32bit linux builds #21([7952c28](https://github.com/DEgITx/rats-search/commit/7952c283efc3ed4bd9e2510ae2bf66e40cc50b4a)) by @DEgITx
- **replication:** Fix error on replication([b5c19cc](https://github.com/DEgITx/rats-search/commit/b5c19ccae6e72125ed4a9f6caaccaf26a06f723c)) by @DEgITx
- **replication:** Enable replication by default([6ae6713](https://github.com/DEgITx/rats-search/commit/6ae67135737860899ed073e45eca209c704b7f18)) by @DEgITx
- **db:** Fix some null values errors([fe4e293](https://github.com/DEgITx/rats-search/commit/fe4e2932d1953e16dc779862a8e9762ac1331753)) by @DEgITx


### 🚀 Features


- **p2p:** App version on p2p([6a63bc6](https://github.com/DEgITx/rats-search/commit/6a63bc626df473de512422f16878df0dea640368)) by @DEgITx
- **p2p:** Show torrents number of other clients([2ba4938](https://github.com/DEgITx/rats-search/commit/2ba49380f61200264b5e1a9baf347dc57dd20e56)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 6 commits


## [0.15.2](https://github.com/DEgITx/rats-search/compare/v0.15.1...v0.15.2) (2018-03-28)


### 🐛 Bug Fixes


- **settings:** Save settings fix #19([eaa7aa7](https://github.com/DEgITx/rats-search/commit/eaa7aa7f7c7cefe14f4cc3612768991801c570f3)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


## [0.15.1](https://github.com/DEgITx/rats-search/compare/v0.15.0...v0.15.1) (2018-03-27)


### 🐛 Bug Fixes


- **top:** Remote top shown fix([c1cb197](https://github.com/DEgITx/rats-search/commit/c1cb1976b5714c167bf7e14e1fd916fdf980808d)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


## [0.15.0](https://github.com/DEgITx/rats-search/compare/v0.14.1...v0.15.0) (2018-03-26)


### 🐛 Bug Fixes


- **portable:** Portable win32 build([653d393](https://github.com/DEgITx/rats-search/commit/653d3936ae3d8c78f377557f281bfb52cae4e90f)) by @DEgITx
- **top:** Fix week and month top load([99b48ef](https://github.com/DEgITx/rats-search/commit/99b48ef28525727f6ea7f3317c2f5b65f89398fd)) by @DEgITx


### 📚 Documentation


- **readme:** Screenshots([9e00d0f](https://github.com/DEgITx/rats-search/commit/9e00d0ff691eafca749ad11ab797c2f8643fb842)) by @DEgITx


### 🔧 Refactor


- **api:** Moved to seperate file([5c777cb](https://github.com/DEgITx/rats-search/commit/5c777cb6297c68f6d832f2d4e49d11263f9f4218)) by @DEgITx


### 🚀 Features


- **win:** Win 32bit build([d353edc](https://github.com/DEgITx/rats-search/commit/d353edc56fe8231e7abc97cf89af243c03fcd89f)) by @DEgITx
- **top:** P2p top search([960731f](https://github.com/DEgITx/rats-search/commit/960731fb8045094ad5d376f68fb1e9e41baaf843)) by @DEgITx
- **top:** Categories to tab items and new items load([c78857d](https://github.com/DEgITx/rats-search/commit/c78857db60c208dc6cac2750ac5598cc640b19cd)) by @DEgITx
- **header:** Back to main button at top([0714131](https://github.com/DEgITx/rats-search/commit/0714131a5f06f7bce1844fd00abc6d4ab639c06c)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 8 commits


## [0.14.1](https://github.com/DEgITx/rats-search/compare/v0.14.0...v0.14.1) (2018-03-17)


### 🐛 Bug Fixes


- **p2p:** Still fix adding youself to list([ca881ff](https://github.com/DEgITx/rats-search/commit/ca881fff8121518abff7ac3a52568f98eec4dac3)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


## [0.14.0](https://github.com/DEgITx/rats-search/compare/v0.13.0...v0.14.0) (2018-03-17)


### 🐛 Bug Fixes


- **p2p:** Disconnect on protocol bad handshake on server side([349a015](https://github.com/DEgITx/rats-search/commit/349a01557e90d1444805142a4a54a43098824aba)) by @DEgITx
- **portable:** Fix error on renaming portable directory([07f3ab2](https://github.com/DEgITx/rats-search/commit/07f3ab2bd94465089535742c040c51916525efaa)) by @DEgITx
- **ssh:** Fix closing ssh on exit([cc1fd1d](https://github.com/DEgITx/rats-search/commit/cc1fd1d49d5fd1a621813b79ffa858c5055c6e28)) by @DEgITx


### 🚀 Features


- **menu:** New menu items (support)([b346a1a](https://github.com/DEgITx/rats-search/commit/b346a1a25a832c66e8f60aec2af4fc7a4c83ac02)) by @DEgITx
- **menu:** Version as menu item([0f2a7b4](https://github.com/DEgITx/rats-search/commit/0f2a7b44dd4f8e16da4ab623ceb4640b87b85c2d)) by @DEgITx
- **p2p:** Tunnels support for bad peers([500b852](https://github.com/DEgITx/rats-search/commit/500b8522091514717da1a64a1f58f3dacf6aa8a3)) by @DEgITx
- **ssh:** Fix key store([0b8732c](https://github.com/DEgITx/rats-search/commit/0b8732cbb8bd009fae41b7e9f5c075beb6e76633)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 7 commits


## [0.13.0](https://github.com/DEgITx/rats-search/compare/v0.12.0...v0.13.0) (2018-03-13)


### 🐛 Bug Fixes


- **info:** Fix info files limitation in output([cfff5c7](https://github.com/DEgITx/rats-search/commit/cfff5c7603c2aa8566f60705601b2ff148300346)) by @DEgITx
- **p2p:** Fix connection by yourself in process([223fe8c](https://github.com/DEgITx/rats-search/commit/223fe8c1abcaa91bd60ee530e8606342636678da)) by @DEgITx
- **gui:** Fix date displaing in torrent details([911c650](https://github.com/DEgITx/rats-search/commit/911c650dd3f08844c82fbb82625078bfb04173c0)) by @DEgITx
- **gui:** Loading progress when open top([268c7d3](https://github.com/DEgITx/rats-search/commit/268c7d30e4202d077b834ad37fc7600bb3a85195)) by @DEgITx
- **scanner:** Always add id - replication fix([9c9e64e](https://github.com/DEgITx/rats-search/commit/9c9e64e9a1c45a4eb224bf859f3e1c761562b11b)) by @DEgITx


### 📚 Documentation


- **manual:** About peers search([54c9fb1](https://github.com/DEgITx/rats-search/commit/54c9fb10b7b4ea45682bbb2f3b542c6399b1ef34)) by @DEgITx


### 🚀 Features


- **manual:** Usage manual (help) in menu([cbc6a49](https://github.com/DEgITx/rats-search/commit/cbc6a49aec3fe2ba35063fa4567c452a87d596c9)) by @DEgITx
- **p2p:** P2p torrents replication([db8a8cb](https://github.com/DEgITx/rats-search/commit/db8a8cbd36cc95167f311982fe2ba92adbab7eb7)) by @DEgITx
- **gui:** New header design (more compressed)([b03655e](https://github.com/DEgITx/rats-search/commit/b03655e57083a0245ddd12a4cddee27815e0a48e)) by @DEgITx
- **gui:** Compact header design([ce4e364](https://github.com/DEgITx/rats-search/commit/ce4e364e72d8296ceba17e93d0a09fd5385c0818)) by @DEgITx
- **gui:** Settings on header([eeb4a97](https://github.com/DEgITx/rats-search/commit/eeb4a97899bfd63086ac1c64e587243c68f07f63)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 11 commits


## [0.12.0](https://github.com/DEgITx/rats-search/compare/v0.11.0...v0.12.0) (2018-03-10)


### 🐛 Bug Fixes


- **macos:** Mac os icon size fix #10([3c0930f](https://github.com/DEgITx/rats-search/commit/3c0930fcd63c7b58d1e73228e11f7617c10ff581)) by @DEgITx


### 🚀 Features


- **p2p:** Peers limitation to prevent connection spam([022a6da](https://github.com/DEgITx/rats-search/commit/022a6dae7937d19bcc23cc6ecd9c4d9764511d64)) by @DEgITx
- **p2p:** Life peers exchange([c7a217e](https://github.com/DEgITx/rats-search/commit/c7a217ec37b4e486ca711051dad008f1d87a85ef)) by @DEgITx
- **p2p:** Bootstrap peers([10d0a2f](https://github.com/DEgITx/rats-search/commit/10d0a2fbc072abba68aee603a9908dba1ab12a4a)) by @DEgITx
- **portable:** Portable win64 version support #9([8ae812f](https://github.com/DEgITx/rats-search/commit/8ae812f18ed8ea0cba752c6780f5e181819de6ef)) by @DEgITx
- **search:** Add 100 mb interval to search #12([2beabe0](https://github.com/DEgITx/rats-search/commit/2beabe084dc7a2155cc876510127572be936c550)) by @DEgITx
- **config:** Peers number settings([6838b93](https://github.com/DEgITx/rats-search/commit/6838b938b7cfc97d5846af817cb2923d1966906f)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 7 commits


## [0.11.0](https://github.com/DEgITx/rats-search/compare/v0.10.0...v0.11.0) (2018-03-07)


### 🐛 Bug Fixes


- **p2p:** Situation when ignoring yourself address is very slow([5cd9b9f](https://github.com/DEgITx/rats-search/commit/5cd9b9f22922a53b6a141416244f294673340f7b)) by @DEgITx
- **changelog:** Fix modal peers display([c407998](https://github.com/DEgITx/rats-search/commit/c40799816b3ad7c7ee9e9239ad06279e04604b7c)) by @DEgITx
- **gui:** Typo in save settings([d5a9d8c](https://github.com/DEgITx/rats-search/commit/d5a9d8c0cd35acbed8651de86193afc8eeefdc5e)) by @DEgITx


### 🚀 Features


- **download:** Torrent downloads manager([1e86ba8](https://github.com/DEgITx/rats-search/commit/1e86ba84c12eca131f8ae064101abb7242dda393)) by @DEgITx
- **gui:** Access to top from menu([32bf512](https://github.com/DEgITx/rats-search/commit/32bf512bf5eb254fd2aeebb0ff1e5d46f816c43b)) by @DEgITx
- **donate:** Donate window([ae45555](https://github.com/DEgITx/rats-search/commit/ae455551169c851a1c4a73915568088323088dab)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 6 commits


## [0.10.0](https://github.com/DEgITx/rats-search/compare/v0.9.0...v0.10.0) (2018-03-03)


### 🐛 Bug Fixes


- **p2p:** Some ignoring of local addresses([0209d3e](https://github.com/DEgITx/rats-search/commit/0209d3e498ee4e5b8ff4e59ed28a12d00c8279b8)) by @DEgITx
- **p2p:** Fix back connection([e8bef06](https://github.com/DEgITx/rats-search/commit/e8bef06697f76777b0ad6797320538a3df24cebb)) by @DEgITx
- **log:** Dont show any upnp messages in log([d26dc54](https://github.com/DEgITx/rats-search/commit/d26dc54a5e2353dc01d59db433335cd84db03870)) by @DEgITx
- **search:** Merge files in search when same torrent twice in a row([a60beea](https://github.com/DEgITx/rats-search/commit/a60beeaee3baed1dcee5593c56e0d62dbd67c5e4)) by @DEgITx
- **gui:** Cleanup footer([5832626](https://github.com/DEgITx/rats-search/commit/5832626fd230b5ce78c4812f2b68f32bc821de77)) by @DEgITx
- **network:** Fix stun error on startup if no network available([3bed2b1](https://github.com/DEgITx/rats-search/commit/3bed2b13cb0f3bcd70f226bcb734213e1e36def2)) by @DEgITx
- **gui:** Some incorrect information removed from top header([a8db927](https://github.com/DEgITx/rats-search/commit/a8db927c8d928970e711ad681a99fc1bc896b93c)) by @DEgITx
- **vote:** Just disable for future improvements in p2p([bbea5c5](https://github.com/DEgITx/rats-search/commit/bbea5c5d8a03e5db34b641a4a468216e726c91c1)) by @DEgITx
- **search:** Show torrent page from remote peer([1c626fa](https://github.com/DEgITx/rats-search/commit/1c626fa2bffb5ab1fa033bb5cc9df6cef3409515)) by @DEgITx


### 🚀 Features


- **p2p:** Protocol check and support responce p2p connection([2779bfe](https://github.com/DEgITx/rats-search/commit/2779bfe49dd2b1a32197a7487b89da0f9eaac12c)) by @DEgITx
- **p2p:** Peers exchange([c2b030d](https://github.com/DEgITx/rats-search/commit/c2b030dddc13ab91dbc238464c5f1aa6213753e2)) by @DEgITx
- **secure:** Responce only to rats messages([6535494](https://github.com/DEgITx/rats-search/commit/65354947ba74297d3ca5ab170a7e8b9614c0daa1)) by @DEgITx
- **search:** Visual separate p2p torrents([b579106](https://github.com/DEgITx/rats-search/commit/b579106c66124024a4a9ccb0a76ba4a4fb626899)) by @DEgITx
- **p2p:** Peers backup on exit([6de95a1](https://github.com/DEgITx/rats-search/commit/6de95a1addc0572eebc956d601a42267fc05e3e0)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 14 commits


## [0.9.0](https://github.com/DEgITx/rats-search/compare/v0.8.0...v0.9.0) (2018-03-02)


### 📚 Documentation


- **usage:** English basic translation of usage manual([0b39227](https://github.com/DEgITx/rats-search/commit/0b39227a1f075f89d7ac4464f65ab69a4b8e9f57)) by @DEgITx


### 🚀 Features


- **network:** UPnP support([b876c71](https://github.com/DEgITx/rats-search/commit/b876c71f124ef47ada8e6f3f22bacb7cb8352b52)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 2 commits


## [0.8.0](https://github.com/DEgITx/rats-search/compare/v0.7.1...v0.8.0) (2018-03-01)


### ⚡ Performance


- **network:** Network usage optimization([590671e](https://github.com/DEgITx/rats-search/commit/590671e1f5bdb826a5978701209094eacd1d9f3a)) by @DEgITx
- **network:** Limit package usage([bdaaa02](https://github.com/DEgITx/rats-search/commit/bdaaa020cc23bd38d5fc565c0712e0dd214a585f)) by @DEgITx


### 📚 Documentation


- **docs:** Basic ru doc([30e2c53](https://github.com/DEgITx/rats-search/commit/30e2c53c676b8cc3fa9f68720a25106109da51b1)) by @DEgITx


### 🚀 Features


- **config:** Some network limitations([9a95903](https://github.com/DEgITx/rats-search/commit/9a95903cd5e298456e388e981333d084e75a778c)) by @DEgITx
- **config:** New scanning options([df969bf](https://github.com/DEgITx/rats-search/commit/df969bf9b95819b895c99598bb4dd1910f3573a5)) by @DEgITx
- **menu:** Report bug in application([3eda3f8](https://github.com/DEgITx/rats-search/commit/3eda3f84ca3fa41eec2c78368c2784aa19eaaa91)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 6 commits


## [0.7.1](https://github.com/DEgITx/rats-search/compare/v0.7.0...v0.7.1) (2018-02-21)


### 🐛 Bug Fixes


- **config:** Settings port descriptions([d560e5e](https://github.com/DEgITx/rats-search/commit/d560e5ec23027d8a21fc1d075c1d394f1956ae53)) by @DEgITx
- **p2p:** Errors on peers ignore([c91ade5](https://github.com/DEgITx/rats-search/commit/c91ade59031949bab8db63aa4220c46c424a59d5)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 2 commits


## [0.7.0](https://github.com/DEgITx/rats-search/compare/v0.6.1...v0.7.0) (2018-02-18)


### ⚡ Performance


- **p2p:** Reduce traffic of p2p calls([d011d6e](https://github.com/DEgITx/rats-search/commit/d011d6ed825e471eec1742d326ebf8435be4ae4c)) by @DEgITx


### 🐛 Bug Fixes


- **p2p:** Compact convert ip's([a57ff6e](https://github.com/DEgITx/rats-search/commit/a57ff6e43021aebf0fd1fec1090175b32a91d591)) by @DEgITx
- **p2p:** Fix disconnects after each call([98f0404](https://github.com/DEgITx/rats-search/commit/98f040486963d4a10fee0613fd18849e6666c1e0)) by @DEgITx
- **p2p:** Closing on end([196f046](https://github.com/DEgITx/rats-search/commit/196f04645cdd2e2d0a1f41a0d4fb93c9e806cbd2)) by @DEgITx
- **app:** Messaging on closing([cbab330](https://github.com/DEgITx/rats-search/commit/cbab33039a201b9117625689901b899ee83085dc)) by @DEgITx
- **p2p:** Closing on end reverted (it was ok by default)([f23d459](https://github.com/DEgITx/rats-search/commit/f23d45996beb0ded5d42fbf452a342cf029afbb4)) by @DEgITx
- **config:** Color of search indicator([fd4f3b0](https://github.com/DEgITx/rats-search/commit/fd4f3b09e5a77a0d3901634b69cae8ee1ddb7205)) by @DEgITx
- **p2p:** Ignore errors on conn([64ab971](https://github.com/DEgITx/rats-search/commit/64ab97127ad3de84786db7c7c88e6527d8ee64ea)) by @DEgITx


### 🚀 Features


- **p2p:** Other rats peers detection([b1d2c4a](https://github.com/DEgITx/rats-search/commit/b1d2c4a3345f611b767e09daf412840849328a02)) by @DEgITx
- **p2p:** Basic search retransfer([91f5a3f](https://github.com/DEgITx/rats-search/commit/91f5a3fabd6caef884f4f9bd4d74c9e86ed1df90)) by @DEgITx
- **p2p:** Files search also([c33f927](https://github.com/DEgITx/rats-search/commit/c33f927a7d394e41b6b316784e34d9fe1bb5f88b)) by @DEgITx
- **app:** P2p peers indicator([e5b9e47](https://github.com/DEgITx/rats-search/commit/e5b9e4793a4a0de588c94fa3effa2adc4ed14032)) by @DEgITx
- **config:** P2p enable/disable([ca7afa6](https://github.com/DEgITx/rats-search/commit/ca7afa614ad7e37ac8818d91270603aef6dfdac6)) by @DEgITx
- **p2p:** Ignore your address([08b73aa](https://github.com/DEgITx/rats-search/commit/08b73aa7a9b2dbc8bbe418ad965f2ea88a4e7903)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 14 commits


## [0.6.0](https://github.com/DEgITx/rats-search/compare/v0.5.0...v0.6.0) (2018-02-12)


### 🐛 Bug Fixes


- **macos:** Fix updates on Mac OS X([fd64c70](https://github.com/DEgITx/rats-search/commit/fd64c708538e3d76c2112381201603393d2792d8)) by @DEgITx
- **macos:** Normal icon([2125abc](https://github.com/DEgITx/rats-search/commit/2125abc9b4a4224ddf78ab32525dd1f4fc6a3418)) by @DEgITx
- **windows:** Start fix in some cases (possible win7 fix)([464da34](https://github.com/DEgITx/rats-search/commit/464da346d1c882f465abc1d00145c57a13533247)) by @DEgITx
- **macos:** Fix closing under some platforms([a5caf25](https://github.com/DEgITx/rats-search/commit/a5caf25269a0bcf87181c4f13f652c447aab1c93)) by @DEgITx


### 🔧 Refactor


- **path:** Search path module([4a85510](https://github.com/DEgITx/rats-search/commit/4a855104b1bd5476e2dfb62272023976135c8fae)) by @DEgITx


### 🚀 Features


- **log:** Log autoupdate info([cb28d1e](https://github.com/DEgITx/rats-search/commit/cb28d1edee6a793316e2b316b58c4e2ae9a5c0b2)) by @DEgITx
- **log:** System information([b984565](https://github.com/DEgITx/rats-search/commit/b984565b0f9bb6c2bd4158f7f6d17f5c1fd5710d)) by @DEgITx
- **app:** Allow run only single application at same time([ae8b35e](https://github.com/DEgITx/rats-search/commit/ae8b35e68561bac34a2066a049551a258a5bbc28)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 8 commits


## [0.5.0](https://github.com/DEgITx/rats-search/compare/v0.4.0...v0.5.0) (2018-02-11)


### 🐛 Bug Fixes


- **autoupdate:** Fix error on downloading updater([f7a3308](https://github.com/DEgITx/rats-search/commit/f7a3308fab6f5177640ca30342fe61f980354272)) by @DEgITx


### 📚 Documentation


- **readme:** Notification about torrent integration([78e4090](https://github.com/DEgITx/rats-search/commit/78e4090c22a0c925c74d10da65bf8ca4b3d95fb8)) by @DEgITx


### 🚀 Features


- **autoupdate:** Autoupdate on statup([ee1a601](https://github.com/DEgITx/rats-search/commit/ee1a6013d60f71fe3546e139c65fc990a390a87f)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 3 commits


## [0.4.0](https://github.com/DEgITx/rats-search/compare/v0.3.1...v0.4.0) (2018-02-10)


### 🐛 Bug Fixes


- **app:** Support additional arguments on messages([0e46df3](https://github.com/DEgITx/rats-search/commit/0e46df3dbc44accb4df28a40b2d52a8941de03df)) by @DEgITx
- **app:** Fix removing events on render process([075bf7a](https://github.com/DEgITx/rats-search/commit/075bf7a9da0db907dbedddf4a7d4bb01284d78a2)) by @DEgITx
- **image:** Restored spinner on downloading metadata([63e1af9](https://github.com/DEgITx/rats-search/commit/63e1af9b59f5bf12bc838a781ad8b1228b0a7355)) by @DEgITx
- **download:** Problem on dht api after delete torrent([00d03ed](https://github.com/DEgITx/rats-search/commit/00d03ed81e9b9eb8e7b3a08ae0df9d25ce07ad49)) by @DEgITx
- **download:** Cancel controls on torrent element([ef70521](https://github.com/DEgITx/rats-search/commit/ef705211ab7f3cdc81d0f89c65d48163fac4c360)) by @DEgITx
- **download:** Bug when cancel button not appear in main page([73299fb](https://github.com/DEgITx/rats-search/commit/73299fbbc8d281342819c5a8e0e010edc84b0ea5)) by @DEgITx


### 📚 Documentation


- **readme:** Release version in readme([9741c92](https://github.com/DEgITx/rats-search/commit/9741c92019fefbe384daf61f7bb26ab371baaac0)) by @DEgITx


### 🚀 Features


- **download:** Basic torrent download support([765177d](https://github.com/DEgITx/rats-search/commit/765177d89d9dfcca3d4a0dc3252126b93fc30958)) by @DEgITx
- **download:** More control over torrent download([6b28874](https://github.com/DEgITx/rats-search/commit/6b288741d19803cb8e99eec16bb993b4b88ace83)) by @DEgITx
- **menu:** New context menu with copy/paste([c035e88](https://github.com/DEgITx/rats-search/commit/c035e880e960faf5886682e431d5170a89d0c3e9)) by @DEgITx
- **download:** Torrent progress on torrent element([603798c](https://github.com/DEgITx/rats-search/commit/603798cbce2c1b5fb62a2b500d60b8ed9cc076c7)) by @DEgITx
- **config:** Download torrents directory in config([d320448](https://github.com/DEgITx/rats-search/commit/d3204489034d60cf7eecc856a5489a8312d73714)) by @DEgITx
- **download:** Cancel downloading([76651b6](https://github.com/DEgITx/rats-search/commit/76651b6e026670a141e371e58728d6525f4de775)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 13 commits


## [0.3.1](https://github.com/DEgITx/rats-search/compare/v0.3.0...v0.3.1) (2018-02-06)


### 🐛 Bug Fixes


- **unix:** Resolve problem with icon part at some systems([4cc1e8a](https://github.com/DEgITx/rats-search/commit/4cc1e8a62ba129c194ff2716afcf7c7c84565847)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 1 commit


## [0.3.0](https://github.com/DEgITx/rats-search/compare/v0.2.0...v0.3.0) (2018-02-05)


### 🐛 Bug Fixes


- **config:** Fixed saving configuration on develop builds([9a99803](https://github.com/DEgITx/rats-search/commit/9a99803c68b55141f5c4677598d2b714c5a30de6)) by @DEgITx
- **feed:** Disabled fake torrents for desktop([74def9f](https://github.com/DEgITx/rats-search/commit/74def9f7638a7c5a2dfcdd00944c5fc973e9c6eb)) by @DEgITx
- **windows:** Resolve some encoding problem with search daemon process([7b4f370](https://github.com/DEgITx/rats-search/commit/7b4f37081fd19b3826587ca8fe843444f32eb661)) by @DEgITx


### 📚 Documentation


- **readme:** Contribution section([ce6698d](https://github.com/DEgITx/rats-search/commit/ce6698da973b9efa20c2c25ac4b8db0ae44216f9)) by @DEgITx


### 🚀 Features


- **config:** Database place path([17aff1a](https://github.com/DEgITx/rats-search/commit/17aff1aca0acecc53c709f4a8ca8c3ef14671865)) by @DEgITx
- **changelog:** Changelog inside application([9bac53f](https://github.com/DEgITx/rats-search/commit/9bac53f559f42551767ca7fce1f130660924d852)) by @DEgITx
- **config:** Message when settings saved([8212748](https://github.com/DEgITx/rats-search/commit/8212748f475991e8df023b9f0755abde42781232)) by @DEgITx




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 7 commits


## [0.2.0](https://github.com/DEgITx/rats-search/compare/v0.1.0...v0.2.0) (2018-02-04)


### 🐛 Bug Fixes


- **config:** Saving config restored([fa1512d](https://github.com/DEgITx/rats-search/commit/fa1512dee56a6332c23a82b419360f956ff122b2)) by @DEgITx
- **scanner:** Scanner second enable call([8bbe87e](https://github.com/DEgITx/rats-search/commit/8bbe87ec7417d0e07779953f87c300a1a80b2ba9)) by @DEgITx
- **interface:** Back button on save page([513df76](https://github.com/DEgITx/rats-search/commit/513df76c576875794c9b18498c18ef2332dbad8f)) by @DEgITx
- **app:** Fast and proper closing([837f595](https://github.com/DEgITx/rats-search/commit/837f59569f28e9866d3fd5c83397d8d6b97a2551)) by @DEgITx
- **linux:** Proper db use under linux([23e4b60](https://github.com/DEgITx/rats-search/commit/23e4b603d5b7249e502fc19ec9e93ec399c8294f))
- **linux:** Options on start([936612c](https://github.com/DEgITx/rats-search/commit/936612c91348b9493d2185939b6b9232c84766ea)) by @DEgITx
- **tests:** Disable scanner tests([2830525](https://github.com/DEgITx/rats-search/commit/28305250aa4484a310156bbf6bde8b8abd2288c6)) by @DEgITx
- **macos:** Proper work under MacOS X([d2bf25d](https://github.com/DEgITx/rats-search/commit/d2bf25de6bbad512d075396378365492097c3195)) by @DEgITx
- **macos:** Network category of application([3eb22fa](https://github.com/DEgITx/rats-search/commit/3eb22facb88a34f25d6b9be324440e4a42350d9f)) by @DEgITx


### 📦 Miscellaneous


- **tests:** Tests simplified([d21e093](https://github.com/DEgITx/rats-search/commit/d21e093ac7e570b127d6b7f0ee3134d90a65daad)) by @DEgITx
- **ci:** Restore travis build([40c8585](https://github.com/DEgITx/rats-search/commit/40c8585129beb4280811c0dee8f16d1c0a1ce2b4)) by @DEgITx
- **mac:** Move brew packages to release([e8e7179](https://github.com/DEgITx/rats-search/commit/e8e717992b1c8215eaad99adf3770924e91a6f9b)) by @DEgITx
- **ci:** Fix platform builds([547703f](https://github.com/DEgITx/rats-search/commit/547703faa34e8ba08a9cb69de86aec2bd225e396)) by @DEgITx
- **ci:** Changelog publish fix([501238b](https://github.com/DEgITx/rats-search/commit/501238b71a7a4dd3db9d985153d338792b78cd52)) by @DEgITx
- **ci:** Resolved problem with often builds([5a6c7f0](https://github.com/DEgITx/rats-search/commit/5a6c7f02bacf2641cbd5201d929b910e5daf71e3)) by @DEgITx


### 🔧 Refactor


- **db:** One basic function to all files push([5fc8ddd](https://github.com/DEgITx/rats-search/commit/5fc8ddde2bba306a76c4e2669ad273481fad2f30)) by @DEgITx
- **tests:** Rename e2e tests([6646381](https://github.com/DEgITx/rats-search/commit/6646381a49577ba691131befd7746f272a161ae8)) by @DEgITx


### 🚀 Features


- **config:** Saving configuration([3160720](https://github.com/DEgITx/rats-search/commit/316072008acb9aff5cd7cd18300619230a908f52)) by @DEgITx
- **db:** New database([98309ab](https://github.com/DEgITx/rats-search/commit/98309ab681587b3646d9665fc91fccb249ce6e49)) by @DEgITx
- **tests:** New scanner test([b205245](https://github.com/DEgITx/rats-search/commit/b205245e4009352a438aa29509bb7d86509c55aa)) by @DEgITx
- **tests:** Db test([ae6864b](https://github.com/DEgITx/rats-search/commit/ae6864ba558281ec40d5b0cf67b7021cc946ab52)) by @DEgITx
- **readme:** Readme([c904f8b](https://github.com/DEgITx/rats-search/commit/c904f8b11416522fbb7a5f0bd781afa6a6c9f5c9)) by @DEgITx
- **build:** Semantic release messages scan([354d68b](https://github.com/DEgITx/rats-search/commit/354d68bd5c39b508188850e4e6b89448ae747461)) by @DEgITx
- **readme:** Travis status build in readme([864d24c](https://github.com/DEgITx/rats-search/commit/864d24c783b906012e247c6c1be9f4d711a9c493)) by @DEgITx
- **readme:** Semantic release in readme([8f5ace0](https://github.com/DEgITx/rats-search/commit/8f5ace0a8001de1cfa93bdede8b4281fae2bc498)) by @DEgITx
- **config:** New settings for application (port, cpu usage eth.)([a87a59a](https://github.com/DEgITx/rats-search/commit/a87a59a828e56c38e0482f73c1a1f94fb4d9faf2)) by @semantic-release-bot




### 👥 Contributors



- [@DEgITx](https://github.com/DEgITx) — 24 commits


- [@semantic-release-bot](https://github.com/semantic-release-bot) — 1 commit


## [0.1.0](https://github.com/DEgITx/rats-search/compare/...v0.1.0) (2018-01-29)




