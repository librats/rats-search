# Rats Search Test Suite
cmake_minimum_required(VERSION 3.16)

# Find Qt6 Test
find_package(Qt6 REQUIRED COMPONENTS Test Core Gui Widgets Sql Concurrent Network)

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
    ${SRC_DIR}/contentdetector.cpp
    ${SRC_DIR}/api/updatemanager.cpp
    ${SRC_DIR}/api/configmanager.cpp
)
target_include_directories(rats_testable PUBLIC ${SRC_DIR} ${CMAKE_BINARY_DIR})
target_link_libraries(rats_testable PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Network
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
add_rats_test(test_updatemanager)
add_rats_test(test_contentdetector)

# Integration test: starts real Manticore and verifies all query patterns
add_rats_test(test_manticore_queries)

# Copy libmysql.dll next to test executables so the QMYSQL driver can load
if(WIN32)
    set(LIBMYSQL_PATH "${CMAKE_SOURCE_DIR}/imports/win/x64/libmysql.dll")
    if(EXISTS "${LIBMYSQL_PATH}")
        add_custom_command(TARGET test_manticore_queries POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LIBMYSQL_PATH}"
                "$<TARGET_FILE_DIR:test_manticore_queries>"
            COMMENT "Copying libmysql.dll for integration tests..."
        )
    endif()
endif()

# Custom target to run all tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_torrentinfo test_searchresultmodel test_sphinxql test_utils test_updatemanager test_contentdetector test_manticore_queries
    COMMENT "Running all tests..."
)

message(STATUS "")
message(STATUS "=== Rats Search Tests ===")
message(STATUS "Tests enabled: ON")
message(STATUS "Qt6 Test version: ${Qt6Test_VERSION}")
message(STATUS "=========================")
message(STATUS "")
