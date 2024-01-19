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

    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -DPROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR} -DPROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR} -DPROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH} ")

    set(IMHEX_VERSION_STRING ${IMHEX_VERSION})
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set(IMHEX_VERSION_STRING ${IMHEX_VERSION_STRING})
        add_compile_definitions(NDEBUG)
    elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(IMHEX_VERSION_STRING ${IMHEX_VERSION_STRING}-Debug)
        add_compile_definitions(DEBUG _GLIBCXX_DEBUG _GLIBCXX_VERBOSE)
    elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(IMHEX_VERSION_STRING ${IMHEX_VERSION_STRING})
        add_compile_definitions(NDEBUG)
    elseif (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        set(IMHEX_VERSION_STRING ${IMHEX_VERSION_STRING}-MinSizeRel)
        add_compile_definitions(NDEBUG)
    endif ()

    if (IMHEX_STATIC_LINK_PLUGINS)
        add_compile_definitions(IMHEX_STATIC_LINK_PLUGINS)
    endif ()
endmacro()

function(addDefineToSource SOURCE DEFINE)
    set_property(
            SOURCE ${SOURCE}
            APPEND
            PROPERTY COMPILE_DEFINITIONS "${DEFINE}"
    )
endfunction()

# Detect current OS / System
macro(detectOS)
    if (WIN32)
        add_compile_definitions(OS_WINDOWS)
        set(CMAKE_INSTALL_BINDIR ".")
        set(CMAKE_INSTALL_LIBDIR ".")
        set(PLUGINS_INSTALL_LOCATION "plugins")
    elseif (APPLE)
        add_compile_definitions(OS_MACOS)
        set(CMAKE_INSTALL_BINDIR ".")
        set(CMAKE_INSTALL_LIBDIR ".")
        set(PLUGINS_INSTALL_LOCATION "plugins")
        enable_language(OBJC)
        enable_language(OBJCXX)
    elseif (EMSCRIPTEN)
        add_compile_definitions(OS_WEB)
    elseif (UNIX AND NOT APPLE)
        add_compile_definitions(OS_LINUX)
        include(GNUInstallDirs)

        if(IMHEX_PLUGINS_IN_SHARE)
            set(PLUGINS_INSTALL_LOCATION "share/imhex/plugins")
        else()
            set(PLUGINS_INSTALL_LOCATION "${CMAKE_INSTALL_LIBDIR}/imhex/plugins")

            # Add System plugin location for plugins to be loaded from
            # IMPORTANT: This does not work for Sandboxed or portable builds such as the Flatpak or AppImage release
            add_compile_definitions(SYSTEM_PLUGINS_LOCATION="${CMAKE_INSTALL_FULL_LIBDIR}/imhex")
        endif()

    else ()
        message(FATAL_ERROR "Unknown / unsupported system!")
    endif()

endmacro()

macro(configurePackingResources)
    if (WIN32)
        if (NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
            set(APPLICATION_TYPE WIN32)
        endif ()

        set(IMHEX_ICON "${IMHEX_BASE_FOLDER}/resources/resource.rc")

        if (IMHEX_GENERATE_PACKAGE)
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
    elseif (APPLE OR ${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
        set(IMHEX_ICON "${IMHEX_BASE_FOLDER}/resources/dist/macos/AppIcon.icns")
        set(BUNDLE_NAME "imhex.app")

        if (IMHEX_GENERATE_PACKAGE)
            set(APPLICATION_TYPE MACOSX_BUNDLE)
            set_source_files_properties(${IMHEX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
            set(MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
            set(MACOSX_BUNDLE_INFO_STRING "WerWolv")
            set(MACOSX_BUNDLE_BUNDLE_NAME "ImHex")
            set(MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/dist/macos/Info.plist.in")
            set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
            set(MACOSX_BUNDLE_GUI_IDENTIFIER "net.WerWolv.ImHex")
            set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}-${IMHEX_COMMIT_HASH_SHORT}")
            set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

            string(TIMESTAMP CURR_YEAR "%Y")
            set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2020 - ${CURR_YEAR} WerWolv. All rights reserved." )
            if ("${CMAKE_GENERATOR}" STREQUAL "Xcode")
                set (IMHEX_BUNDLE_PATH "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${BUNDLE_NAME}")
            else ()
                set (IMHEX_BUNDLE_PATH "${CMAKE_BINARY_DIR}/${BUNDLE_NAME}")
            endif()

            set(PLUGINS_INSTALL_LOCATION "${IMHEX_BUNDLE_PATH}/Contents/MacOS/plugins")
            set(CMAKE_INSTALL_LIBDIR "${IMHEX_BUNDLE_PATH}/Contents/Frameworks")
        endif()
    endif()
endmacro()

macro(createPackage)
    set(LIBRARY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

    file(MAKE_DIRECTORY "plugins")
    foreach (plugin IN LISTS PLUGINS)
        add_subdirectory("plugins/${plugin}")
        if (TARGET ${plugin})
            set_target_properties(${plugin} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
            set_target_properties(${plugin} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

            if (APPLE)
                if (IMHEX_GENERATE_PACKAGE)
                    set_target_properties(${plugin} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PLUGINS_INSTALL_LOCATION})
                else ()
                    set_target_properties(${plugin} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
                endif ()
            else ()
                if (WIN32)
                    get_target_property(target_type ${plugin} TYPE)
                    if (target_type STREQUAL "SHARED_LIBRARY")
                        install(TARGETS ${plugin} RUNTIME DESTINATION ${PLUGINS_INSTALL_LOCATION})
                    else ()
                        install(TARGETS ${plugin} LIBRARY DESTINATION ${PLUGINS_INSTALL_LOCATION})
                    endif()
                else()
                    install(TARGETS ${plugin} LIBRARY DESTINATION ${PLUGINS_INSTALL_LOCATION})
                endif()

            endif()

            add_dependencies(imhex_all ${plugin})
        endif ()
    endforeach()

    set_target_properties(libimhex PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

    if (WIN32)
        # Install binaries directly in the prefix, usually C:\Program Files\ImHex.
        set(CMAKE_INSTALL_BINDIR ".")

        set(PLUGIN_TARGET_FILES "")
        foreach (plugin IN LISTS PLUGINS)
            list(APPEND PLUGIN_TARGET_FILES "$<TARGET_FILE:${plugin}>")
        endforeach ()

        # Grab all dynamically linked dependencies.
        install(CODE "set(CMAKE_INSTALL_BINDIR \"${CMAKE_INSTALL_BINDIR}\")")
        install(CODE "set(PLUGIN_TARGET_FILES \"${PLUGIN_TARGET_FILES}\")")
        install(CODE [[
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES ${PLUGIN_TARGET_FILES} $<TARGET_FILE:libimhex> $<TARGET_FILE:main>
            RESOLVED_DEPENDENCIES_VAR _r_deps
            UNRESOLVED_DEPENDENCIES_VAR _u_deps
            CONFLICTING_DEPENDENCIES_PREFIX _c_deps
            DIRECTORIES ${DEP_FOLDERS} $ENV{PATH}
            POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
        )

        if(_c_deps_FILENAMES)
            message(WARNING "Conflicting dependencies for library: \"${_c_deps}\"!")
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

    if (APPLE)
        if (IMHEX_GENERATE_PACKAGE)
            include(PostprocessBundle)

            set_target_properties(libimhex PROPERTIES SOVERSION ${IMHEX_VERSION})

            set_property(TARGET main PROPERTY MACOSX_BUNDLE_INFO_PLIST ${MACOSX_BUNDLE_INFO_PLIST})

            # Fix rpath
            add_custom_command(TARGET imhex_all POST_BUILD COMMAND ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "@executable_path/../Frameworks/" $<TARGET_FILE:main>)

            add_custom_target(build-time-make-plugins-directory ALL COMMAND ${CMAKE_COMMAND} -E make_directory "${IMHEX_BUNDLE_PATH}/Contents/MacOS/plugins")
            add_custom_target(build-time-make-resources-directory ALL COMMAND ${CMAKE_COMMAND} -E make_directory "${IMHEX_BUNDLE_PATH}/Contents/Resources")

            downloadImHexPatternsFiles("${IMHEX_BUNDLE_PATH}/Contents/MacOS")

            install(FILES ${IMHEX_ICON} DESTINATION "${IMHEX_BUNDLE_PATH}/Contents/Resources")
            install(TARGETS main BUNDLE DESTINATION ".")

            # Update library references to make the bundle portable
            postprocess_bundle(imhex_all main)

            # Enforce DragNDrop packaging.
            set(CPACK_GENERATOR "DragNDrop")

            set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/resources/dist/macos/AppIcon.icns")
            set(CPACK_BUNDLE_PLIST "${CMAKE_BINARY_DIR}/${BUNDLE_NAME}/Contents/Info.plist")
        endif()
    else()
        install(TARGETS main RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        if (TARGET updater)
            install(TARGETS updater RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        endif()
        if (TARGET main-forwarder)
            install(TARGETS main-forwarder BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR})
        endif()
    endif()

    if (IMHEX_GENERATE_PACKAGE)
        set(CPACK_BUNDLE_NAME "ImHex")

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

macro(configureCMake)
    message(STATUS "Configuring ImHex v${IMHEX_VERSION}")

    # Enable C and C++ languages
    enable_language(C CXX)

    # Configure use of recommended build tools
    if (IMHEX_USE_DEFAULT_BUILD_SETTINGS)
        message(STATUS "Configuring CMake to use recommended build tools...")

        find_program(CCACHE_PATH ccache)
        find_program(NINJA_PATH ninja)
        find_program(LD_LLD_PATH ld.lld)
        find_program(AR_LLVMLIBS_PATH llvm-ar)
        find_program(RANLIB_LLVMLIBS_PATH llvm-ranlib)

        if (CCACHE_PATH)
            set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PATH})
            set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PATH})
        else ()
            message(WARNING "ccache not found!")
        endif ()

        if (AR_LLVMLIBS_PATH)
            set(CMAKE_AR ${AR_LLVMLIBS_PATH})
        else ()
            message(WARNING "llvm-ar not found, using default ar!")
        endif ()

        if (RANLIB_LLVMLIBS_PATH)
            set(CMAKE_RANLIB ${RANLIB_LLVMLIBS_PATH})
        else ()
            message(WARNING "llvm-ranlib not found, using default ranlib!")
        endif ()

        if (LD_LLD_PATH)
            set(CMAKE_LINKER ${LD_LLD_PATH})
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fuse-ld=lld")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=lld")
        else ()
            message(WARNING "lld not found, using default linker!")
        endif ()

        if (NINJA_PATH)
            set(CMAKE_GENERATOR Ninja)
        else ()
            message(WARNING "ninja not found, using default generator!")
        endif ()
    endif()

    # Enable LTO if desired and supported
    if (IMHEX_ENABLE_LTO)
        include(CheckIPOSupported)

        check_ipo_supported(RESULT result OUTPUT output_error)
        if (result)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
            message(STATUS "LTO enabled!")
        else ()
            message(WARNING "LTO is not supported: ${output_error}")
        endif ()
    endif ()

    # Some libraries we use set the BUILD_SHARED_LIBS variable to ON, which causes CMake to
    # display a warning about options being set using set() instead of option().
    # Explicitly set the policy to NEW to suppress the warning.
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

    set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)

    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Disable deprecated warnings" FORCE)
endmacro()

macro(setDefaultBuiltTypeIfUnset)
    if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Using RelWithDebInfo build type as it was left unset" FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "RelWithDebInfo")
    endif()
endmacro()

function(loadVersion version)
    set(VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${VERSION_FILE})
    file(READ "${VERSION_FILE}" read_version)
    string(STRIP ${read_version} read_version)
    set(${version} ${read_version} PARENT_SCOPE)
endfunction()

function(detectBadClone)
    if (IMHEX_IGNORE_BAD_CLONE)
        return()
    endif()

    file (GLOB EXTERNAL_DIRS "lib/external/*" "lib/third_party/*")
    foreach (EXTERNAL_DIR ${EXTERNAL_DIRS})
        file(GLOB_RECURSE RESULT "${EXTERNAL_DIR}/*")
        list(LENGTH RESULT ENTRY_COUNT)
        if(ENTRY_COUNT LESS_EQUAL 1)
            message(FATAL_ERROR "External dependency ${EXTERNAL_DIR} is empty!\nMake sure to correctly clone ImHex using the --recurse-submodules git option or initialize the submodules manually.")
        endif()
    endforeach ()
endfunction()

function(verifyCompiler)
    if (IMHEX_IGNORE_BAD_COMPILER)
        return()
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12.0.0")
        message(FATAL_ERROR "ImHex requires GCC 12.0.0 or newer. Please use the latest GCC version.")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "17.0.0")
        message(FATAL_ERROR "ImHex requires Clang 17.0.0 or newer. Please use the latest Clang version.")
    elseif (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
        message(FATAL_ERROR "ImHex can only be compiled with GCC or Clang. ${CMAKE_CXX_COMPILER_ID} is not supported.")
    endif()
endfunction()

macro(detectBundledPlugins)
    file(GLOB PLUGINS_DIRS "plugins/*")

    if (NOT DEFINED IMHEX_INCLUDE_PLUGINS)
        foreach(PLUGIN_DIR ${PLUGINS_DIRS})
            if (EXISTS "${PLUGIN_DIR}/CMakeLists.txt")
                get_filename_component(PLUGIN_NAME ${PLUGIN_DIR} NAME)
                if (NOT (${PLUGIN_NAME} IN_LIST IMHEX_EXCLUDE_PLUGINS))
                    list(APPEND PLUGINS ${PLUGIN_NAME})
                endif ()
            endif()
        endforeach()
    else()
        set(PLUGINS ${IMHEX_INCLUDE_PLUGINS})
    endif()

    foreach(PLUGIN_NAME ${PLUGINS})
        message(STATUS "Enabled bundled plugin '${PLUGIN_NAME}'")
    endforeach()

    if (NOT PLUGINS)
        message(FATAL_ERROR "No bundled plugins enabled")
    endif()

    if (NOT ("builtin" IN_LIST PLUGINS))
        message(FATAL_ERROR "The 'builtin' plugin is required for ImHex to work!")
    endif ()
endmacro()

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
                GIT_TAG origin/master
        )

        message(STATUS "Downloading ImHex-Patterns repo branch ${PATTERNS_BRANCH}...")
        FetchContent_MakeAvailable(imhex_patterns)
        message(STATUS "Finished downloading ImHex-Patterns")

    else ()
        # Maybe patterns are cloned to a subdirectory
        set(imhex_patterns_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ImHex-Patterns")
    endif ()

    if (EXISTS ${imhex_patterns_SOURCE_DIR})
        set(PATTERNS_FOLDERS_TO_INSTALL constants encodings includes patterns magic nodes)
        foreach (FOLDER ${PATTERNS_FOLDERS_TO_INSTALL})
            install(DIRECTORY "${imhex_patterns_SOURCE_DIR}/${FOLDER}" DESTINATION ${dest} PATTERN "**/_schema.json" EXCLUDE)
        endforeach ()
    endif ()

endfunction()

macro(setupCompilerFlags target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

        # Define strict compilation flags
        if (IMHEX_STRICT_WARNINGS)
            set(IMHEX_COMMON_FLAGS "${IMHEX_COMMON_FLAGS} -Wall -Wextra -Wpedantic -Werror")
        endif()

        if (UNIX AND NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            set(IMHEX_COMMON_FLAGS "${IMHEX_COMMON_FLAGS} -rdynamic")
        endif()

        set(IMHEX_CXX_FLAGS "-fexceptions -frtti")

        # Disable some warnings
        set(IMHEX_C_CXX_FLAGS "-Wno-unknown-warning-option -Wno-array-bounds -Wno-deprecated-declarations")

        if (IMHEX_ENABLE_UNITY_BUILD AND WIN32)
            set(IMHEX_COMMON_FLAGS "${IMHEX_COMMON_FLAGS} -Wa,-mbig-obj")
        endif ()
    endif()

    # Disable some warnings for gcc
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(IMHEX_C_CXX_FLAGS "${IMHEX_C_CXX_FLAGS} -Wno-restrict -Wno-stringop-overread -Wno-stringop-overflow -Wno-dangling-reference")
    endif()

    # Define emscripten-specific disabled warnings
    if (EMSCRIPTEN)
        set(IMHEX_C_CXX_FLAGS "${IMHEX_C_CXX_FLAGS} -pthread -Wno-dollar-in-identifier-extension -Wno-pthreads-mem-growth")
    endif ()

    # Set actual CMake flags
    set_target_properties(${target} PROPERTIES COMPILE_FLAGS "${IMHEX_COMMON_FLAGS} ${IMHEX_C_CXX_FLAGS}")
    set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS}    ${IMHEX_COMMON_FLAGS} ${IMHEX_C_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS}  ${IMHEX_COMMON_FLAGS} ${IMHEX_C_CXX_FLAGS}  ${IMHEX_CXX_FLAGS}")
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

macro(addBundledLibraries)
    find_package(PkgConfig REQUIRED)

    set(EXTERNAL_LIBS_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/lib/external")
    set(THIRD_PARTY_LIBS_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/lib/third_party")

    set(BUILD_SHARED_LIBS OFF)
    add_subdirectory(${THIRD_PARTY_LIBS_FOLDER}/imgui)

    add_subdirectory(${THIRD_PARTY_LIBS_FOLDER}/microtar EXCLUDE_FROM_ALL)
    set_target_properties(microtar PROPERTIES POSITION_INDEPENDENT_CODE ON)

    add_subdirectory(${EXTERNAL_LIBS_FOLDER}/libwolv EXCLUDE_FROM_ALL)
    set_property(TARGET libwolv-types       PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET libwolv-utils       PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET libwolv-io          PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET libwolv-hash        PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET libwolv-containers  PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET libwolv-net         PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET libwolv-math_eval   PROPERTY POSITION_INDEPENDENT_CODE ON)

    set(XDGPP_INCLUDE_DIRS "${THIRD_PARTY_LIBS_FOLDER}/xdgpp")
    set(FPHSA_NAME_MISMATCHED ON CACHE BOOL "")

    find_package(PkgConfig REQUIRED)

    if(NOT USE_SYSTEM_FMT)
        add_subdirectory(${THIRD_PARTY_LIBS_FOLDER}/fmt EXCLUDE_FROM_ALL)
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

    if (NOT EMSCRIPTEN)
        # curl
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(LIBCURL REQUIRED IMPORTED_TARGET libcurl>=7.60.0)

        # nfd
        if (NOT USE_SYSTEM_NFD)
            add_subdirectory(${THIRD_PARTY_LIBS_FOLDER}/nativefiledialog EXCLUDE_FROM_ALL)
            set_target_properties(nfd PROPERTIES POSITION_INDEPENDENT_CODE ON)
            set(NFD_LIBRARIES nfd)
        else()
            find_package(nfd)
            set(NFD_LIBRARIES nfd)
        endif()
    endif()

    if(NOT USE_SYSTEM_NLOHMANN_JSON)
        add_subdirectory(${THIRD_PARTY_LIBS_FOLDER}/nlohmann_json EXCLUDE_FROM_ALL)
        set(NLOHMANN_JSON_LIBRARIES nlohmann_json)
    else()
        find_package(nlohmann_json 3.10.2 REQUIRED)
        set(NLOHMANN_JSON_LIBRARIES nlohmann_json::nlohmann_json)
    endif()

    if (NOT USE_SYSTEM_LLVM)
        add_subdirectory(${THIRD_PARTY_LIBS_FOLDER}/llvm-demangle EXCLUDE_FROM_ALL)
        set_target_properties(LLVMDemangle PROPERTIES POSITION_INDEPENDENT_CODE ON)
    else()
        find_package(LLVM REQUIRED Demangle)
    endif()

    if (NOT USE_SYSTEM_JTHREAD)
        add_subdirectory(${THIRD_PARTY_LIBS_FOLDER}/jthread EXCLUDE_FROM_ALL)
        set(JTHREAD_LIBRARIES jthread)
    else()
        find_path(JOSUTTIS_JTHREAD_INCLUDE_DIRS "condition_variable_any2.hpp")
        include_directories(${JOSUTTIS_JTHREAD_INCLUDE_DIRS})

        add_library(jthread INTERFACE)
        target_include_directories(jthread INTERFACE ${JOSUTTIS_JTHREAD_INCLUDE_DIRS})
        set(JTHREAD_LIBRARIES jthread)
    endif()

    set(LIBPL_BUILD_CLI_AS_EXECUTABLE OFF)
    add_subdirectory(${EXTERNAL_LIBS_FOLDER}/pattern_language EXCLUDE_FROM_ALL)
    set_target_properties(libpl PROPERTIES POSITION_INDEPENDENT_CODE ON)

    find_package(mbedTLS 3.4.0 REQUIRED)

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

function(enableUnityBuild TARGET)
    if (IMHEX_ENABLE_UNITY_BUILD)
        set_target_properties(${TARGET} PROPERTIES UNITY_BUILD ON UNITY_BUILD_MODE BATCH)
    endif ()
endfunction()

function(generatePDBs)
    if (NOT IMHEX_GENERATE_PDBS)
        return()
    endif ()

    if (NOT WIN32 OR CMAKE_BUILD_TYPE STREQUAL "Debug")
        return()
    endif ()

    include(FetchContent)
    FetchContent_Declare(
            cv2pdb
            URL "https://github.com/rainers/cv2pdb/releases/download/v0.52/cv2pdb-0.52.zip"
            DOWNLOAD_EXTRACT_TIMESTAMP ON
    )
    FetchContent_Populate(cv2pdb)

    set(PDBS_TO_GENERATE main main-forwarder libimhex ${PLUGINS})
    foreach (PDB ${PDBS_TO_GENERATE})
        if (PDB STREQUAL "main")
            set(GENERATED_PDB imhex)
        elseif (PDB STREQUAL "main-forwarder")
            set(GENERATED_PDB imhex-gui)
        elseif (PDB STREQUAL "libimhex")
            set(GENERATED_PDB libimhex)
        else ()
            set(GENERATED_PDB plugins/${PDB})
        endif ()

        add_custom_target(${PDB}_pdb DEPENDS ${CMAKE_BINARY_DIR}/${GENERATED_PDB}.pdb)
        add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/${GENERATED_PDB}.pdb
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMAND
                (
                    ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/${GENERATED_PDB}.pdb &&
                    ${cv2pdb_SOURCE_DIR}/cv2pdb64.exe $<TARGET_FILE:${PDB}> ${CMAKE_BINARY_DIR}/${GENERATED_PDB} &&
                    ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/${GENERATED_PDB}
                ) || (exit 0)
                COMMAND_EXPAND_LISTS)

        install(FILES ${CMAKE_BINARY_DIR}/${GENERATED_PDB}.pdb DESTINATION ".")

        add_dependencies(imhex_all ${PDB}_pdb)
    endforeach ()

endfunction()

function(generateSDKDirectory)
    if (WIN32)
        set(SDK_PATH "./sdk")
    elseif (APPLE)
        set(SDK_PATH "${BUNDLE_NAME}/Contents/Resources/sdk")
    else()
        set(SDK_PATH "share/imhex/sdk")
    endif()

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/libimhex DESTINATION "${SDK_PATH}/lib")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/external DESTINATION "${SDK_PATH}/lib")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/third_party/imgui DESTINATION "${SDK_PATH}/lib/third_party")
    if (NOT USE_SYSTEM_FMT)
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/third_party/fmt DESTINATION "${SDK_PATH}/lib/third_party")
    endif()
    if (NOT USE_SYSTEM_NLOHMANN_JSON)
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/third_party/nlohmann_json DESTINATION "${SDK_PATH}/lib/third_party")
    endif()

    install(FILES ${CMAKE_SOURCE_DIR}/cmake/modules/ImHexPlugin.cmake DESTINATION "${SDK_PATH}/cmake/modules")
    install(FILES ${CMAKE_SOURCE_DIR}/cmake/build_helpers.cmake DESTINATION "${SDK_PATH}/cmake")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/cmake/sdk/ DESTINATION "${SDK_PATH}")
    install(TARGETS libimhex ARCHIVE DESTINATION "${SDK_PATH}/lib")
    install(TARGETS libimhex RUNTIME DESTINATION "${SDK_PATH}/lib")
    install(TARGETS libimhex LIBRARY DESTINATION "${SDK_PATH}/lib")
endfunction()

function(addIncludesFromLibrary target library)
    get_target_property(library_include_dirs ${library} INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(${target} PRIVATE ${library_include_dirs})
endfunction()