cmake_minimum_required(VERSION 3.16)
project(rats-search VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set Qt6 as required
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

# Find Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Gui 
    Widgets 
    Network
    Sql
    Concurrent
)

# Option to build with system librats or bundled version
option(RATS_SEARCH_USE_SYSTEM_LIBRATS "Use system-installed librats" OFF)
option(RATS_SEARCH_BUILD_TESTS "Build tests" OFF)

# Include librats
if(RATS_SEARCH_USE_SYSTEM_LIBRATS)
    find_package(librats REQUIRED)
else()
    # Add librats as subdirectory
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/librats/CMakeLists.txt")
        set(RATS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(RATS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(RATS_SEACH_FEATURES ON CACHE BOOL "" FORCE)
        add_subdirectory(src/librats)
    else()
        message(FATAL_ERROR "librats not found. Please initialize git submodules: git submodule update --init --recursive")
    endif()
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/searchengine.cpp
    src/torrentdatabase.cpp
    src/torrentspider.cpp
    src/p2pnetwork.cpp
    src/searchresultmodel.cpp
    src/torrentclient.cpp
    src/manticoremanager.cpp
    src/sphinxql.cpp
    src/searchapi.cpp
)

# Header files
set(HEADERS
    src/mainwindow.h
    src/searchengine.h
    src/torrentdatabase.h
    src/torrentspider.h
    src/p2pnetwork.h
    src/searchresultmodel.h
    src/torrentclient.h
    src/manticoremanager.h
    src/sphinxql.h
    src/searchapi.h
)

# Resources
set(RESOURCES
    resources/resources.qrc
)

# Create executable
add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    rats
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Installation
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# Qt deployment
if(WIN32)
    # Windows deployment
    get_target_property(QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
    
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-compiler-runtime "$<TARGET_FILE:${PROJECT_NAME}>"
        COMMENT "Running windeployqt..."
    )
elseif(APPLE)
    # macOS deployment
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${QT_BIN_DIR}")
    
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>"
        COMMENT "Running macdeployqt..."
    )
endif()

# Set output directories
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Application properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "RatsSearch"
    MACOSX_BUNDLE_GUI_IDENTIFIER com.ratssearch.app
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

message(STATUS "")
message(STATUS "=== Rats Search Configuration ===")
message(STATUS "Qt6 version: ${Qt6_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Using system librats: ${RATS_SEARCH_USE_SYSTEM_LIBRATS}")
message(STATUS "=================================")
message(STATUS "")

