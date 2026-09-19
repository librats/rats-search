# Code Review Skill

## Description
Review code changes and ensure quality, consistency, and adherence to project conventions.

## Review Checklist

### 1. Code Style

#### C++ Style
- [ ] Uses C++17 features appropriately
- [ ] Follows camelCase naming for variables/functions
- [ ] Follows PascalCase naming for classes
- [ ] Uses `#pragma once` for header guards
- [ ] Includes order: Qt → stdlib → project headers
- [ ] No unnecessary comments (unless requested)

#### Qt Patterns
- [ ] Uses Qt's signal/slot mechanism
- [ ] Uses `QObject::connect()` with proper context
- [ ] Uses `Q_PROPERTY` for exposed properties
- [ ] Uses `QTimer` for periodic tasks
- [ ] Uses smart pointers (`std::unique_ptr`/`std::shared_ptr`)

### 2. Memory Management

- [ ] No raw `new` without parent or smart pointer
- [ ] Proper cleanup in destructors
- [ ] No circular references
- [ ] Thread-safe object access

### 3. Error Handling

- [ ] Validates external input
- [ ] Handles error states gracefully
- [ ] Uses appropriate error reporting
- [ ] No silent failures

### 4. Performance

- [ ] No unnecessary copies
- [ ] Efficient data structures
- [ ] Proper use of Qt containers
- [ ] No memory leaks

### 5. Testing

- [ ] New code has corresponding tests
- [ ] Tests cover edge cases
- [ ] Tests are isolated
- [ ] Build succeeds with tests enabled

## Review Process

### Step 1: Understand the Change

1. Read the commit message and PR description
2. Understand the purpose of the change
3. Identify affected components

### Step 2: Code Analysis

#### Static Analysis
```bash
# Run clang-tidy
clang-tidy src/*.cpp src/api/*.cpp -- -I src -I src/api

# Run clang-format check
clang-format --dry-run src/*.cpp src/*.h
```

#### Build Verification
```bash
# Build with tests
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DRATS_SEARCH_BUILD_TESTS=ON
cmake --build build --config Debug --parallel

# Run tests
cd build && ctest --output-on-failure --parallel
```

### Step 3: Detailed Review

#### File-by-File Review
1. Check header files for:
   - Proper include guards
   - Necessary includes
   - Class design
   - API consistency

2. Check source files for:
   - Implementation correctness
   - Memory management
   - Error handling
   - Performance

#### Specific Patterns to Check

```cpp
// Signal/slot connections
connect(sender, &Sender::signal, receiver, &Receiver::slot);

// Smart pointers
auto ptr = std::make_unique MyClass();

// Qt containers
QVector<int> vec;
QMap<QString, int> map;

// Thread safety
QMutexLocker lock(&mutex);
```

### Step 4: Testing

1. Run existing tests
2. Verify new tests pass
3. Check edge cases
4. Test on different platforms (if possible)

### Step 5: Documentation

1. Check if documentation needs updates
2. Verify API documentation
3. Update CHANGELOG if needed

## Common Issues to Watch For

### 1. Memory Issues

```cpp
// Bad: Raw pointer without parent
MyWidget *w = new MyWidget();

// Good: With parent
MyWidget *w = new MyWidget(this);

// Good: Smart pointer
auto w = std::make_unique<MyWidget>();
```

### 2. Thread Safety

```cpp
// Bad: UI access from thread
void Worker::doWork() {
    label->setText("Done");  // CRASH!
}

// Good: Signal/slot
void Worker::doWork() {
    emit workDone("Done");
}
```

### 3. Resource Leaks

```cpp
// Bad: No cleanup
void func() {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    // db not closed
}

// Good: Proper cleanup
void func() {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    // Use db
    db.close();
}
```

### 4. Error Handling

```cpp
// Bad: Silent failure
void func() {
    file.open(QIODevice::ReadOnly);
    // No error check
}

// Good: Error handling
void func() {
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file:" << file.errorString();
        return;
    }
}
```

## Review Comments

### Positive Feedback
- "Nice use of Qt's signal/slot mechanism"
- "Good error handling here"
- "Clean and efficient implementation"

### Constructive Criticism
- "Consider using smart pointers here to avoid potential leaks"
- "This could be simplified using Qt's container algorithms"
- "Add error handling for the file operation"

### Questions for Clarification
- "What happens if the database connection fails?"
- "Is this thread-safe when called from multiple threads?"
- "Should we add a test for this edge case?"

## References

- C++ Core Guidelines: https://isocpp.github.io/CppCoreGuidelines/
- Qt Best Practices: https://doc.qt.io/qt-6/best-practices.html
- Project conventions: `AGENTS.md`
