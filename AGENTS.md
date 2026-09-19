# Rats Search - Development Guide

## Project Overview

**Rats on The Boat** is a high-performance BitTorrent search engine built with C++17 and Qt6. It crawls the DHT network, indexes torrents, and provides full-text search via Manticore Search. Supports desktop (GUI) and server (console) modes.

## Build Commands

### Configure & Build
```bash
# Configure (Debug)
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DRATS_SEARCH_BUILD_TESTS=ON

# Configure (Release)
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DRATS_SEARCH_BUILD_TESTS=ON

# Build
cmake --build build --config Debug --parallel

# Build with ASAN
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DRATS_ENABLE_ASAN=ON
```

### Run Tests
```bash
# Run all tests
cd build && ctest --output-on-failure --parallel

# Run specific test
cd build && ./tests/test_sphinxql

# Run with offscreen display (Linux headless)
QT_QPA_PLATFORM=offscreen ctest --output-on-failure
```

### Linting & Formatting
```bash
# clang-format (if configured)
clang-format -i src/*.cpp src/*.h src/api/*.cpp src/api/*.h

# Check with clang-tidy
clang-tidy src/*.cpp src/api/*.cpp -- -I src -I src/api
```

## Architecture

```
src/
├── main.cpp                    # Entry point, CLI parsing, GUI/console modes
├── mainwindow.cpp/h            # Main GUI window
├── settingsdialog.cpp/h        # Settings UI
├── torrentdatabase.cpp/h       # SQLite/Manticore database layer
├── torrentspider.cpp/h         # DHT crawler/indexer
├── p2pnetwork.cpp/h            # P2P networking (librats wrapper)
├── searchresultmodel.cpp/h     # Qt model for search results
├── torrentclient.cpp/h         # BitTorrent download client
├── torrentexporter.cpp/h       # .torrent file generation
├── manticoremanager.cpp/h      # Manticore Search process manager
├── sphinxql.cpp/h              # SphinxQL query builder
├── contentdetector.cpp/h       # Content type detection
├── autostartmanager.cpp/h      # OS autostart registration
├── startupinfo.cpp/h           # System info logging
├── toptorrentswidget.cpp/h     # Top torrents tab
├── feedwidget.cpp/h            # Activity feed tab
├── downloadswidget.cpp/h       # Downloads tab
├── activitywidget.cpp/h        # Activity feed widget
├── favoritesmanager.cpp/h      # Favorites persistence
├── favoriteswidget.cpp/h       # Favorites UI
├── torrentitemdelegate.cpp/h   # Custom list item rendering
├── torrentdetailspanel.cpp/h   # Torrent details panel
├── torrentfileswidget.cpp/h    # File tree widget
├── badwords.h                  # Content filter lists
├── utils.h                     # String utilities
├── version.h.in                # Version template (CMake generates version.h)
├── api/
│   ├── ratsapi.cpp/h           # Core API facade
│   ├── configmanager.cpp/h     # JSON config read/write
│   ├── feedmanager.cpp/h       # P2P feed aggregation
│   ├── apiserver.cpp/h         # REST/WebSocket API server
│   ├── updatemanager.cpp/h     # Auto-update checker
│   ├── translationmanager.cpp/h # i18n management
│   ├── p2pstoremanager.cpp/h   # P2P data storage
│   ├── trackerwrapper.cpp/h    # Tracker protocol wrapper
│   ├── trackerinfoscraper.cpp/h# Tracker metadata scraper
│   └── migrationmanager.cpp/h  # v1.x → v2.x migration
└── librats/                    # Git submodule: P2P networking library
```

## Key Conventions

### C++ Style
- **Standard**: C++17
- **Qt version**: 6.9+ (Qt6 with WebSockets)
- **Naming**: camelCase for variables/functions, PascalCase for classes
- **Headers**: Use `#pragma once`
- **Includes**: Qt headers first, then stdlib, then project headers
- **Smart pointers**: Prefer `std::unique_ptr` / `std::shared_ptr` over raw pointers
- **Strings**: Use `QString` throughout; convert at boundaries only

### Qt Patterns
- Use Qt's signal/slot mechanism for inter-component communication
- Use `QObject::connect()` with lambda slots for inline handlers
- Use `Q_PROPERTY` for properties exposed to QML or introspection
- Use `QTimer` for periodic tasks, not raw threads
- Use `QSqlDatabase` connection pooling for database access
- Use `QThread` + worker object pattern for background tasks

### Build System
- **CMake** minimum 3.16, use Ninja generator
- **MOC/RCC** auto-enabled via `CMAKE_AUTOMOC` / `CMAKE_AUTORCC`
- Tests use Qt Test framework (`QTest`)
- librats is a git submodule in `src/librats/`
- Platform-specific: Win32 RC file, macOS bundle, Linux AppImage

### Database
- Primary storage: SQLite (via Qt SQL)
- Full-text search: Manticore Search (SphinxQL protocol)
- Config: JSON file (`rats.json`) in data directory
- Manticore binaries bundled in `imports/{platform}/{arch}/`

### File Organization
- One class per header/source pair
- UI widgets: `*Widget.cpp/h`
- Managers: `*Manager.cpp/h`
- API layer: `src/api/` subdirectory
- Tests mirror source structure: `tests/test_*.cpp`

## Testing

Tests are in `tests/` and use Qt Test framework:
- `test_torrentinfo.cpp` - Torrent data structures
- `test_searchresultmodel.cpp` - Search model logic
- `test_sphinxql.cpp` - Query builder correctness
- `test_utils.cpp` - Utility functions
- `test_contentdetector.cpp` - Content type detection
- `test_updatemanager.cpp` - Update check logic
- `test_manticore_queries.cpp` - Integration tests (requires Manticore running)

### Writing Tests
```cpp
#include <QtTest>
#include "component.h"

class TestComponent : public QObject
{
    Q_OBJECT

private slots:
    void testBasicFunction();
    void testEdgeCase();
};

void TestComponent::testBasicFunction()
{
    Component c;
    QVERIFY(c.isValid());
    QCOMPARE(c.value(), expected);
}

QTEST_MAIN(TestComponent)
#include "test_component.moc"
```

## Common Tasks

### Adding a New Widget
1. Create `src/newwidget.cpp` and `src/newwidget.h`
2. Inherit from `QWidget` or appropriate base
3. Add to `SOURCES` and `HEADERS` in `CMakeLists.txt`
4. Follow existing widget patterns (e.g., `toptorrentswidget.cpp`)

### Adding a New API Endpoint
1. Add method to `src/api/ratsapi.h/cpp`
2. Wire it in `src/api/apiserver.cpp` for REST
3. Add to WebSocket handler if real-time updates needed

### Adding a Test
1. Create `tests/test_newfeature.cpp`
2. Add `add_rats_test(test_newfeature)` to `tests/CMakeLists.txt`
3. Add to the `check` target's `DEPENDS` list

## Dependencies

- **Qt 6.9+**: Core, Gui, Widgets, Network, Sql, Concurrent, WebSockets, LinguistTools
- **librats**: P2P networking (DHT, mDNS, NAT, Noise encryption, GossipSub)
- **Manticore Search**: Full-text search engine (bundled binaries)
- **CMake 3.16+**: Build system
- **Ninja**: Recommended build tool

## CI/CD

GitHub Actions builds for:
- Windows x64 (MSVC) → ZIP + Inno Setup installer
- Linux x64 (GCC) → AppImage / tar.gz
- macOS ARM (Apple Silicon) → DMG
- macOS Intel → DMG (temporarily disabled)

Tests run on all platforms in CI.

## Translations

- Source strings use `tr()` / `QT_TRANSLATE_NOOP()`
- `.ts` files in `translations/`
- Use `qt6_add_translation()` in CMake
- See `docs/TRANSLATION.md` for contributor guide

## Debugging

- Enable debug logging: Build with `CMAKE_BUILD_TYPE=Debug`
- AddressSanitizer: `-DRATS_ENABLE_ASAN=ON`
- ThreadSanitizer: `-DRATS_ENABLE_TSAN=ON` (mutually exclusive with ASAN)
- Log file: `<data-dir>/rats-search.log`
- Console mode: `./RatsSearch --console --spider`
