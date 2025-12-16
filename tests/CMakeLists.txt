# Rats Search Test Suite
cmake_minimum_required(VERSION 3.16)

# Find Qt6 Test
find_package(Qt6 REQUIRED COMPONENTS Test Core Gui Widgets Sql Concurrent)

# Enable Qt testing
enable_testing()

# Include source directory
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# Common test sources (from main project)
set(TESTABLE_SOURCES
    ${SRC_DIR}/searchresultmodel.cpp
    ${SRC_DIR}/torrentdatabase.cpp
    ${SRC_DIR}/sphinxql.cpp
    ${SRC_DIR}/manticoremanager.cpp
)

set(TESTABLE_HEADERS
    ${SRC_DIR}/searchresultmodel.h
    ${SRC_DIR}/torrentdatabase.h
    ${SRC_DIR}/sphinxql.h
    ${SRC_DIR}/manticoremanager.h
)

# Test: TorrentInfo
add_executable(test_torrentinfo
    test_torrentinfo.cpp
    ${SRC_DIR}/torrentdatabase.cpp
    ${SRC_DIR}/sphinxql.cpp
    ${SRC_DIR}/manticoremanager.cpp
)
target_include_directories(test_torrentinfo PRIVATE ${SRC_DIR})
target_link_libraries(test_torrentinfo PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Sql
    Qt6::Concurrent
)
add_test(NAME TorrentInfoTest COMMAND test_torrentinfo)

# Test: SearchResultModel
add_executable(test_searchresultmodel
    test_searchresultmodel.cpp
    ${SRC_DIR}/searchresultmodel.cpp
    ${SRC_DIR}/torrentdatabase.cpp
    ${SRC_DIR}/sphinxql.cpp
    ${SRC_DIR}/manticoremanager.cpp
)
target_include_directories(test_searchresultmodel PRIVATE ${SRC_DIR})
target_link_libraries(test_searchresultmodel PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Concurrent
)
add_test(NAME SearchResultModelTest COMMAND test_searchresultmodel)

# Test: SphinxQL (unit tests for SQL building functions)
add_executable(test_sphinxql
    test_sphinxql.cpp
    ${SRC_DIR}/sphinxql.cpp
    ${SRC_DIR}/manticoremanager.cpp
)
target_include_directories(test_sphinxql PRIVATE ${SRC_DIR})
target_link_libraries(test_sphinxql PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Sql
    Qt6::Concurrent
)
add_test(NAME SphinxQLTest COMMAND test_sphinxql)

# Test: Utils (utility functions and edge cases)
add_executable(test_utils
    test_utils.cpp
    ${SRC_DIR}/torrentdatabase.cpp
    ${SRC_DIR}/sphinxql.cpp
    ${SRC_DIR}/manticoremanager.cpp
)
target_include_directories(test_utils PRIVATE ${SRC_DIR})
target_link_libraries(test_utils PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Sql
    Qt6::Concurrent
)
add_test(NAME UtilsTest COMMAND test_utils)

# Custom target to run all tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_torrentinfo test_searchresultmodel test_sphinxql test_utils
    COMMENT "Running all tests..."
)

message(STATUS "")
message(STATUS "=== Rats Search Tests ===")
message(STATUS "Tests enabled: ON")
message(STATUS "Qt6 Test version: ${Qt6Test_VERSION}")
message(STATUS "=========================")
message(STATUS "")

