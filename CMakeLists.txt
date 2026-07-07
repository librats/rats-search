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

file(GLOB_RECURSE RATS_CORE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/domain/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/data/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/peer/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rest/*.cpp
)
file(GLOB_RECURSE RATS_CORE_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/domain/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/data/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/peer/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rest/*.h
)

add_library(ratscore STATIC ${RATS_CORE_SOURCES} ${RATS_CORE_HEADERS})

# Mirror the on-disk layer layout in IDEs (VS Solution Explorer / Xcode groups).
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files"
    FILES ${RATS_CORE_SOURCES} ${RATS_CORE_HEADERS})

target_include_directories(ratscore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Core deliberately does NOT link Qt6::Widgets — the domain/data/net/service
# layers must stay free of any GUI dependency. Widgets live only in the ui layer.
target_link_libraries(ratscore PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    Qt6::WebSockets
    rats
)

# The GUI / executable layer — everything the app binary compiles beyond the
# ratscore library: pre-Application bootstrap helpers (src/bootstrap) and the Qt
# Widgets presentation layer (src/ui). Globbed with CONFIGURE_DEPENDS just like
# ratscore, so adding a file re-runs CMake automatically. main.cpp — the sole
# file at the src/ root — is listed explicitly on the executable below.
file(GLOB_RECURSE RATS_GUI_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bootstrap/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp
)
file(GLOB_RECURSE RATS_GUI_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bootstrap/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.h
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
    src/main.cpp
    ${RATS_GUI_SOURCES}
    ${RATS_GUI_HEADERS}
    ${RESOURCES}
    ${TRANSLATION_QRC}
    ${QM_FILES}
    ${WIN_RC_FILE}
)

# Mirror the on-disk layout in IDEs for the executable's own sources.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files"
    FILES src/main.cpp ${RATS_GUI_SOURCES} ${RATS_GUI_HEADERS})

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
    ratscore
)

# Include directories. src/ui is on the path so GUI files can include each other
# by bare name; ratscore headers are reached via the src/ prefix (e.g.
# "app/application.h", "domain/torrent.h").
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Installation
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# Qt deployment
get_target_property(QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
    
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-compiler-runtime "$<TARGET_FILE:${PROJECT_NAME}>"
        COMMENT "Running windeployqt..."
    )
elseif(APPLE)
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

