include(FetchContent)

macro(addVersionDefines)
    if (IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git")
        # Get the current working branch
        execute_process(
                COMMAND git rev-parse --abbrev-ref HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_BRANCH
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        # Get the latest abbreviated commit hash of the working branch
        execute_process(
                COMMAND git log -1 --format=%h
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_COMMIT_HASH
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        add_compile_definitions(GIT_COMMIT_HASH="${GIT_COMMIT_HASH}" GIT_BRANCH="${GIT_BRANCH}")
    endif ()

    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -DPROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR} -DPROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR} -DPROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH} ")

    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set(IMHEX_VERSION_STRING ${IMHEX_VERSION})
    elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(IMHEX_VERSION_STRING ${IMHEX_VERSION}-Debug)
        add_compile_definitions(DEBUG)
    elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(IMHEX_VERSION_STRING ${IMHEX_VERSION}-RelWithDebInfo)
    elseif (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        set(IMHEX_VERSION_STRING ${IMHEX_VERSION}-MinSizeRel)
    endif ()

    add_compile_definitions(IMHEX_VERSION="${IMHEX_VERSION_STRING}")

endmacro()

macro(configurePython)
    set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
    set(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)

    # Enforce that we use non system Python 3 on macOS.
    set(Python_FIND_FRAMEWORK NEVER)

    find_package(Python COMPONENTS Development REQUIRED)
    if(Python_VERSION LESS 3)
        message(STATUS ${PYTHON_VERSION_MAJOR_MINOR})
        message(FATAL_ERROR "No valid version of Python 3 was found.")
    endif()

    string(REPLACE "." ";" PYTHON_VERSION_MAJOR_MINOR ${Python_VERSION})

    list(LENGTH PYTHON_VERSION_MAJOR_MINOR PYTHON_VERSION_COMPONENT_COUNT)

    if (PYTHON_VERSION_COMPONENT_COUNT EQUAL 3)
        list(REMOVE_AT PYTHON_VERSION_MAJOR_MINOR 2)
    endif ()
    list(JOIN PYTHON_VERSION_MAJOR_MINOR "." PYTHON_VERSION_MAJOR_MINOR)

    add_compile_definitions(PYTHON_VERSION_MAJOR_MINOR="${PYTHON_VERSION_MAJOR_MINOR}")
endmacro()

# Detect current OS / System
macro(detectOS)
    if (WIN32)
        add_compile_definitions(OS_WINDOWS)
        set(CMAKE_INSTALL_BINDIR ".")
        set(CMAKE_INSTALL_LIBDIR ".")
        set(PLUGINS_INSTALL_LOCATION "plugins")
        set(MAGIC_INSTALL_LOCATION "magic")
    elseif (APPLE)
        add_compile_definitions(OS_MACOS)
        set(CMAKE_INSTALL_BINDIR ".")
        set(CMAKE_INSTALL_LIBDIR ".")
        set(PLUGINS_INSTALL_LOCATION "plugins")
        set(MAGIC_INSTALL_LOCATION "magic")
    elseif (UNIX AND NOT APPLE)
        add_compile_definitions(OS_LINUX)
        set(CMAKE_INSTALL_BINDIR "bin")
        set(CMAKE_INSTALL_LIBDIR "lib")
        set(PLUGINS_INSTALL_LOCATION "share/imhex/plugins")
        set(MAGIC_INSTALL_LOCATION "share/imhex/magic")
    else ()
        message(FATAL_ERROR "Unknown / unsupported system!")
    endif()
endmacro()

# Detect 32 vs. 64 bit system
macro(detectArch)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_compile_definitions(ARCH_64_BIT)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        add_compile_definitions(ARCH_32_BIT)
    endif()
endmacro()


macro(configurePackingResources)
    option (CREATE_PACKAGE "Create a package with CPack" OFF)

    if (APPLE)
        option (CREATE_BUNDLE "Create a bundle on macOS" OFF)
    endif()

    if (WIN32)
        set(APPLICATION_TYPE)
        set(IMHEX_ICON "${IMHEX_BASE_FOLDER}/resources/resource.rc")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--allow-multiple-definition")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-subsystem,windows")

        if (CREATE_PACKAGE)
            set(CPACK_GENERATOR "WIX")
            set(CPACK_PACKAGE_NAME "ImHex")
            set(CPACK_PACKAGE_VENDOR "WerWolv")
            set(CPACK_WIX_UPGRADE_GUID "05000E99-9659-42FD-A1CF-05C554B39285")
            set(CPACK_WIX_PRODUCT_ICON "${PROJECT_SOURCE_DIR}/resources/icon.ico")
            set(CPACK_PACKAGE_INSTALL_DIRECTORY "ImHex")
            set_property(INSTALL "$<TARGET_FILE_NAME:main>"
                    PROPERTY CPACK_START_MENU_SHORTCUTS "ImHex"
                    )
            set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/resources/LICENSE.rtf")
        endif()
    elseif (APPLE)
        set (IMHEX_ICON "${IMHEX_BASE_FOLDER}/resources/AppIcon.icns")

        if (CREATE_BUNDLE)
            set(APPLICATION_TYPE MACOSX_BUNDLE)
            set_source_files_properties(${IMHEX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
            set(MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
            set(MACOSX_BUNDLE_INFO_STRING "WerWolv")
            set(MACOSX_BUNDLE_BUNDLE_NAME "ImHex")
            set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
            set(MACOSX_BUNDLE_GUI_IDENTIFIER "net.WerWolv.ImHex")
            set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}-${GIT_COMMIT_HASH}")
            set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
            set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2020 WerWolv and Thog. All rights reserved." )
            if ("${CMAKE_GENERATOR}" STREQUAL "Xcode")
                set ( bundle_path "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/ImHex.app" )
            else ()
                set ( bundle_path "${CMAKE_BINARY_DIR}/ImHex.app" )
            endif()
        endif()
    endif()
endmacro()

macro(createPackage)
    file(MAKE_DIRECTORY "plugins")
    foreach (plugin IN LISTS PLUGINS)
        add_subdirectory("plugins/${plugin}")
        if (TARGET ${plugin})
            get_target_property(IS_RUST_PROJECT ${plugin} RUST_PROJECT)

            set_target_properties(${plugin} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
            set_target_properties(${plugin} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

            if (IS_RUST_PROJECT)
                set_target_properties(${plugin} PROPERTIES CARGO_BUILD_TARGET_DIR  ${CMAKE_BINARY_DIR}/plugins)

                get_target_property(PLUGIN_LOCATION ${plugin} LOCATION)

                install(FILES "${PLUGIN_LOCATION}/../${plugin}.hexplug" DESTINATION "${PLUGINS_INSTALL_LOCATION}")
            else ()
                if (WIN32)
                    install(TARGETS ${plugin} RUNTIME DESTINATION ${PLUGINS_INSTALL_LOCATION})
                elseif (APPLE)
                    if (CREATE_BUNDLE)
                        set_target_properties(${plugin} PROPERTIES LIBRARY_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:main>/${PLUGINS_INSTALL_LOCATION})
                    else ()
                        set_target_properties(${plugin} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
                    endif ()
                else ()
                    install(TARGETS ${plugin} LIBRARY DESTINATION ${PLUGINS_INSTALL_LOCATION})
                endif ()
            endif ()

            add_dependencies(imhex_all ${plugin})
        endif ()
    endforeach()

    set_target_properties(libimhex PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

    if (WIN32)
        # Install binaries directly in the prefix, usually C:\Program Files\ImHex.
        set(CMAKE_INSTALL_BINDIR ".")

        # Grab all dynamically linked dependencies.
        INSTALL(CODE "set(CMAKE_INSTALL_BINDIR \"${CMAKE_INSTALL_BINDIR}\")")
        INSTALL(CODE "get_filename_component(PY_PARENT \"${Python_LIBRARIES}\" DIRECTORY)")
        INSTALL(CODE "LIST(APPEND DEP_FOLDERS \${PY_PARENT})")
        install(CODE [[
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES $<TARGET_FILE:builtin> $<TARGET_FILE:libimhex> $<TARGET_FILE:main>
            RESOLVED_DEPENDENCIES_VAR _r_deps
            UNRESOLVED_DEPENDENCIES_VAR _u_deps
            CONFLICTING_DEPENDENCIES_PREFIX _c_deps
            DIRECTORIES ${DEP_FOLDERS} $ENV{PATH}
            POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
        )

        if(_u_deps)
            message(WARNING "There were unresolved dependencies for binary: \"${_u_deps}\"!")
        endif()
        if(_c_deps_FILENAMES)
            message(WARNING "There were conflicting dependencies for library: \"${_c_deps}\"!")
        endif()

        foreach(_file ${_r_deps})
            file(INSTALL
                DESTINATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}"
                TYPE SHARED_LIBRARY
                FOLLOW_SYMLINK_CHAIN
                FILES "${_file}"
                )
        endforeach()
        ]])

        install(FILES "$<TARGET_FILE:libimhex>" DESTINATION "${CMAKE_INSTALL_LIBDIR}")
        downloadImHexPatternsFiles("./")
    elseif(UNIX AND NOT APPLE)
        configure_file(${CMAKE_SOURCE_DIR}/dist/DEBIAN/control.in ${CMAKE_BINARY_DIR}/DEBIAN/control)
        install(FILES ${CMAKE_SOURCE_DIR}/dist/imhex.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
        install(FILES ${CMAKE_SOURCE_DIR}/resources/icon.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/pixmaps RENAME imhex.png)
        install(FILES "$<TARGET_FILE:libimhex>" DESTINATION "${CMAKE_INSTALL_LIBDIR}")
        downloadImHexPatternsFiles("./")
    endif()

    if (CREATE_BUNDLE)
        include(PostprocessBundle)

        # Fix rpath
        add_custom_command(TARGET imhex_all POST_BUILD COMMAND ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "@executable_path/../Frameworks/" $<TARGET_FILE:main>)

        # FIXME: Remove this once we move/integrate the plugins directory.
        add_custom_target(build-time-make-plugins-directory ALL COMMAND ${CMAKE_COMMAND} -E make_directory "${bundle_path}/Contents/MacOS/plugins")
        add_custom_target(build-time-make-resources-directory ALL COMMAND ${CMAKE_COMMAND} -E make_directory "${bundle_path}/Contents/Resources")

        downloadImHexPatternsFiles("${bundle_path}/Contents/MacOS")
        
        install(FILES ${IMHEX_ICON} DESTINATION "${bundle_path}/Contents/Resources")
        install(TARGETS main BUNDLE DESTINATION ".")
        install(FILES $<TARGET_FILE:main> DESTINATION "${bundle_path}")

        # Update library references to make the bundle portable
        postprocess_bundle(imhex_all main)

        # Enforce DragNDrop packaging.
        set(CPACK_GENERATOR "DragNDrop")
    else()
        install(TARGETS main RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()

    if (CREATE_PACKAGE)
        include(apple)
        include(CPack)
    endif()
endmacro()

function(JOIN OUTPUT GLUE)
    set(_TMP_RESULT "")
    set(_GLUE "") # effective glue is empty at the beginning
    foreach(arg ${ARGN})
        set(_TMP_RESULT "${_TMP_RESULT}${_GLUE}${arg}")
        set(_GLUE "${GLUE}")
    endforeach()
    set(${OUTPUT} "${_TMP_RESULT}" PARENT_SCOPE)
endfunction()

macro(setDefaultBuiltTypeIfUnset)
    if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Using Release build type as it was left unset" FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
    endif()
endmacro()

macro(detectBadClone)
    file (GLOB EXTERNAL_DIRS "lib/external/*")
    foreach (EXTERNAL_DIR ${EXTERNAL_DIRS})
        file(GLOB RESULT "${EXTERNAL_DIR}/*")
        list(LENGTH RESULT ENTRY_COUNT)
        if(ENTRY_COUNT LESS_EQUAL 1)
            message(FATAL_ERROR "External dependency ${EXTERNAL_DIR} is empty!\nMake sure to correctly clone ImHex using the --recurse-submodules git option or initialize the submodules manually.")
        endif()
    endforeach ()
endmacro()


function(downloadImHexPatternsFiles dest)
    FetchContent_Declare(
        imhex_patterns
        GIT_REPOSITORY https://github.com/WerWolv/ImHex-Patterns.git
        GIT_TAG master
    )

    FetchContent_Populate(imhex_patterns)

    set(PATTERNS_FOLDERS_TO_INSTALL constants encodings includes patterns magic)
    foreach (FOLDER ${PATTERNS_FOLDERS_TO_INSTALL})
        install(DIRECTORY "${imhex_patterns_SOURCE_DIR}/${FOLDER}" DESTINATION ${dest})
    endforeach()

endfunction()