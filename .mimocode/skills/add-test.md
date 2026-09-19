# Add Test Skill

## Description
Add a new unit test to the Rats Search test suite.

## Prerequisites
- Qt6 Test framework
- Understanding of the component being tested

## Test Structure

Tests are in `tests/` and use Qt Test framework:
- `tests/test_torrentinfo.cpp` - Torrent data structures
- `tests/test_searchresultmodel.cpp` - Search model logic
- `tests/test_sphinxql.cpp` - Query builder correctness
- `tests/test_utils.cpp` - Utility functions
- `tests/test_contentdetector.cpp` - Content type detection
- `tests/test_updatemanager.cpp` - Update check logic
- `tests/test_manticore_queries.cpp` - Integration tests

## Steps

### 1. Create Test File
Create `tests/test_newfeature.cpp`:

```cpp
#include <QtTest>
#include "component.h"

class TestNewFeature : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    void testBasicFunction();
    void testEdgeCase();
    void testErrorHandling();

private:
    Component *m_component;
};

void TestNewFeature::initTestCase()
{
    // Called once before all tests
}

void TestNewFeature::cleanupTestCase()
{
    // Called once after all tests
}

void TestNewFeature::init()
{
    // Called before each test
    m_component = new Component(this);
}

void TestNewFeature::cleanup()
{
    // Called after each test
    delete m_component;
}

void TestNewFeature::testBasicFunction()
{
    QVERIFY(m_component->isValid());
    QCOMPARE(m_component->value(), 42);
}

void TestNewFeature::testEdgeCase()
{
    m_component->setValue(0);
    QCOMPARE(m_component->value(), 0);
    
    m_component->setValue(-1);
    QCOMPARE(m_component->value(), -1);
}

void TestNewFeature::testErrorHandling()
{
    QVERIFY_EXCEPTION_THROWN(m_component->invalidOperation(), std::runtime_error);
}

QTEST_MAIN(TestNewFeature)
#include "test_newfeature.moc"
```

### 2. Update CMakeLists.txt
Add the test to `tests/CMakeLists.txt`:

```cmake
# Add to the list of tests
add_rats_test(test_newfeature)

# Update the check target's DEPENDS list
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_torrentinfo test_searchresultmodel test_sphinxql test_utils test_updatemanager test_contentdetector test_manticore_queries test_newfeature
    COMMENT "Running all tests..."
)
```

### 3. Build and Run
```bash
# Build
cmake --build build --config Debug --parallel

# Run all tests
cd build && ctest --output-on-failure --parallel

# Run specific test
cd build && ./tests/test_newfeature
```

## Test Categories

### Unit Tests
- Test individual functions/methods
- Fast execution
- No external dependencies

### Integration Tests
- Test component interactions
- May require running services (e.g., Manticore)
- Example: `test_manticore_queries.cpp`

### GUI Tests
- Test widget behavior
- Require `QT_QPA_PLATFORM=offscreen` on headless systems

## Qt Test Assertions

```cpp
// Boolean check
QVERIFY(condition);

// Equality check
QCOMPARE(actual, expected);

// Floating point comparison
QCOMPARE(qFuzzyCompare(actual, expected), true);

// String comparison
QCOMPARE(actual QString, expected QString);

// Exception testing
QVERIFY_EXCEPTION_THROWN(expression, exceptionType);

// Null pointer check
QVERIFY(ptr != nullptr);
QVERIFY(ptr == nullptr);
```

## Test Data

### Using QTest::addColumn
```cpp
void TestComponent::test_data()
{
    QTest::addColumn<int>("input");
    QTest::addColumn<int>("expected");
    
    QTest::newRow("positive") << 5 << 10;
    QTest::newRow("negative") << -5 << -10;
    QTest::newRow("zero") << 0 << 0;
}

void TestComponent::test()
{
    QFETCH(int, input);
    QFETCH(int, expected);
    
    QCOMPARE(component.process(input), expected);
}
```

## Best Practices

1. **Test isolation**: Each test should be independent
2. **Clear naming**: Use descriptive test names
3. **Setup/teardown**: Use init/cleanup for resources
4. **Edge cases**: Test boundary conditions
5. **Error handling**: Test error paths
6. **Documentation**: Comment complex test logic

## References
- Qt Test documentation: https://doc.qt.io/qt-6/qtest.html
- Existing tests: `tests/test_sphinxql.cpp`
- Test CMake: `tests/CMakeLists.txt`
