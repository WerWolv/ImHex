macro(add_imhex_plugin)
    # Parse arguments
    set(options "")
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES INCLUDES LIBRARIES)
    cmake_parse_arguments(IMHEX_PLUGIN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Define new project for plugin
    project(${IMHEX_PLUGIN_NAME})

    # Create a new shared library for the plugin source code
    add_library(${IMHEX_PLUGIN_NAME} SHARED ${IMHEX_PLUGIN_SOURCES})

    # Add include directories and link libraries
    target_include_directories(${IMHEX_PLUGIN_NAME} PRIVATE ${IMHEX_PLUGIN_INCLUDES})
    target_link_libraries(${IMHEX_PLUGIN_NAME} PRIVATE libimhex ${FMT_LIBRARIES} ${IMHEX_PLUGIN_LIBRARIES})

    # Add IMHEX_PROJECT_NAME and IMHEX_VERSION define
    target_compile_definitions(${IMHEX_PLUGIN_NAME} PRIVATE IMHEX_PROJECT_NAME="${IMHEX_PLUGIN_NAME}")
    target_compile_definitions(${IMHEX_PLUGIN_NAME} PRIVATE IMHEX_VERSION="${IMHEX_VERSION_STRING}")

    # Enable required compiler flags
    set_target_properties(${IMHEX_PLUGIN_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    setupCompilerFlags(${IMHEX_PLUGIN_NAME})

    # Configure build properties
    set_target_properties(${IMHEX_PLUGIN_NAME}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
            CXX_STANDARD 23
            PREFIX ""
            SUFFIX ".hexplug"
    )

    # Setup a romfs for the plugin
    set(LIBROMFS_RESOURCE_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/romfs)
    set(LIBROMFS_PROJECT_NAME ${IMHEX_PLUGIN_NAME})
    add_subdirectory(${IMHEX_BASE_FOLDER}/lib/external/libromfs ${CMAKE_CURRENT_BINARY_DIR}/libromfs)
    set_target_properties(${LIBROMFS_LIBRARY} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(${IMHEX_PLUGIN_NAME} PRIVATE ${LIBROMFS_LIBRARY})

    # Add the new plugin to the main dependency list so it gets built by default
    add_dependencies(imhex_all ${IMHEX_PLUGIN_NAME})

    if (EMSCRIPTEN)
        target_link_libraries(plugin-bundle PUBLIC ${IMHEX_PLUGIN_NAME})
    endif ()

endmacro()
