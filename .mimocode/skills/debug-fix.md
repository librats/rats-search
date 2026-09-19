# Debug and Fix Skill

## Description
Debug issues, analyze errors, and fix bugs in the Rats Search codebase.

## Common Issue Categories

### 1. Build Errors

#### CMake Configuration Issues
```bash
# Check CMake version
cmake --version

# Clean and reconfigure
rm -rf build
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DRATS_SEARCH_BUILD_TESTS=ON
```

#### Qt6 Not Found
```bash
# Verify Qt6 installation
qmake6 --version

# Set Qt6 path if needed
export CMAKE_PREFIX_PATH=/path/to/qt6
```

#### Missing Submodules
```bash
# Initialize submodules
git submodule update --init --recursive

# Check submodule status
git submodule status
```

### 2. Runtime Errors

#### Database Issues
```bash
# Check database file permissions
ls -la ~/.local/share/RatsSearch/

# Reset database (backup first!)
mv ~/.local/share/RatsSearch/rats-search.db ~/.local/share/RatsSearch/rats-search.db.bak
```

#### Port Conflicts
```bash
# Check if ports are in use
netstat -tulpn | grep -E '4445|4446|8095'

# Kill process using port
lsof -i :8095
kill -9 <PID>
```

#### Manticore Search Issues
```bash
# Check if Manticore is running
ps aux | grep searchd

# Check Manticore logs
tail -f ~/.local/share/RatsSearch/manticore.log

# Restart Manticore
pkill searchd
./build/bin/searchd --config ~/.local/share/RatsSearch/manticore.conf
```

### 3. Memory Issues

#### AddressSanitizer Build
```bash
# Build with ASAN
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DRATS_ENABLE_ASAN=ON
cmake --build build --config Debug --parallel

# Run with ASAN
./build/bin/RatsSearch
```

#### ThreadSanitizer Build
```bash
# Build with TSAN (mutually exclusive with ASAN)
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DRATS_ENABLE_TSAN=ON
cmake --build build --config Debug --parallel
```

### 4. GUI Issues

#### Qt Platform Plugin Error
```bash
# Run with offscreen platform
QT_QPA_PLATFORM=offscreen ./build/bin/RatsSearch

# Check available platforms
ls /path/to/qt6/plugins/platforms/
```

#### Missing Qt Plugins
```bash
# Set plugin path
export QT_PLUGIN_PATH=/path/to/qt6/plugins

# Check required plugins
ldd ./build/bin/RatsSearch | grep Qt
```

## Debugging Techniques

### 1. Logging

#### Enable Debug Logging
```cpp
// In main.cpp or component
qDebug() << "Debug message";
qInfo() << "Info message";
qWarning() << "Warning message";
qCritical() << "Critical message";
```

#### Custom Log Handler
```cpp
// Already implemented in main.cpp
qInstallMessageHandler(customMessageHandler);
```

### 2. Qt Creator Debugger

1. Set breakpoint in code
2. Press F5 to start debugging
3. Use Variables panel to inspect values
4. Use Call Stack to trace execution

### 3. GDB调试

```bash
# Build with debug info
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --parallel

# Run with GDB
gdb ./build/bin/RatsSearch

# Common GDB commands
(gdb) break main
(gdb) run
(gdb) next
(gdb) step
(gdb) print variable
(gdb) backtrace
```

### 4. Valgrind

```bash
# Memory leak detection
valgrind --leak-check=full ./build/bin/RatsSearch

# Call graph analysis
valgrind --callgrind=./callgrind.out ./build/bin/RatsSearch
```

## Common Fixes

### 1. Signal/Slot Connection Issues

```cpp
// Wrong: Missing Q_OBJECT macro
class MyClass : public QObject {
    // Missing Q_OBJECT
};

// Correct
class MyClass : public QObject {
    Q_OBJECT
    // ...
};
```

### 2. Memory Leaks

```cpp
// Wrong: Raw pointer without parent
MyWidget *widget = new MyWidget();

// Correct: With parent
MyWidget *widget = new MyWidget(this);

// Or use smart pointer
auto widget = std::make_unique<MyWidget>();
```

### 3. Thread Safety

```cpp
// Wrong: Accessing UI from thread
void Worker::doWork() {
    label->setText("Working...");  // CRASH!
}

// Correct: Use signal/slot
void Worker::doWork() {
    emit workDone("Working...");
}

// In main thread
connect(worker, &Worker::workDone, label, &QLabel::setText);
```

### 4. Database Connection

```cpp
// Wrong: Multiple connections
QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
QSqlDatabase db2 = QSqlDatabase::addDatabase("QSQLITE");  // Overwrites!

// Correct: Named connections
QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "connection1");
QSqlDatabase db2 = QSqlDatabase::addDatabase("QSQLITE", "connection2");
```

## Performance Profiling

### Qt Performance
```bash
# Enable Qt performance logging
export QT_LOGGING_RULES="qt.performance=true"

# Use Qt Performance tools in Qt Creator
```

### System Profiling
```bash
# Linux perf
perf record ./build/bin/RatsSearch
perf report

# Callgrind
valgrind --tool=callgrind ./build/bin/RatsSearch
```

## References

- Qt6 Debugging: https://doc.qt.io/qt-6/debugging.html
- ASAN documentation: https://clang.llvm.org/docs/AddressSanitizer.html
- GDB tutorial: https://www.gnu.org/software/gdb/documentation/
- Project logs: `<data-dir>/rats-search.log`
