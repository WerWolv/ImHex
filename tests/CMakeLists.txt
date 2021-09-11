cmake_minimum_required(VERSION 3.16)

project(tests)


# Add new tests here #
set(AVAILABLE_TESTS
        Placement
)



add_executable(tests source/main.cpp )
target_include_directories(tests PRIVATE include)
target_link_libraries(tests libimhex)
set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_custom_command(TARGET tests
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/test_data" ${CMAKE_BINARY_DIR})

foreach (test IN LISTS AVAILABLE_TESTS)
    add_test(NAME "${test}" COMMAND tests "${test}" WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach ()