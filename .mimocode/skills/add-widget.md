# Add Widget Skill

## Description
Add a new Qt widget to the Rats Search project.

## Prerequisites
- Qt6 development environment
- CMake build system configured

## Steps

### 1. Create Header File
Create `src/newwidget.h` with the widget class declaration:

```cpp
#pragma once

#include <QWidget>

class NewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NewWidget(QWidget *parent = nullptr);
    ~NewWidget();

private:
    // UI members
};
```

### 2. Create Source File
Create `src/newwidget.cpp` with the implementation:

```cpp
#include "newwidget.h"

NewWidget::NewWidget(QWidget *parent)
    : QWidget(parent)
{
    // Setup UI
}

NewWidget::~NewWidget()
{
}
```

### 3. Update CMakeLists.txt
Add the new files to the SOURCES and HEADERS lists in `CMakeLists.txt`:

```cmake
set(SOURCES
    # ... existing sources ...
    src/newwidget.cpp
)

set(HEADERS
    # ... existing headers ...
    src/newwidget.h
)
```

### 4. Follow Existing Patterns
- Look at `toptorrentswidget.cpp` for a simple widget example
- Look at `feedwidget.cpp` for a more complex widget
- Use Qt's signal/slot mechanism for communication
- Use `#pragma once` for header guards

### 5. Build and Test
```bash
cmake --build build --config Debug --parallel
cd build && ctest --output-on-failure
```

## Example Templates

### Simple Widget
```cpp
// newwidget.h
#pragma once

#include <QWidget>

class QLabel;
class QVBoxLayout;

class NewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NewWidget(QWidget *parent = nullptr);

private:
    QLabel *m_titleLabel;
    QVBoxLayout *m_layout;
};
```

```cpp
// newwidget.cpp
#include "newwidget.h"
#include <QLabel>
#include <QVBoxLayout>

NewWidget::NewWidget(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    
    m_titleLabel = new QLabel(tr("New Widget"), this);
    m_layout->addWidget(m_titleLabel);
    
    setLayout(m_layout);
}
```

## References
- Qt6 Documentation: https://doc.qt.io/qt-6/
- Project patterns: `src/toptorrentswidget.cpp`
