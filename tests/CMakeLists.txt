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

    # Qt's plain test logger writes to OutputDebugString instead of stdout when
    # Windows reports no attached console -- which is the case on a CI runner and
    # for anything reading the test through a pipe, ctest included. A failing test
    # then appears as a bare "***Failed" with no FAIL line naming the function.
    # Forcing the console assumption keeps the log on stdout, where ctest sees it.
    if(WIN32)
        set_tests_properties(${TEST_NAME} PROPERTIES ENVIRONMENT "QT_ASSUME_STDERR_HAS_CONSOLE=1")
    endif()
endfunction()

# Unit tests (pure, no external process required)
add_rats_test(test_domain)
add_rats_test(test_query)
add_rats_test(test_infohash)
add_rats_test(test_row_id)
add_rats_test(test_update_service)
add_rats_test(test_config_store)
add_rats_test(test_search_history)
add_rats_test(test_filter_policy)
add_rats_test(test_manticore_maintenance)
add_rats_test(test_database_dump)
add_rats_test(test_database_snapshot)
add_rats_test(test_database_transfer_resume)
add_rats_test(test_voting_record)

# The single-instance guard lives in src/bootstrap (executable-only, not in
# ratscore), so its source is compiled straight into the test.
add_rats_test(test_single_instance ${CMAKE_SOURCE_DIR}/src/bootstrap/single_instance.cpp)

# The theme lives in src/ui (executable-only, not in ratscore) and reads its
# stylesheet template and token tables out of the Qt resources, so both the
# source and the qrc are compiled straight into this test.
add_rats_test(test_theme
    ${CMAKE_SOURCE_DIR}/src/ui/theme.cpp
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc)

# Content classifier needs the :/content/*.json Qt resources. Those live in the
# main resources.qrc (they are not compiled into ratscore), so compile the qrc
# straight into this test executable (CMAKE_AUTORCC is ON globally).
add_rats_test(test_content_classifier ${CMAKE_SOURCE_DIR}/resources/resources.qrc)

# Integration test: starts a real Manticore instance and drives the new
# data layer (Manticore + Database + TorrentRepository).
add_rats_test(test_manticore_queries)

# Copy libmysql.dll next to test executables so the QMYSQL driver can load.
#
# It has to be the libmysql the QMYSQL plugin was *built against*. Manticore
# ships its own copy in imports/ for its own tools (8.3 at the time of writing)
# while the prebuilt Qt driver comes with 8.4, and a DLL next to the executable
# wins the search over Qt's bin directory -- so copying Manticore's here made
# the plugin read MYSQL_FIELD at the wrong offsets. It then reports string
# columns with an integer type: `SELECT * FROM files` handed back `hash` as
# uint 0, every hash verification rejected the row as somebody else's, and file
# lists came back empty with no error anywhere. Measured on this repo: with
# Manticore's 8.3 next to the binary test_manticore_queries failed 5 runs out of
# 12, with the driver's 8.4 zero out of 45.
#
# So take Qt's copy and never substitute Manticore's: no library at all fails
# loudly ("can not load requested driver 'QMYSQL'"), the wrong one fails
# silently and intermittently.
if(WIN32)
    set(LIBMYSQL_CANDIDATES "")
    if(DEFINED QT6_INSTALL_PREFIX AND DEFINED QT6_INSTALL_BINS)
        list(APPEND LIBMYSQL_CANDIDATES "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/libmysql.dll")
    endif()
    if(TARGET Qt6::Core)
        get_target_property(QT_CORE_LOCATION Qt6::Core LOCATION)
        if(QT_CORE_LOCATION)
            get_filename_component(QT_BIN_DIR "${QT_CORE_LOCATION}" DIRECTORY)
            list(APPEND LIBMYSQL_CANDIDATES "${QT_BIN_DIR}/libmysql.dll")
        endif()
    endif()

    set(LIBMYSQL_PATH "")
    foreach(candidate IN LISTS LIBMYSQL_CANDIDATES)
        if(EXISTS "${candidate}")
            set(LIBMYSQL_PATH "${candidate}")
            break()
        endif()
    endforeach()

    if(LIBMYSQL_PATH)
        message(STATUS "Integration tests: libmysql.dll from ${LIBMYSQL_PATH}")
        add_custom_command(TARGET test_manticore_queries POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LIBMYSQL_PATH}"
                "$<TARGET_FILE_DIR:test_manticore_queries>"
            COMMENT "Copying libmysql.dll for integration tests..."
        )
    else()
        message(STATUS "Integration tests: no libmysql.dll next to Qt. Copy the one that ships with the "
                       "QMYSQL driver next to the test binary by hand -- NOT the one in imports/, which "
                       "belongs to Manticore and makes the plugin misread result metadata.")
    endif()
endif()

# Custom target to run all tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_domain test_query test_infohash test_update_service
            test_config_store test_filter_policy test_content_classifier test_manticore_queries
            test_manticore_maintenance
            test_database_dump
            test_voting_record
            test_single_instance
            test_theme
    COMMENT "Running all tests..."
)

message(STATUS "")
message(STATUS "=== Rats Search Tests ===")
message(STATUS "Tests enabled: ON")
message(STATUS "Qt6 Test version: ${Qt6Test_VERSION}")
message(STATUS "=========================")
message(STATUS "")
