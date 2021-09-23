cmake_minimum_required(VERSION 3.16)
project(microtar)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

add_library(microtar STATIC
        source/microtar.c
)

target_include_directories(microtar PUBLIC include)
