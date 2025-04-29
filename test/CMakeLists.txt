cmake_minimum_required(VERSION 3.10)

include(GoogleTest)

file(GLOB_RECURSE BABYDB_TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*/*test.cpp")

# #########################################
# "make check-tests"
# #########################################
add_custom_target(build-tests COMMAND ${CMAKE_CTEST_COMMAND} --show-only)
add_custom_target(check-tests COMMAND ${CMAKE_CTEST_COMMAND})
add_custom_target(check-tests-details COMMAND ${CMAKE_CTEST_COMMAND} --verbose)

# #########################################
# "make XYZ_test"
# #########################################
foreach (babydb_test_source ${BABYDB_TEST_SOURCES})
    # Create a human readable name.
    get_filename_component(babydb_test_filename ${babydb_test_source} NAME)
    string(REPLACE ".cpp" "" babydb_test_name ${babydb_test_filename})

    # Add the test target separately and as part of "make check-tests".
    add_executable(${babydb_test_name} EXCLUDE_FROM_ALL ${babydb_test_source})
    add_dependencies(build-tests ${babydb_test_name})
    add_dependencies(check-tests ${babydb_test_name})
    add_dependencies(check-tests-details ${babydb_test_name})

    gtest_discover_tests(${babydb_test_name}
            EXTRA_ARGS
            --gtest_output=xml:${CMAKE_BINARY_DIR}/test/${babydb_test_name}.xml
            --gtest_catch_exceptions=0
            DISCOVERY_TIMEOUT 120
            PROPERTIES
            TIMEOUT 120
            )

    target_link_libraries(${babydb_test_name} babydb gtest gmock_main)

    # Set test target properties and dependencies.
    set_target_properties(${babydb_test_name}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
            COMMAND ${babydb_test_name}
            )
endforeach ()
