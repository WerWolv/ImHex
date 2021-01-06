cmake_minimum_required(VERSION 3.16)
project(glad)

set(CMAKE_CXX_STANDARD 17)

if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc -static")
endif()

add_library(glad
    source/glad.c
)

target_include_directories(glad PUBLIC include)

if (WIN32)
    target_link_libraries(glad PRIVATE libdl.a)
else()
    target_link_libraries(glad PRIVATE dl)
endif()