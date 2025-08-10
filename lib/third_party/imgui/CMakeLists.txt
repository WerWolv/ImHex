cmake_minimum_required(VERSION 3.16)
project(imgui)

set(CMAKE_CXX_STANDARD 23)

add_library(imgui_all_includes INTERFACE)

add_subdirectory(imgui)
add_subdirectory(cimgui)
add_subdirectory(implot)
add_subdirectory(implot3d)
add_subdirectory(imnodes)
add_subdirectory(backend)
add_subdirectory(imgui_test_engine)

set(IMGUI_LIBRARIES imgui_imgui imgui_cimgui imgui_implot imgui_implot3d imgui_imnodes imgui_backend)
set(IMGUI_LIBRARIES ${IMGUI_LIBRARIES} PARENT_SCOPE)

if (NOT IMHEX_EXTERNAL_PLUGIN_BUILD)
    foreach (LIBRARY IN LISTS IMGUI_LIBRARIES)
        target_compile_definitions(${LIBRARY} PRIVATE EXPORT_SYMBOLS=1)
    endforeach ()
endif()