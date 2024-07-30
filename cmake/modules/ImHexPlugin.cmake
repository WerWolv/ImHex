macro(add_imhex_plugin)
    # Parse arguments
    set(options LIBRARY_PLUGIN)
    set(oneValueArgs NAME IMHEX_VERSION)
    set(multiValueArgs SOURCES INCLUDES LIBRARIES FEATURES)
    cmake_parse_arguments(IMHEX_PLUGIN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (IMHEX_PLUGIN_IMHEX_VERSION)
        message(STATUS "Compiling plugin ${IMHEX_PLUGIN_NAME} for ImHex Version ${IMHEX_PLUGIN_IMHEX_VERSION}")
        set(IMHEX_VERSION_STRING "${IMHEX_PLUGIN_IMHEX_VERSION}")
    endif()

    if (IMHEX_STATIC_LINK_PLUGINS)
        set(IMHEX_PLUGIN_LIBRARY_TYPE STATIC)

        target_link_libraries(libimhex PUBLIC ${IMHEX_PLUGIN_NAME})

        configure_file(${CMAKE_SOURCE_DIR}/dist/web/plugin-bundle.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/plugin-bundle.cpp @ONLY)
        target_sources(main PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/plugin-bundle.cpp)
        set(IMHEX_PLUGIN_SUFFIX ".hexplug")
    else()
        if (IMHEX_PLUGIN_LIBRARY_PLUGIN)
            set(IMHEX_PLUGIN_LIBRARY_TYPE SHARED)
            set(IMHEX_PLUGIN_SUFFIX ".hexpluglib")
        else()
            set(IMHEX_PLUGIN_LIBRARY_TYPE MODULE)
            set(IMHEX_PLUGIN_SUFFIX ".hexplug")
        endif()
    endif()

    # Define new project for plugin
    project(${IMHEX_PLUGIN_NAME})

    # Create a new shared library for the plugin source code
    add_library(${IMHEX_PLUGIN_NAME} ${IMHEX_PLUGIN_LIBRARY_TYPE} ${IMHEX_PLUGIN_SOURCES})

    # Add include directories and link libraries
    target_include_directories(${IMHEX_PLUGIN_NAME} PUBLIC ${IMHEX_PLUGIN_INCLUDES})
    target_link_libraries(${IMHEX_PLUGIN_NAME} PUBLIC ${IMHEX_PLUGIN_LIBRARIES})
    target_link_libraries(${IMHEX_PLUGIN_NAME} PRIVATE libimhex ${FMT_LIBRARIES} imgui_all_includes libwolv)
    addIncludesFromLibrary(${IMHEX_PLUGIN_NAME} libpl)
    addIncludesFromLibrary(${IMHEX_PLUGIN_NAME} libpl-gen)

    precompileHeaders(${IMHEX_PLUGIN_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/include")

    # Add IMHEX_PROJECT_NAME and IMHEX_VERSION define
    target_compile_definitions(${IMHEX_PLUGIN_NAME} PRIVATE IMHEX_PROJECT_NAME="${IMHEX_PLUGIN_NAME}")
    target_compile_definitions(${IMHEX_PLUGIN_NAME} PRIVATE IMHEX_VERSION="${IMHEX_VERSION_STRING}")
    target_compile_definitions(${IMHEX_PLUGIN_NAME} PRIVATE IMHEX_PLUGIN_NAME=${IMHEX_PLUGIN_NAME})

    # Enable required compiler flags
    enableUnityBuild(${IMHEX_PLUGIN_NAME})
    setupCompilerFlags(${IMHEX_PLUGIN_NAME})

    # Configure build properties
    set_target_properties(${IMHEX_PLUGIN_NAME}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${IMHEX_MAIN_OUTPUT_DIRECTORY}/plugins"
            CXX_STANDARD 23
            PREFIX ""
            SUFFIX ${IMHEX_PLUGIN_SUFFIX}
    )

    # Set rpath of plugin libraries to the plugins folder
    if (APPLE)
        set_target_properties(${IMHEX_PLUGIN_NAME} PROPERTIES BUILD_RPATH "@executable_path/../Frameworks;@executable_path/plugins")
    endif()

    # Setup a romfs for the plugin
    list(APPEND LIBROMFS_RESOURCE_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/romfs)
    set(LIBROMFS_PROJECT_NAME ${IMHEX_PLUGIN_NAME})
    add_subdirectory(${IMHEX_BASE_FOLDER}/lib/external/libromfs ${CMAKE_CURRENT_BINARY_DIR}/libromfs)
    target_link_libraries(${IMHEX_PLUGIN_NAME} PRIVATE ${LIBROMFS_LIBRARY})

    set(FEATURE_DEFINE_CONTENT)

    if (IMHEX_PLUGIN_FEATURES)
        list(LENGTH IMHEX_PLUGIN_FEATURES IMHEX_FEATURE_COUNT)
        math(EXPR IMHEX_FEATURE_COUNT "${IMHEX_FEATURE_COUNT} - 1" OUTPUT_FORMAT DECIMAL)
        foreach(index RANGE 0 ${IMHEX_FEATURE_COUNT} 2)
            list(SUBLIST IMHEX_PLUGIN_FEATURES ${index} 2 IMHEX_PLUGIN_FEATURE)
            list(GET IMHEX_PLUGIN_FEATURE 0 feature_define)
            list(GET IMHEX_PLUGIN_FEATURE 1 feature_description)

            string(TOUPPER ${feature_define} feature_define)
            add_definitions(-DIMHEX_PLUGIN_${IMHEX_PLUGIN_NAME}_FEATURE_${feature_define}=0)
            set(FEATURE_DEFINE_CONTENT "${FEATURE_DEFINE_CONTENT}{ \"${feature_description}\", IMHEX_FEATURE_ENABLED(${feature_define}) },")
        endforeach()
    endif()

    target_compile_options(${IMHEX_PLUGIN_NAME} PRIVATE -DIMHEX_PLUGIN_FEATURES_CONTENT=${FEATURE_DEFINE_CONTENT})

    # Add the new plugin to the main dependency list so it gets built by default
    if (TARGET imhex_all)
        add_dependencies(imhex_all ${IMHEX_PLUGIN_NAME})
    endif()

    if (IMHEX_EXTERNAL_PLUGIN_BUILD)
        install(TARGETS ${IMHEX_PLUGIN_NAME} DESTINATION ".")
    endif()

    # Fix rpath
    if (APPLE)
        set_target_properties(
                ${IMHEX_PLUGIN_NAME}
                PROPERTIES
                    INSTALL_RPATH "@executable_path/../Frameworks;@executable_path/plugins"
        )
    elseif (UNIX)
        set(PLUGIN_RPATH "")
        list(APPEND PLUGIN_RPATH "$ORIGIN")

        if (IMHEX_PLUGIN_ADD_INSTALL_PREFIX_TO_RPATH)
            list(APPEND PLUGIN_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
        endif()

        set_target_properties(
                ${IMHEX_PLUGIN_NAME}
                PROPERTIES
                    INSTALL_RPATH_USE_ORIGIN ON
                    INSTALL_RPATH "${PLUGIN_RPATH}"
        )
    endif()

    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt AND IMHEX_ENABLE_UNIT_TESTS AND IMHEX_ENABLE_PLUGIN_TESTS)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
        target_link_libraries(${IMHEX_PLUGIN_NAME} PUBLIC ${IMHEX_PLUGIN_NAME}_tests)
        target_compile_definitions(${IMHEX_PLUGIN_NAME}_tests PRIVATE IMHEX_PROJECT_NAME="${IMHEX_PLUGIN_NAME}-tests")
    endif()
endmacro()

macro(add_romfs_resource input output)
    if (NOT EXISTS ${input})
        message(WARNING "Resource file ${input} does not exist")
    endif()

    configure_file(${input} ${CMAKE_CURRENT_BINARY_DIR}/romfs/${output} COPYONLY)

    list(APPEND LIBROMFS_RESOURCE_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/romfs)
endmacro()

macro (enable_plugin_feature feature)
    string(TOUPPER ${feature} feature)
    if (NOT (feature IN_LIST IMHEX_PLUGIN_FEATURES))
        message(FATAL_ERROR "Feature ${feature} is not enabled for plugin ${IMHEX_PLUGIN_NAME}")
    endif()

    remove_definitions(-DIMHEX_PLUGIN_${IMHEX_PLUGIN_NAME}_FEATURE_${feature}=0)
    add_definitions(-DIMHEX_PLUGIN_${IMHEX_PLUGIN_NAME}_FEATURE_${feature}=1)
endmacro()