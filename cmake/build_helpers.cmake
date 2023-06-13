include(FetchContent)

if(IMHEX_STRIP_RELEASE)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CPACK_STRIP_FILES TRUE)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_link_options($<$<CONFIG:RELEASE>:-s>)
    endif()
endif()

macro(addDefines)
    if (NOT IMHEX_VERSION)
        message(FATAL_ERROR "IMHEX_VERSION is not defined")
    endif ()

    if (DEFINED IMHEX_COMMIT_HASH_LONG AND DEFINED IMHEX_COMMIT_HASH_SHORT AND DEFINED IMHEX_COMMIT_BRANCH)
        add_compile_definitions(
                GIT_COMMIT_HASH_LONG="${IMHEX_COMMIT_HASH_LONG}"
                IMHEX_COMMIT_HASH_SHORT="${IMHEX_COMMIT_HASH_SHORT}"
                GIT_BRANCH="${IMHEX_COMMIT_BRANCH}"
        )
    else()
        # Get the current working branch
        execute_process(
                COMMAND git rev-parse --abbrev-ref HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_BRANCH
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE RESULT_BRANCH
        )

        # Get the latest abbreviated commit hash of the working branch
        execute_process(
                COMMAND git log -1 --format=%h --abbrev=7
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_COMMIT_HASH_SHORT
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE RESULT_HASH_SHORT
        )

        execute_process(
                COMMAND git log -1 --format=%H
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_COMMIT_HASH_LONG
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE RESULT_HASH_LONG
        )

        if (RESULT_BRANCH EQUAL 0 AND RESULT_HASH_LONG EQUAL 0 AND RESULT_HASH_SHORT EQUAL 0)
            add_compile_definitions(
                    GIT_COMMIT_HASH_SHORT="${GIT_COMMIT_HASH_SHORT}"
                    GIT_COMMIT_HASH_LONG="${GIT_COMMIT_HASH_LONG}"
                    GIT_BRANCH="${GIT_BRANCH}")
        endif ()
    endif ()

    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -DPROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR} -DPROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR} -DPROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH} ")

    set(IMHEX_VERSION_STRING ${IMHEX_VERSION})
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set(IMHEX_VERSION_STRING ${IMHEX_VERSION_STRING})
        add_compile_definitions(NDEBUG)
    elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(IMHEX_VERSION_STRING ${IMHEX_VERSION_STRING}-Debug)
        add_compile_definitions(DEBUG _GLIBCXX_DEBUG _GLIBCXX_VERBOSE)
    elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(IMHEX_VERSION_STRING ${IMHEX_VERSION_STRING}-RelWithDebInfo)
        add_compile_definitions(NDEBUG)
    elseif (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        set(IMHEX_VERSION_STRING ${IMHEX_VERSION_STRING}-MinSizeRel)
        add_compile_definitions(NDEBUG)
    endif ()

    add_compile_definitions(IMHEX_VERSION="${IMHEX_VERSION_STRING}")

    if (NOT IMHEX_DISABLE_UPDATE_CHECK)
        add_compile_definitions(HEX_UPDATE_CHECK)
    endif()
endmacro()

# Detect current OS / System
macro(detectOS)
    if (WIN32)
        add_compile_definitions(OS_WINDOWS)
        set(CMAKE_INSTALL_BINDIR ".")
        set(CMAKE_INSTALL_LIBDIR ".")
        set(PLUGINS_INSTALL_LOCATION "plugins")

        if (NOT USE_SYSTEM_CURL)
            SET(IMHEX_USE_BUNDLED_CA ON)
        endif ()
    elseif (APPLE)
        add_compile_definitions(OS_MACOS)
        set(CMAKE_INSTALL_BINDIR ".")
        set(CMAKE_INSTALL_LIBDIR ".")
        set(PLUGINS_INSTALL_LOCATION "plugins")
        enable_language(OBJC)
        enable_language(OBJCXX)
    elseif (UNIX AND NOT APPLE)
        add_compile_definitions(OS_LINUX)
        include(GNUInstallDirs)

        if(IMHEX_PLUGINS_IN_SHARE)
            set(PLUGINS_INSTALL_LOCATION "share/imhex/plugins")
        else()
            set(PLUGINS_INSTALL_LOCATION "${CMAKE_INSTALL_LIBDIR}/imhex/plugins")
            # Warning : Do not work with portable versions such as appimage (because the path is hardcoded)
            add_compile_definitions(SYSTEM_PLUGINS_LOCATION="${CMAKE_INSTALL_FULL_LIBDIR}/imhex") # "plugins" will be appended from the app
        endif()

    else ()
        message(FATAL_ERROR "Unknown / unsupported system!")
    endif()

    if(IMHEX_USE_BUNDLED_CA)
        add_compile_definitions(IMHEX_USE_BUNDLED_CA)
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
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-subsystem,windows")

        if (CREATE_PACKAGE)
            set(CPACK_GENERATOR "WIX")
            set(CPACK_PACKAGE_NAME "ImHex")
            set(CPACK_PACKAGE_VENDOR "WerWolv")
            set(CPACK_WIX_UPGRADE_GUID "05000E99-9659-42FD-A1CF-05C554B39285")
            set(CPACK_WIX_PRODUCT_ICON "${PROJECT_SOURCE_DIR}/resources/dist/windows/icon.ico")
            set(CPACK_WIX_UI_BANNER "${PROJECT_SOURCE_DIR}/resources/dist/windows/wix_banner.png")
            set(CPACK_WIX_UI_DIALOG "${PROJECT_SOURCE_DIR}/resources/dist/windows/wix_dialog.png")
            set(CPACK_WIX_CULTURES "en-US;de-DE;ja-JP;it-IT;pt-BR;zh-CN;zh-TW")
            set(CPACK_PACKAGE_INSTALL_DIRECTORY "ImHex")
            set_property(INSTALL "$<TARGET_FILE_NAME:main>"
                    PROPERTY CPACK_START_MENU_SHORTCUTS "ImHex"
                    )
            set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/resources/dist/windows/LICENSE.rtf")
        endif()
    elseif (APPLE)
        set (IMHEX_ICON "${IMHEX_BASE_FOLDER}/resources/dist/macos/AppIcon.icns")

        if (CREATE_BUNDLE)
            set(APPLICATION_TYPE MACOSX_BUNDLE)
            set_source_files_properties(${IMHEX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
            set(MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
            set(MACOSX_BUNDLE_INFO_STRING "WerWolv")
            set(MACOSX_BUNDLE_BUNDLE_NAME "ImHex")
            set(MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/dist/macos/Info.plist.in")
            set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
            set(MACOSX_BUNDLE_GUI_IDENTIFIER "net.WerWolv.ImHex")
            set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}-${GIT_COMMIT_HASH_SHORT}")
            set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

            string(TIMESTAMP CURR_YEAR "%Y")
            set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2020 - ${CURR_YEAR} WerWolv. All rights reserved." )
            if ("${CMAKE_GENERATOR}" STREQUAL "Xcode")
                set (IMHEX_BUNDLE_PATH "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/ImHex.app")
            else ()
                set (IMHEX_BUNDLE_PATH "${CMAKE_BINARY_DIR}/ImHex.app")
            endif()
        endif()
    endif()
endmacro()

macro(createPackage)
    set(LIBRARY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

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

                install(FILES "${PLUGIN_LOCATION}/../${plugin}.hexplug" DESTINATION "${PLUGINS_INSTALL_LOCATION}" PERMISSIONS ${LIBRARY_PERMISSIONS})
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

        install(FILES "$<TARGET_FILE:libimhex>" DESTINATION "${CMAKE_INSTALL_LIBDIR}" PERMISSIONS ${LIBRARY_PERMISSIONS})
        downloadImHexPatternsFiles("./")
    elseif(UNIX AND NOT APPLE)

        set_target_properties(libimhex PROPERTIES SOVERSION ${IMHEX_VERSION})
        
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dist/DEBIAN/control.in ${CMAKE_BINARY_DIR}/DEBIAN/control)
        
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_PREFIX}/share/licenses/imhex)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/dist/imhex.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/pixmaps RENAME imhex.png)
        install(FILES "$<TARGET_FILE:libimhex>" DESTINATION "${CMAKE_INSTALL_LIBDIR}" PERMISSIONS ${LIBRARY_PERMISSIONS})
        downloadImHexPatternsFiles("./share/imhex")
        
        # install AppStream file
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/dist/net.werwolv.imhex.metainfo.xml DESTINATION ${CMAKE_INSTALL_PREFIX}/share/metainfo)
        
        # install symlink for the old standard name
        file(CREATE_LINK net.werwolv.imhex.metainfo.xml ${CMAKE_CURRENT_BINARY_DIR}/net.werwolv.imhex.appdata.xml SYMBOLIC)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/net.werwolv.imhex.appdata.xml DESTINATION ${CMAKE_INSTALL_PREFIX}/share/metainfo)

    endif()
    
    if (CREATE_BUNDLE)
        include(PostprocessBundle)
        
        set_target_properties(libimhex PROPERTIES SOVERSION ${IMHEX_VERSION})

        set_property(TARGET main PROPERTY MACOSX_BUNDLE_INFO_PLIST ${MACOSX_BUNDLE_INFO_PLIST})

        # Fix rpath
        add_custom_command(TARGET imhex_all POST_BUILD COMMAND ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "@executable_path/../Frameworks/" $<TARGET_FILE:main>)

        # FIXME: Remove this once we move/integrate the plugins directory.
        add_custom_target(build-time-make-plugins-directory ALL COMMAND ${CMAKE_COMMAND} -E make_directory "${IMHEX_BUNDLE_PATH}/Contents/MacOS/plugins")
        add_custom_target(build-time-make-resources-directory ALL COMMAND ${CMAKE_COMMAND} -E make_directory "${IMHEX_BUNDLE_PATH}/Contents/Resources")

        downloadImHexPatternsFiles("${IMHEX_BUNDLE_PATH}/Contents/MacOS")
        
        install(FILES ${IMHEX_ICON} DESTINATION "${IMHEX_BUNDLE_PATH}/Contents/Resources")
        install(TARGETS main BUNDLE DESTINATION ".")
        install(FILES $<TARGET_FILE:main> DESTINATION "${IMHEX_BUNDLE_PATH}")

        # Update library references to make the bundle portable
        postprocess_bundle(imhex_all main)

        # Enforce DragNDrop packaging.
        set(CPACK_GENERATOR "DragNDrop")
    else()
        install(TARGETS main RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()

    if (CREATE_PACKAGE)
        set (CPACK_BUNDLE_NAME "ImHex")
        set (CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/resources/dist/macos/AppIcon.icns" )
        set (CPACK_BUNDLE_PLIST "${CMAKE_BINARY_DIR}/ImHex.app/Contents/Info.plist")

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

function(loadVersion version)
    set(VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${VERSION_FILE})
    file(READ "${VERSION_FILE}" read_version)
    string(STRIP ${read_version} read_version)
    set(${version} ${read_version} PARENT_SCOPE)
endfunction()

set(badCloneExclusions "")

function(detectBadClone)
    if (IMHEX_IGNORE_BAD_CLONE)
        return()
    endif()

    file (GLOB EXTERNAL_DIRS "lib/external/*")
    foreach (EXTERNAL_DIR ${EXTERNAL_DIRS})
        get_filename_component(basename EXTERNAL_DIR NAME)
        list(FIND badCloneExclusions ${basename} excludedIndex)
        if(NOT excludedIndex EQUAL -1)
            file(GLOB RESULT "${EXTERNAL_DIR}/*")
            list(LENGTH RESULT ENTRY_COUNT)
            if(ENTRY_COUNT LESS_EQUAL 1)
                message(FATAL_ERROR "External dependency ${EXTERNAL_DIR} is empty!\nMake sure to correctly clone ImHex using the --recurse-submodules git option or initialize the submodules manually.")
            endif()
        endif()
    endforeach ()
endfunction()

function(verifyCompiler)
    if (IMHEX_IGNORE_BAD_COMPILER)
        return()
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12.0.0")
        message(FATAL_ERROR "ImHex requires GCC 12.0.0 or newer. Please use the latest GCC version.")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0.0")
        message(FATAL_ERROR "ImHex requires Clang 14.0.0 or newer. Please use the latest Clang version.")
    elseif (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
        message(FATAL_ERROR "ImHex can only be compiled with GCC or Clang. ${CMAKE_CXX_COMPILER_ID} is not supported.")
    endif()
endfunction()

macro(setVariableInParent variable value)
    get_directory_property(hasParent PARENT_DIRECTORY)

    if (hasParent)
        set(${variable} "${value}" PARENT_SCOPE)
    else ()
        set(${variable} "${value}")
    endif ()
endmacro()

function(downloadImHexPatternsFiles dest)
    if (NOT IMHEX_OFFLINE_BUILD)
        if (IMHEX_PATTERNS_PULL_MASTER)
            set(PATTERNS_BRANCH master)
        else ()
            set(PATTERNS_BRANCH ImHex-v${IMHEX_VERSION})
        endif ()

        FetchContent_Declare(
            imhex_patterns
            GIT_REPOSITORY https://github.com/WerWolv/ImHex-Patterns.git
            GIT_TAG master
        )

        message(STATUS "Downloading ImHex-Patterns repo branch ${PATTERNS_BRANCH}...")
        FetchContent_MakeAvailable(imhex_patterns)
        message(STATUS "Finished downloading ImHex-Patterns")

    else ()
        # Maybe patterns are cloned to a subdirectory
        set(imhex_patterns_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ImHex-Patterns")
    endif ()

    if (EXISTS ${imhex_patterns_SOURCE_DIR})
        set(PATTERNS_FOLDERS_TO_INSTALL constants encodings includes patterns magic)
        foreach (FOLDER ${PATTERNS_FOLDERS_TO_INSTALL})
            install(DIRECTORY "${imhex_patterns_SOURCE_DIR}/${FOLDER}" DESTINATION ${dest} PATTERN "**/_schema.json" EXCLUDE)
        endforeach ()
    endif ()

endfunction()

macro(setupCompilerWarnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(IMHEX_COMMON_FLAGS "-Wall -Wextra -Wpedantic -Werror")
        set(IMHEX_C_FLAGS "${IMHEX_COMMON_FLAGS} -Wno-array-bounds")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            set(IMHEX_C_FLAGS "${IMHEX_C_FLAGS} -Wno-restrict -Wno-stringop-overread -Wno-stringop-overflow -Wno-dangling-reference")
        endif()
    endif()

    set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS}    ${IMHEX_C_FLAGS}")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS}  ${IMHEX_C_FLAGS}")
    set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} ${IMHEX_COMMON_FLAGS}")
endmacro()

# uninstall target
macro(setUninstallTarget)
    if(NOT TARGET uninstall)
        configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)
    
        add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
    endif()
endmacro()

find_package(PkgConfig REQUIRED)

macro(addBundledLibraries)
    set(EXTERN_LIBS_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/lib/external")

    set(BUILD_SHARED_LIBS OFF)
    add_subdirectory(${EXTERN_LIBS_FOLDER}/imgui)
    set_target_properties(imgui PROPERTIES POSITION_INDEPENDENT_CODE ON)

    add_subdirectory(${EXTERN_LIBS_FOLDER}/microtar EXCLUDE_FROM_ALL)
    set_target_properties(microtar PROPERTIES POSITION_INDEPENDENT_CODE ON)

    add_subdirectory(${EXTERN_LIBS_FOLDER}/libwolv EXCLUDE_FROM_ALL)
    set_property(TARGET libwolv-types PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET libwolv-utils PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET libwolv-io PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET libwolv-hash PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET libwolv-containers PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET libwolv-net PROPERTY POSITION_INDEPENDENT_CODE ON)

    set(XDGPP_INCLUDE_DIRS "${EXTERN_LIBS_FOLDER}/xdgpp")
    set(CURL_USE_MBEDTLS ON)
    set(BUILD_CURL_EXE OFF)
    set(FPHSA_NAME_MISMATCHED ON CACHE BOOL "")

    if(NOT USE_SYSTEM_FMT)
        add_subdirectory(${EXTERN_LIBS_FOLDER}/fmt EXCLUDE_FROM_ALL)
        set_target_properties(fmt PROPERTIES POSITION_INDEPENDENT_CODE ON)
        set(FMT_LIBRARIES fmt::fmt-header-only)
    else()
        find_package(fmt 8.0.0 REQUIRED)
        set(FMT_LIBRARIES fmt::fmt)
    endif()

    if (IMHEX_USE_GTK_FILE_PICKER)
        set(NFD_PORTAL OFF CACHE BOOL "Use Portals for Linux file dialogs" FORCE)
    else ()
        set(NFD_PORTAL ON CACHE BOOL "Use GTK for Linux file dialogs" FORCE)
    endif ()

    if (NOT USE_SYSTEM_NFD)
        add_subdirectory(${EXTERN_LIBS_FOLDER}/nativefiledialog EXCLUDE_FROM_ALL)
        set_target_properties(nfd PROPERTIES POSITION_INDEPENDENT_CODE ON)
        set(NFD_LIBRARIES nfd)
    else()
        find_package(nfd)
        set(NFD_LIBRARIES nfd)
    endif()

    if(NOT USE_SYSTEM_NLOHMANN_JSON)
        add_subdirectory(${EXTERN_LIBS_FOLDER}/nlohmann_json EXCLUDE_FROM_ALL)
        set(NLOHMANN_JSON_LIBRARIES nlohmann_json)
    else()
        find_package(nlohmann_json 3.10.2 REQUIRED)
        set(NLOHMANN_JSON_LIBRARIES nlohmann_json::nlohmann_json)
    endif()

    if(NOT USE_SYSTEM_CURL)
        add_subdirectory(${EXTERN_LIBS_FOLDER}/curl EXCLUDE_FROM_ALL)
        set_target_properties(libcurl PROPERTIES POSITION_INDEPENDENT_CODE ON)
        target_compile_options(libcurl PRIVATE -Wno-deprecated-declarations)
        set(LIBCURL_LIBRARIES libcurl)
    else()
        pkg_check_modules(LIBCURL REQUIRED IMPORTED_TARGET libcurl>=7.76.1)
    endif()

    if (NOT USE_SYSTEM_LLVM)
        add_subdirectory(${EXTERN_LIBS_FOLDER}/llvm-demangle EXCLUDE_FROM_ALL)
        set_target_properties(LLVMDemangle PROPERTIES POSITION_INDEPENDENT_CODE ON)
    else()
        find_package(LLVM REQUIRED Demangle)
    endif()

    if (NOT USE_SYSTEM_YARA)
        add_subdirectory(${EXTERN_LIBS_FOLDER}/yara EXCLUDE_FROM_ALL)
        set_target_properties(libyara PROPERTIES POSITION_INDEPENDENT_CODE ON)
        set(YARA_LIBRARIES libyara)
    else()
        pkg_check_modules(YARA REQUIRED IMPORTED_TARGET yara)
    endif()

    if (NOT USE_SYSTEM_MINIAUDIO)
        add_subdirectory(${EXTERN_LIBS_FOLDER}/miniaudio EXCLUDE_FROM_ALL)
        set_target_properties(miniaudio PROPERTIES POSITION_INDEPENDENT_CODE ON)
        set(MINIAUDIO_LIBRARIES miniaudio)
    else()
        pkg_check_modules(miniaudio REQUIRED IMPORTED_TARGET miniaudio)
    endif()

    if (NOT USE_SYSTEM_CAPSTONE)
        set(CAPSTONE_BUILD_STATIC_RUNTIME OFF CACHE BOOL "Disable shared library building")
        set(CAPSTONE_BUILD_SHARED OFF CACHE BOOL "Disable shared library building")
        set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "Disable tests")
        add_subdirectory(${EXTERN_LIBS_FOLDER}/capstone EXCLUDE_FROM_ALL)
        set_target_properties(capstone PROPERTIES POSITION_INDEPENDENT_CODE ON)
        set(CAPSTONE_LIBRARIES "capstone")
        set(CAPSTONE_INCLUDE_DIRS ${EXTERN_LIBS_FOLDER}/capstone/include)
    else()
        pkg_search_module(CAPSTONE 4.0.2 REQUIRED capstone)
    endif()

    add_subdirectory(${EXTERN_LIBS_FOLDER}/pattern_language EXCLUDE_FROM_ALL)
    set_target_properties(libpl PROPERTIES POSITION_INDEPENDENT_CODE ON)

    find_package(mbedTLS 2.26.0 REQUIRED)

    pkg_search_module(MAGIC libmagic>=5.39)
    if(NOT MAGIC_FOUND)
        find_library(MAGIC 5.39 magic REQUIRED)
    else()
        set(MAGIC_INCLUDE_DIRS ${MAGIC_INCLUDEDIR})
    endif()

    if (NOT IMHEX_DISABLE_STACKTRACE)
        if (WIN32)
            message(STATUS "StackWalk enabled!")
            set(LIBBACKTRACE_LIBRARIES DbgHelp.lib)
        else ()
            find_package(Backtrace)
            if (${Backtrace_FOUND})
                message(STATUS "Backtrace enabled! Header: ${Backtrace_HEADER}")

                if (Backtrace_HEADER STREQUAL "execinfo.h")
                    set(LIBBACKTRACE_LIBRARIES ${Backtrace_LIBRARY})
                    set(LIBBACKTRACE_INCLUDE_DIRS ${Backtrace_INCLUDE_DIR})
                    add_compile_definitions(BACKTRACE_HEADER=\"${Backtrace_HEADER}\")
                    add_compile_definitions(HEX_HAS_EXECINFO)
                elseif (Backtrace_HEADER STREQUAL "backtrace.h")
                    set(LIBBACKTRACE_LIBRARIES ${Backtrace_LIBRARY})
                    set(LIBBACKTRACE_INCLUDE_DIRS ${Backtrace_INCLUDE_DIR})
                    add_compile_definitions(BACKTRACE_HEADER=\"${Backtrace_HEADER}\")
                    add_compile_definitions(HEX_HAS_BACKTRACE)
                endif ()
            endif()
        endif ()
    endif ()
endmacro()
