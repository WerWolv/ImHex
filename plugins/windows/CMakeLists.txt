cmake_minimum_required(VERSION 3.16)

# Change this to the name of your plugin #
project(windows)

# Add your source files here #
if (WIN32)

    add_library(${PROJECT_NAME} SHARED
            source/plugin_windows.cpp

            source/views/view_tty_console.cpp

            source/content/ui_items.cpp
            source/content/settings_entries.cpp
            source/content/providers.cpp

            source/content/providers/process_memory_provider.cpp
    )

    # Add additional include directories here #
    target_include_directories(${PROJECT_NAME} PRIVATE include)
    # Add additional libraries here #
    target_link_libraries(${PROJECT_NAME} PRIVATE libimhex)


    # ---- No need to change anything from here downwards unless you know what you're doing ---- #

    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".hexplug")

    if (WIN32)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--allow-multiple-definition -fvisibility=hidden")
    endif()

    add_compile_definitions(IMHEX_PROJECT_NAME="${PROJECT_NAME}")
    set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    setupCompilerWarnings(${PROJECT_NAME})

    set(LIBROMFS_RESOURCE_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/romfs)
    set(LIBROMFS_PROJECT_NAME ${PROJECT_NAME})
    add_subdirectory(../../lib/external/libromfs ${CMAKE_CURRENT_BINARY_DIR}/libromfs)
    set_target_properties(${LIBROMFS_LIBRARY} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBROMFS_LIBRARY})

endif ()