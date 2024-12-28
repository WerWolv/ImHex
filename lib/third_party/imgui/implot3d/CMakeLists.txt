cmake_minimum_required(VERSION 3.16)
# https://github.com/brenocq/implot3d
project(imgui_implot3d)

set(CMAKE_CXX_STANDARD 17)

if (NOT IMHEX_EXTERNAL_PLUGIN_BUILD)
    add_library(imgui_implot3d OBJECT
        source/implot3d.cpp
        source/implot3d_items.cpp
        source/implot3d_demo.cpp
        source/implot3d_meshes.cpp
    )

    target_include_directories(imgui_implot3d PUBLIC
        include
    )

    target_link_libraries(imgui_implot3d PRIVATE imgui_includes)
endif()

target_include_directories(imgui_all_includes INTERFACE include)
