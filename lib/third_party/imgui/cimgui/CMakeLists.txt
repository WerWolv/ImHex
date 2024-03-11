cmake_minimum_required(VERSION 3.16)

project(imgui_cimgui)

set(CMAKE_CXX_STANDARD 17)

if (NOT IMHEX_EXTERNAL_PLUGIN_BUILD)
    add_library(imgui_cimgui SHARED
        source/cimgui.cpp
    )

    target_include_directories(imgui_cimgui PUBLIC
        include
    )

    target_link_libraries(imgui_cimgui PRIVATE imgui_includes libimhex)
    set_target_properties(imgui_cimgui PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set_target_properties(imgui_cimgui PROPERTIES OUTPUT_NAME "cimgui")
    set_target_properties(imgui_cimgui PROPERTIES PREFIX "")
    add_dependencies(imhex_all imgui_cimgui)
endif()

install(FILES "$<TARGET_FILE:imgui_cimgui>" DESTINATION "${CMAKE_INSTALL_LIBDIR}" PERMISSIONS ${LIBRARY_PERMISSIONS})
set_target_properties(imgui_cimgui PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})