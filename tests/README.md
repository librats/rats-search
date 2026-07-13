# Rats Search Test Suite

Qt Test based. Every test links `ratscore` directly (the layered core library),
so no separate "testable" target is needed.

## Layout

```
tests/
├── CMakeLists.txt
├── test_domain.cpp             # domain::Torrent / File / content enums + codec
├── test_infohash.cpp           # infohash::normalize / isValid
├── test_query.cpp              # data/query.h SQL escaping and builders
├── test_config_store.cpp       # app::ConfigStore write path + change notification
├── test_update_service.cpp     # service::UpdateService state machine
├── test_content_classifier.cpp # domain::ContentClassifier (needs the Qt resources)
└── test_manticore_queries.cpp  # integration: spawns a real searchd
```

## Building and running

Tests are **off** by default:

```bash
cmake -B build -DRATS_SEARCH_BUILD_TESTS=ON
cmake --build build

cd build
ctest --output-on-failure                 # everything
ctest --output-on-failure -R test_query   # one, by name
./tests/test_query                        # or run the executable directly
```

`cmake --build build --target check` runs ctest through a custom target.

## Notes on the two special tests

- **`test_content_classifier`** reads its extension / bad-word tables from Qt
  resources (`:/content/*.json`). Those live in `resources/resources.qrc`, which
  is *not* compiled into `ratscore`, so the `.qrc` is compiled straight into this
  test executable.

- **`test_manticore_queries`** is an integration test: it starts a real `searchd`
  process from `imports/` and drives `Manticore` + `Database` +
  `TorrentRepository` against it. It needs Qt's **QMYSQL** driver; on Windows a
  POST_BUILD step copies `libmysql.dll` next to the test binary. Without the
  QMYSQL plugin the test fails in `initTestCase` with
  `QMYSQL driver not available` — an environment gap, not a code defect.

Every other test is a pure unit test with no external process.

## Adding a test

1. Write `test_<module>.cpp` with a `QTEST_MAIN(...)` and the matching
   `#include "test_<module>.moc"` at the bottom.
2. Register it in `tests/CMakeLists.txt`:

```cmake
add_rats_test(test_mymodule)
```

`add_rats_test()` creates the executable, links `Qt6::Test` + `ratscore`, and
calls `add_test()`. Add the name to the `check` target's `DEPENDS` list too.
