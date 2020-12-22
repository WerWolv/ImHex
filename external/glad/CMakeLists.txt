cmake_minimum_required(VERSION 3.16)
project(glad)

set(CMAKE_CXX_STANDARD 17)

add_library(glad
    source/glad.c
)

target_include_directories(glad PUBLIC include)
target_link_libraries(glad PRIVATE dl)