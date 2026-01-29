cmake_minimum_required(VERSION 3.16)

project(cli_test)
set(TEST_CATEGORY CLI)

set(AVAILABLE_TESTS
    ReadOnlyFlagSetsMode
)

add_executable(${PROJECT_NAME}
        source/test_cli_readonly.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} PRIVATE libimhex tests_common ${FMT_LIBRARIES})

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

foreach (test IN LISTS AVAILABLE_TESTS)
    add_test(NAME "${TEST_CATEGORY}/${test}" COMMAND ${PROJECT_NAME} "${test}" WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach ()

add_dependencies(unit_tests ${PROJECT_NAME})


