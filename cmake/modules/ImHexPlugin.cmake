macro(add_imhex_plugin)
    set(options "")
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES INCLUDES LIBRARIES)
    cmake_parse_arguments(IMHEX_PLUGIN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    project(${IMHEX_PLUGIN_NAME})

    add_library(${IMHEX_PLUGIN_NAME} SHARED ${IMHEX_PLUGIN_SOURCES})

    target_include_directories(${IMHEX_PLUGIN_NAME} PRIVATE ${IMHEX_PLUGIN_INCLUDES})
    target_link_libraries(${IMHEX_PLUGIN_NAME} PRIVATE libimhex ${FMT_LIBRARIES} ${IMHEX_PLUGIN_LIBRARIES})

    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".hexplug")

    target_compile_definitions(${IMHEX_PLUGIN_NAME} PRIVATE IMHEX_PROJECT_NAME="${IMHEX_PLUGIN_NAME}")
    target_compile_definitions(${IMHEX_PLUGIN_NAME} PRIVATE IMHEX_VERSION="${IMHEX_VERSION_STRING}")

    set_target_properties(${IMHEX_PLUGIN_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    setupCompilerFlags(${IMHEX_PLUGIN_NAME})

    set(LIBROMFS_RESOURCE_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/romfs)
    set(LIBROMFS_PROJECT_NAME ${IMHEX_PLUGIN_NAME})
    add_subdirectory(${IMHEX_BASE_FOLDER}/lib/external/libromfs ${CMAKE_CURRENT_BINARY_DIR}/libromfs)
    set_target_properties(${LIBROMFS_LIBRARY} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(${IMHEX_PLUGIN_NAME} PRIVATE ${LIBROMFS_LIBRARY})

    set_target_properties(${IMHEX_PLUGIN_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
endmacro()