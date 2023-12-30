cmake_minimum_required(VERSION 3.16)
# https://unifoundry.com/unifont/index.html
# https://github.com/microsoft/vscode-codicons
project(imgui_fonts)

set(CMAKE_CXX_STANDARD 17)

add_library(imgui_fonts INTERFACE)

target_include_directories(imgui_fonts INTERFACE
    include
)

target_include_directories(imgui_all_includes INTERFACE include)
