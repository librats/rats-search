# Rats Search - Unit Tests

This directory contains unit tests for the Rats Search application.

## Test Structure

```
tests/
├── CMakeLists.txt           # Test build configuration
├── test_torrentinfo.cpp     # TorrentInfo struct tests
├── test_searchresultmodel.cpp  # SearchResultModel tests
├── test_sphinxql.cpp        # SphinxQL escape/SQL building tests
└── README.md                # This file
```

## Building Tests

Tests are disabled by default. To build with tests enabled:

```bash
# From the project root directory
mkdir build-tests && cd build-tests
cmake -DRATS_SEARCH_BUILD_TESTS=ON ..
cmake --build .
```

## Running Tests

### Run All Tests

```bash
# Using CTest
cd build-tests
ctest --output-on-failure

# Or using the custom target
cmake --build . --target check
```

### Run Individual Tests

```bash
# Run specific test executable
./tests/test_torrentinfo
./tests/test_searchresultmodel
./tests/test_sphinxql
```

### Verbose Output

```bash
# Get detailed output for each test
ctest -V
```

## Test Categories

### TorrentInfo Tests (`test_torrentinfo.cpp`)

Tests for the core `TorrentInfo` struct:
- Default construction
- Hash validation (empty, short, valid, long)
- Content type ID mapping
- Content category ID mapping
- SearchOptions defaults
- TorrentFile struct

### SearchResultModel Tests (`test_searchresultmodel.cpp`)

Tests for the Qt table model:
- Empty model behavior
- Column count
- Setting/clearing results
- Getting torrents by row
- Data display roles
- Header data
- Custom roles (content type, hash)
- Size formatting (bytes, KB, MB, GB, TB)

### SphinxQL Tests (`test_sphinxql.cpp`)

Tests for SQL escape functions:
- Simple string escaping
- Quote escaping (single, double)
- Backslash escaping
- Newline/tab/carriage return escaping
- Complex string escaping
- Empty string handling

## Adding New Tests

1. Create a new test file `test_<module>.cpp`
2. Use Qt Test framework:

```cpp
#include <QtTest/QtTest>

class TestMyModule : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    void testSomething();
};

void TestMyModule::initTestCase() { }
void TestMyModule::cleanupTestCase() { }

void TestMyModule::testSomething()
{
    QCOMPARE(1 + 1, 2);
    QVERIFY(true);
}

QTEST_MAIN(TestMyModule)
#include "test_mymodule.moc"
```

3. Add to `tests/CMakeLists.txt`:

```cmake
add_executable(test_mymodule
    test_mymodule.cpp
    ${SRC_DIR}/mymodule.cpp
)
target_include_directories(test_mymodule PRIVATE ${SRC_DIR})
target_link_libraries(test_mymodule PRIVATE Qt6::Test Qt6::Core)
add_test(NAME MyModuleTest COMMAND test_mymodule)
```

## Continuous Integration

Tests can be integrated into CI pipelines:

```yaml
# Example GitHub Actions step
- name: Run Tests
  run: |
    cmake -B build -DRATS_SEARCH_BUILD_TESTS=ON
    cmake --build build
    cd build && ctest --output-on-failure
```

## Test Coverage

To generate test coverage reports (requires gcov/lcov):

```bash
cmake -DRATS_SEARCH_BUILD_TESTS=ON \
      -DCMAKE_CXX_FLAGS="--coverage" \
      -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

