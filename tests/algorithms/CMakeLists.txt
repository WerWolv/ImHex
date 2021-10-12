cmake_minimum_required(VERSION 3.16)

project(algorithms_tests)

find_package(Catch2 REQUIRED)

add_executable(algorithms_tests source/hash.cpp)
target_include_directories(algorithms_tests PRIVATE include)
target_link_libraries(algorithms_tests libimhex Catch2::Catch2)

set_target_properties(algorithms_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(CTest)
include(Catch)
catch_discover_tests(algorithms_tests TEST_PREFIX "Algorithms/")