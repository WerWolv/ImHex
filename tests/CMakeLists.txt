cmake_minimum_required(VERSION 3.16)

project(unit_tests)


# Add new tests here #
set(AVAILABLE_TESTS
        Placement
        Structs
        Unions
        Enums
        Literals
        Padding
        SucceedingAssert
        FailingAssert
        Bitfields
        Math
)



add_executable(unit_tests source/main.cpp source/tests.cpp)
target_include_directories(unit_tests PRIVATE include)
target_link_libraries(unit_tests libimhex)

set_target_properties(unit_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_custom_command(TARGET unit_tests
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/test_data" ${CMAKE_BINARY_DIR})

foreach (test IN LISTS AVAILABLE_TESTS)
    add_test(NAME "${test}" COMMAND unit_tests "${test}" WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach ()