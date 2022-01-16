cmake_minimum_required(VERSION 3.16)
project(microtar)

add_library(microtar STATIC
        source/microtar.c
)

target_include_directories(microtar PUBLIC include)
