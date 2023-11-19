cmake_minimum_required(VERSION 3.16)
# https://github.com/Nelarius/imnodes
project(imgui_imnodes)

set(CMAKE_CXX_STANDARD 17)

add_library(imgui_imnodes OBJECT
    source/imnodes.cpp
)

target_include_directories(imgui_imnodes PUBLIC
    include
)

target_link_libraries(imgui_imnodes PRIVATE imgui_includes)
set_property(TARGET imgui_imnodes PROPERTY POSITION_INDEPENDENT_CODE ON)
