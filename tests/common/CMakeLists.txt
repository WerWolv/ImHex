cmake_minimum_required(VERSION 3.16)

project(tests_common)

add_library(tests_common STATIC
        source/main.cpp
)
target_include_directories(tests_common PUBLIC include)
target_link_libraries(tests_common PUBLIC libimhex ${FMT_LIBRARIES})