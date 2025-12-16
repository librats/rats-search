# Rats Search Test Suite
cmake_minimum_required(VERSION 3.16)

# Find Qt6 Test
find_package(Qt6 REQUIRED COMPONENTS Test Core Gui Widgets Sql Concurrent)

# Enable Qt testing
enable_testing()

# Include source directory
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# Create a static library with testable sources (avoids recompiling for each test)
add_library(rats_testable STATIC
    ${SRC_DIR}/searchresultmodel.cpp
    ${SRC_DIR}/torrentdatabase.cpp
    ${SRC_DIR}/sphinxql.cpp
    ${SRC_DIR}/manticoremanager.cpp
)
target_include_directories(rats_testable PUBLIC ${SRC_DIR})
target_link_libraries(rats_testable PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Concurrent
)

# Helper function to add a test with minimal boilerplate
function(add_rats_test TEST_NAME)
    add_executable(${TEST_NAME} ${TEST_NAME}.cpp)
    target_link_libraries(${TEST_NAME} PRIVATE Qt6::Test rats_testable)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# Define all tests - just one line each!
add_rats_test(test_torrentinfo)
add_rats_test(test_searchresultmodel)
add_rats_test(test_sphinxql)
add_rats_test(test_utils)

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
