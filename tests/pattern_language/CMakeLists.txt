cmake_minimum_required(VERSION 3.16)

project(pattern_language_tests)


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
        RValues
        Namespaces
        ExtraSemicolon
        Pointers
)



add_executable(pattern_language_tests source/main.cpp source/tests.cpp)
target_include_directories(pattern_language_tests PRIVATE include)
target_link_libraries(pattern_language_tests libimhex)

set_target_properties(pattern_language_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_custom_command(TARGET pattern_language_tests
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/test_data" ${CMAKE_BINARY_DIR})

foreach (test IN LISTS AVAILABLE_TESTS)
    add_test(NAME "PatternLanguage/${test}" COMMAND pattern_language_tests "${test}" WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach ()