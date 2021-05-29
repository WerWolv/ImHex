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

        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGIT_COMMIT_HASH=\"\\\"${GIT_COMMIT_HASH}\"\\\"")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGIT_BRANCH=\"\\\"${GIT_BRANCH}\"\\\"")
    endif()

    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -DPROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR} -DPROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR} -DPROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH} ")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DRELEASE -DIMHEX_VERSION=\"\\\"${PROJECT_VERSION}\"\\\"")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -DIMHEX_VERSION=\"\\\"${PROJECT_VERSION}-Debug\"\\\"")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DRELEASE -DIMHEX_VERSION=\"\\\"${PROJECT_VERSION}-ReleaseWithDebugInfo\"\\\"")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -DRELEASE -DIMHEX_VERSION=\"\\\"${PROJECT_VERSION}-ReleaseMinimumSize\"\\\"")
endmacro()

macro(findLibraries)
    set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
    set(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)

    # Enforce that we use non system Python 3 on macOS.
    set(Python_FIND_FRAMEWORK NEVER)

    # Find packages
    find_package(PkgConfig REQUIRED)

    find_package(mbedTLS REQUIRED)

    pkg_search_module(CAPSTONE REQUIRED capstone)

    find_package(OpenGL REQUIRED)

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

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS} -DPYTHON_VERSION_MAJOR_MINOR=\"\\\"${PYTHON_VERSION_MAJOR_MINOR}\"\\\"")

    pkg_search_module(MAGIC libmagic)
    if(NOT MAGIC_FOUND)
        find_library(MAGIC magic REQUIRED)
    else()
        set(MAGIC_INCLUDE_DIRS ${MAGIC_INCLUDEDIR})
    endif()
endmacro()

# Detect current OS / System
macro(detectOS)
    if (WIN32)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOS_WINDOWS")
        set(CMAKE_INSTALL_BINDIR ".")
        set(CMAKE_INSTALL_LIBDIR ".")
        set(PLUGINS_INSTALL_LOCATION "plugins")
        set(MAGIC_INSTALL_LOCATION "magic")
        set(RESOURCES_INSTALL_LOCATION "resources")
    elseif(APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOS_MACOS")
        set(CMAKE_INSTALL_BINDIR ".")
        set(CMAKE_INSTALL_LIBDIR ".")
        set(PLUGINS_INSTALL_LOCATION "plugins")
        set(MAGIC_INSTALL_LOCATION "magic")
        set(RESOURCES_INSTALL_LOCATION "resources")
    elseif(UNIX AND NOT APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOS_LINUX")
        set(CMAKE_INSTALL_BINDIR "bin")
        set(CMAKE_INSTALL_LIBDIR "lib")
        set(PLUGINS_INSTALL_LOCATION "share/imhex/plugins")
        set(MAGIC_INSTALL_LOCATION "share/imhex/magic")
        set(RESOURCES_INSTALL_LOCATION "share/imhex/resources")
    else()
        message(FATAL_ERROR "Unknown / unsupported system!")
    endif()
endmacro()

# Detect 32 vs. 64 bit system
macro(detectArch)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DARCH_64_BIT")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DARCH_32_BIT")
    endif()
endmacro()


macro(configurePackageCreation)
    option (CREATE_PACKAGE "Create a package with CPack" OFF)

    if (APPLE)
        option (CREATE_BUNDLE "Create a bundle on macOS" OFF)
    endif()

    if (WIN32)
        if (CMAKE_BUILD_TYPE EQUAL "DEBUG")
            set(application_type WIN32)
        else ()
            set(application_type)
        endif ()
        set(imhex_icon "${CMAKE_SOURCE_DIR}/res/resource.rc")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--allow-multiple-definition")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wl,-subsystem,windows")

        if (CREATE_PACKAGE)
            set(CPACK_GENERATOR "WIX")
            set(CPACK_PACKAGE_NAME "ImHex")
            set(CPACK_PACKAGE_VENDOR "WerWolv")
            set(CPACK_WIX_UPGRADE_GUID "05000E99-9659-42FD-A1CF-05C554B39285")
            set(CPACK_WIX_PRODUCT_ICON "${PROJECT_SOURCE_DIR}/res/icon.ico")
            set(CPACK_PACKAGE_INSTALL_DIRECTORY "ImHex")
            set_property(INSTALL "$<TARGET_FILE_NAME:imhex>"
                    PROPERTY CPACK_START_MENU_SHORTCUTS "ImHex"
                    )
            set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/res/LICENSE.rtf")
        endif()
    elseif (APPLE)
        set (imhex_icon "${CMAKE_SOURCE_DIR}/res/mac/AppIcon.icns")

        if (CREATE_BUNDLE)
            set(application_type MACOSX_BUNDLE)
            set_source_files_properties(${imhex_icon} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
            set(MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
            set(MACOSX_BUNDLE_INFO_STRING "WerWolv")
            set(MACOSX_BUNDLE_BUNDLE_NAME "ImHex")
            set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
            set(MACOSX_BUNDLE_GUI_IDENTIFIER "WerWolv.ImHex")
            set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}-${GIT_COMMIT_HASH}")
            set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
            set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2020 WerWolv and Thog. All rights reserved." )
            if ("${CMAKE_GENERATOR}" STREQUAL "Xcode")
                set ( bundle_path "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/imhex.app" )
            else ()
                set ( bundle_path "${CMAKE_BINARY_DIR}/imhex.app" )
            endif()
        endif()
    endif()
endmacro()

macro(createPackage)
    file(MAKE_DIRECTORY "plugins")
    foreach (plugin IN LISTS PLUGINS)
        add_subdirectory("plugins/${plugin}")
        if (TARGET ${plugin})
            set_target_properties(${plugin} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
            install(TARGETS ${plugin} RUNTIME DESTINATION ${PLUGINS_INSTALL_LOCATION})
            add_dependencies(imhex ${plugin})
        endif ()
    endforeach()

    set_target_properties(libimhex PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    install(TARGETS libimhex RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR})

    if (WIN32)
        # Install binaries directly in the prefix, usually C:\Program Files\ImHex.
        set(CMAKE_INSTALL_BINDIR ".")

        # Grab all dynamically linked dependencies.
        INSTALL(CODE "set(CMAKE_INSTALL_BINDIR \"${CMAKE_INSTALL_BINDIR}\")")
        INSTALL(CODE "get_filename_component(PY_PARENT ${Python_LIBRARIES} DIRECTORY)")
        INSTALL(CODE "LIST(APPEND DEP_FOLDERS \${PY_PARENT})")
        install(CODE [[
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES $<TARGET_FILE:imhex>
            RESOLVED_DEPENDENCIES_VAR _r_deps
            UNRESOLVED_DEPENDENCIES_VAR _u_deps
            CONFLICTING_DEPENDENCIES_PREFIX _c_deps
            DIRECTORIES ${DEP_FOLDERS}
            POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
        )

        if(_u_deps)
            message(WARNING "There were unresolved dependencies for binary $<TARGET_FILE:imhex>: \"${_u_deps}\"!")
        endif()
        if(_c_deps_FILENAMES)
            message(WARNING "There were conflicting dependencies for library $<TARGET_FILE:imhex>: \"${_c_deps}\"!")
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
    endif()

    if (UNIX AND NOT APPLE)
        string(REPLACE ":" ";" EXTRA_MAGICDBS "${EXTRA_MAGICDBS}")
    endif ()

    if (NOT EXTRA_MAGICDBS STREQUAL "")
        list(GET EXTRA_MAGICDBS -1 EXTRA_MAGICDBS)

        if (NOT EXTRA_MAGICDBS STREQUAL "NOTFOUND")
            if (EXTRA_MAGICDBS MATCHES ".*\\.mgc")
                install(FILES "${EXTRA_MAGICDBS}" DESTINATION ${MAGIC_INSTALL_LOCATION})
            else ()
                install(FILES "${EXTRA_MAGICDBS}.mgc" DESTINATION ${MAGIC_INSTALL_LOCATION})
            endif ()
        endif ()
    endif ()

    # Compile the imhex-specific magicdb
    add_custom_target(magic_dbs ALL
            SOURCES ${MAGICDBS}
            )
    add_custom_command(TARGET magic_dbs
            COMMAND file -C -m ${CMAKE_SOURCE_DIR}/magic_dbs
            )


    # Install the magicdb files.
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/magic_dbs.mgc DESTINATION ${MAGIC_INSTALL_LOCATION} RENAME imhex.mgc)

    # Install resources
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/resources/ DESTINATION ${RESOURCES_INSTALL_LOCATION})


    if (CREATE_BUNDLE)
        include(PostprocessBundle)

        # Fix rpath
        add_custom_command(TARGET imhex POST_BUILD COMMAND ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "@executable_path/../Frameworks/" $<TARGET_FILE:imhex>)

        # FIXME: Remove this once we move/integrate the plugins directory.
        add_custom_target(build-time-make-plugins-directory ALL COMMAND ${CMAKE_COMMAND} -E make_directory "${bundle_path}/Contents/MacOS/plugins")

        # Update library references to make the bundle portable
        postprocess_bundle(imhex)

        # Enforce DragNDrop packaging.
        set(CPACK_GENERATOR "DragNDrop")

        install(TARGETS imhex BUNDLE DESTINATION .)
    else()
        install(TARGETS imhex RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
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