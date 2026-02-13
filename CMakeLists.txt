cmake_minimum_required(VERSION 3.16)

# Get version from git
find_package(Git QUIET)
if(GIT_FOUND)
    # Get the current git tag/commit
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    # Extract version from git tag (format: v1.2.3 or 1.2.3)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    # Get commit count (revision number)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_REVISION_COUNT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    # Parse version components
    if(GIT_TAG MATCHES "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)")
        set(VERSION_MAJOR ${CMAKE_MATCH_1})
        set(VERSION_MINOR ${CMAKE_MATCH_2})
        set(VERSION_PATCH ${CMAKE_MATCH_3})
    else()
        # Fallback to default version
        set(VERSION_MAJOR 2)
        set(VERSION_MINOR 0)
        set(VERSION_PATCH 0)
    endif()
else()
    # Fallback when git is not available
    set(VERSION_MAJOR 2)
    set(VERSION_MINOR 0)
    set(VERSION_PATCH 0)
    set(GIT_DESCRIBE "unknown")
    set(GIT_REVISION_COUNT 0)
endif()

# Set build number to revision count (commit count)
if(GIT_REVISION_COUNT)
    set(VERSION_BUILD ${GIT_REVISION_COUNT})
else()
    set(VERSION_BUILD 0)
endif()
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_BUILD}")

project(rats-search VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH} LANGUAGES CXX)

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
    WebSockets
    LinguistTools
)

# Configure version.h from template
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    @ONLY
)

# Option to build with system librats or bundled version
option(RATS_SEARCH_USE_SYSTEM_LIBRATS "Use system-installed librats" OFF)
option(RATS_SEARCH_BUILD_TESTS "Build tests" OFF)
option(RATS_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(RATS_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

# Sanitizer configuration - ASAN and TSAN are mutually exclusive
if(RATS_ENABLE_ASAN AND RATS_ENABLE_TSAN)
    message(FATAL_ERROR "AddressSanitizer and ThreadSanitizer cannot be enabled simultaneously")
endif()

# AddressSanitizer configuration
if(RATS_ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(ASAN_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ASAN_FLAGS}")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ASAN_FLAGS}")
        
        message(STATUS "AddressSanitizer enabled (GCC/Clang)")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=address")
        
        message(STATUS "AddressSanitizer enabled (MSVC)")
    else()
        message(WARNING "AddressSanitizer is not supported by this compiler")
    endif()
endif()

# ThreadSanitizer configuration
if(RATS_ENABLE_TSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(TSAN_FLAGS "-fsanitize=thread -fno-omit-frame-pointer -g")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TSAN_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TSAN_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${TSAN_FLAGS}")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${TSAN_FLAGS}")
        
        message(STATUS "ThreadSanitizer enabled (GCC/Clang)")
    else()
        message(WARNING "ThreadSanitizer is only supported by GCC/Clang compilers")
    endif()
endif()

# Include librats
if(RATS_SEARCH_USE_SYSTEM_LIBRATS)
    find_package(librats REQUIRED)
else()
    # Add librats as subdirectory
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/librats/CMakeLists.txt")
        set(RATS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(RATS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(RATS_SEARCH_FEATURES ON CACHE BOOL "" FORCE)
        set(RATS_STORAGE ON CACHE BOOL "" FORCE)
        # Forward sanitizer options to librats
        set(RATS_ENABLE_ASAN ${RATS_ENABLE_ASAN} CACHE BOOL "" FORCE)
        set(RATS_ENABLE_TSAN ${RATS_ENABLE_TSAN} CACHE BOOL "" FORCE)
        add_subdirectory(src/librats)
    else()
        message(FATAL_ERROR "librats not found. Please initialize git submodules: git submodule update --init --recursive")
    endif()
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/legacymigration.cpp
    src/mainwindow.cpp
    src/settingsdialog.cpp
    src/torrentdatabase.cpp
    src/torrentspider.cpp
    src/p2pnetwork.cpp
    src/searchresultmodel.cpp
    src/torrentclient.cpp
    src/manticoremanager.cpp
    src/sphinxql.cpp
    src/torrentitemdelegate.cpp
    src/torrentdetailspanel.cpp
    src/torrentfileswidget.cpp
    src/contentdetector.cpp
    # New tab widgets (migrated from legacy)
    src/toptorrentswidget.cpp
    src/feedwidget.cpp
    src/downloadswidget.cpp
    src/activitywidget.cpp
    # API layer (replaces old searchapi.cpp and searchengine.cpp)
    src/api/ratsapi.cpp
    src/api/configmanager.cpp
    src/api/feedmanager.cpp
    src/api/apiserver.cpp
    src/api/updatemanager.cpp
    src/api/translationmanager.cpp
    src/api/p2pstoremanager.cpp
    src/api/trackerwrapper.cpp
    src/api/trackerinfoscraper.cpp
    src/api/migrationmanager.cpp
)

# Header files
set(HEADERS
    src/mainwindow.h
    src/utils.h
    src/settingsdialog.h
    src/torrentdatabase.h
    src/torrentspider.h
    src/p2pnetwork.h
    src/searchresultmodel.h
    src/torrentclient.h
    src/manticoremanager.h
    src/sphinxql.h
    src/torrentitemdelegate.h
    src/torrentdetailspanel.h
    src/torrentfileswidget.h
    src/contentdetector.h
    # New tab widgets (migrated from legacy)
    src/toptorrentswidget.h
    src/feedwidget.h
    src/downloadswidget.h
    src/activitywidget.h
    # API layer (replaces old searchapi.h and searchengine.h)
    src/api/ratsapi.h
    src/api/configmanager.h
    src/api/feedmanager.h
    src/api/apiserver.h
    src/api/updatemanager.h
    src/api/translationmanager.h
    src/api/p2pstoremanager.h
    src/api/trackerwrapper.h
    src/api/trackerinfoscraper.h
    src/api/migrationmanager.h
)

# Resources
set(RESOURCES
    resources/resources.qrc
)

# Translation files
set(TS_FILES
    translations/ratssearch_ru.ts
    translations/ratssearch_de.ts
    translations/ratssearch_es.ts
    translations/ratssearch_fr.ts
)

# Generate .qm files from .ts files
qt6_add_translation(QM_FILES ${TS_FILES})

# Add generated .qm files to resources via custom qrc
set(TRANSLATION_QRC "${CMAKE_BINARY_DIR}/translations.qrc")
file(WRITE ${TRANSLATION_QRC} "<RCC>\n    <qresource prefix=\"/translations\">\n")
foreach(QM_FILE ${QM_FILES})
    get_filename_component(QM_NAME ${QM_FILE} NAME)
    file(APPEND ${TRANSLATION_QRC} "        <file alias=\"${QM_NAME}\">${QM_FILE}</file>\n")
endforeach()
file(APPEND ${TRANSLATION_QRC} "    </qresource>\n</RCC>\n")

# Windows application icon
if(WIN32)
    set(WIN_RC_FILE resources/app.rc)
endif()

# Create executable
add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
    ${TRANSLATION_QRC}
    ${QM_FILES}
    ${WIN_RC_FILE}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    Qt6::WebSockets
    rats
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api
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

# Enable PDB generation for Windows (even in Release builds)
if(MSVC)
    # Generate PDB files for debugging
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/Zi>
        $<$<CONFIG:RelWithDebInfo>:/Zi>
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/DEBUG>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
        $<$<CONFIG:Release>:/INCREMENTAL:NO>
        $<$<CONFIG:RelWithDebInfo>:/DEBUG>
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

# Application properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "RatsSearch"
    MACOSX_BUNDLE_GUI_IDENTIFIER com.ratssearch.app
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Add tests if enabled
if(RATS_SEARCH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "")
message(STATUS "=== Rats Search Configuration ===")
message(STATUS "Version: ${VERSION_STRING}")
message(STATUS "Git: ${GIT_DESCRIBE}")
message(STATUS "Qt6 version: ${Qt6_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Using system librats: ${RATS_SEARCH_USE_SYSTEM_LIBRATS}")
message(STATUS "Build tests: ${RATS_SEARCH_BUILD_TESTS}")
message(STATUS "AddressSanitizer: ${RATS_ENABLE_ASAN}")
message(STATUS "ThreadSanitizer: ${RATS_ENABLE_TSAN}")
message(STATUS "=================================")
message(STATUS "")

