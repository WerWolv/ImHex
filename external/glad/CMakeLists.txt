cmake_minimum_required(VERSION 3.16)
project(glad)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

add_library(glad STATIC
    source/glad.c
)

target_include_directories(glad PUBLIC include)

target_link_libraries(glad PRIVATE dl)