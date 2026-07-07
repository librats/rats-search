# Rats Search Test Suite
cmake_minimum_required(VERSION 3.16)

# Find Qt6 Test (other Qt modules come transitively from ratscore)
find_package(Qt6 REQUIRED COMPONENTS Test)

# Enable Qt testing
enable_testing()

# Helper function to add a test with minimal boilerplate.
#
# Tests link directly against ratscore (the new layered core library defined in
# the top-level CMakeLists). ratscore carries every rewritten class plus the
# src/ include directory, so no separate "testable" library is needed.
function(add_rats_test TEST_NAME)
    add_executable(${TEST_NAME} ${TEST_NAME}.cpp ${ARGN})
    target_link_libraries(${TEST_NAME} PRIVATE Qt6::Test ratscore)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# Unit tests (pure, no external process required)
add_rats_test(test_domain)
add_rats_test(test_query)
add_rats_test(test_infohash)
add_rats_test(test_update_service)

# Content classifier needs the :/content/*.json Qt resources. Those live in the
# main resources.qrc (they are not compiled into ratscore), so compile the qrc
# straight into this test executable (CMAKE_AUTORCC is ON globally).
add_rats_test(test_content_classifier ${CMAKE_SOURCE_DIR}/resources/resources.qrc)

# Integration test: starts a real Manticore instance and drives the new
# data layer (Manticore + Database + TorrentRepository).
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
    DEPENDS test_domain test_query test_infohash test_update_service
            test_content_classifier test_manticore_queries
    COMMENT "Running all tests..."
)

message(STATUS "")
message(STATUS "=== Rats Search Tests ===")
message(STATUS "Tests enabled: ON")
message(STATUS "Qt6 Test version: ${Qt6Test_VERSION}")
message(STATUS "=========================")
message(STATUS "")
