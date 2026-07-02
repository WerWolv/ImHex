cmake_minimum_required(VERSION 3.16)
# https://github.com/ocornut/imgui_test_engine
project(imgui_test_engine)

set(CMAKE_CXX_STANDARD 23)

if (IMHEX_ENABLE_IMGUI_TEST_ENGINE AND NOT IMHEX_EXTERNAL_PLUGIN_BUILD)
    add_library(imgui_test_engine OBJECT
            source/imgui_capture_tool.cpp
            source/imgui_te_context.cpp
            source/imgui_te_coroutine.cpp
            source/imgui_te_engine.cpp
            source/imgui_te_exporters.cpp
            source/imgui_te_perftool.cpp
            source/imgui_te_ui.cpp
            source/imgui_te_utils.cpp
    )

    target_include_directories(imgui_test_engine PUBLIC
        include
    )
    target_link_libraries(imgui_test_engine PRIVATE imgui_includes imgui_implot)
    target_compile_definitions(imgui_test_engine PUBLIC IMGUI_TEST_ENGINE=1)
    target_compile_definitions(imgui_test_engine PRIVATE EXPORT_SYMBOLS=1)

    if (MSVC)
        target_compile_options(imgui_test_engine PUBLIC /wd4251)
    endif()
endif()

add_library(imgui_test_engine_includes INTERFACE)
target_include_directories(imgui_test_engine_includes INTERFACE include)
target_include_directories(imgui_all_includes INTERFACE include)