cmake_minimum_required(VERSION 3.16)

project(imhex VERSION 1.5.0)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

option (CREATE_PACKAGE "Create a package with CPack" OFF)

if (APPLE)
    option (CREATE_BUNDLE "Create a bundle on macOS" OFF)
endif()

set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
set(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)
set(CMAKE_CXX_STANDARD 20)

# Enforce that we use non system Python 3 on macOS.
set(Python_FIND_FRAMEWORK NEVER)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_search_module(CRYPTO REQUIRED crypto)
pkg_search_module(CAPSTONE REQUIRED capstone)
find_package(OpenGL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Python COMPONENTS Development)

add_subdirectory(external/llvm/Demangle)
add_subdirectory(plugins/libimhex)


if(Python_VERSION LESS 3)
    message(STATUS ${PYTHON_VERSION_MAJOR_MINOR})
    message(FATAL_ERROR "No valid version of Python 3 was found.")
endif()

pkg_search_module(MAGIC libmagic)
if(NOT MAGIC_FOUND)
    find_library(MAGIC magic REQUIRED)
endif()

# Add include directories
include_directories(include ${CRYPTO_INCLUDE_DIRS} ${CAPSTONE_INCLUDE_DIRS} ${MAGIC_INCLUDE_DIRS} ${Python_INCLUDE_DIRS})

# Get Python major and minor
string(REPLACE "." ";" PYTHON_VERSION_MAJOR_MINOR ${Python_VERSION})
list(REMOVE_AT PYTHON_VERSION_MAJOR_MINOR 2)
list(JOIN PYTHON_VERSION_MAJOR_MINOR "." PYTHON_VERSION_MAJOR_MINOR)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS} -DPYTHON_VERSION_MAJOR_MINOR=\"\\\"${PYTHON_VERSION_MAJOR_MINOR}\"\\\"")

# Detect current OS / System
if (MSYS OR MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOS_WINDOWS")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wl,-subsystem,windows")
elseif(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOS_MACOS")
elseif(UNIX AND NOT APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOS_LINUX")
else()
    message(FATAL_ERROR "Unknown / unsupported system!")
endif()

# Detect 32 vs. 64 bit system
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DARCH_64_BIT")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DARCH_32_BIT")
endif()

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

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DRELEASE -DIMHEX_VERSION=\"\\\"${PROJECT_VERSION}\"\\\"")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -DIMHEX_VERSION=\"\\\"${PROJECT_VERSION}-Debug\"\\\"")

if (WIN32)
    set(application_type WIN32)
    set(imhex_icon resource.rc)
elseif (APPLE)
    set (imhex_icon "${PROJECT_SOURCE_DIR}/res/mac/AppIcon.icns")

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

add_executable(imhex ${application_type}
        source/main.cpp
        source/window.cpp

        source/helpers/crypto.cpp
        source/helpers/patches.cpp
        source/helpers/math_evaluator.cpp
        source/helpers/project_file_handler.cpp
        source/helpers/loader_script_handler.cpp
        source/helpers/plugin_handler.cpp

        source/lang/preprocessor.cpp
        source/lang/lexer.cpp
        source/lang/parser.cpp
        source/lang/validator.cpp
        source/lang/evaluator.cpp

        source/providers/file_provider.cpp

        source/views/view_hexeditor.cpp
        source/views/view_pattern.cpp
        source/views/view_pattern_data.cpp
        source/views/view_hashes.cpp
        source/views/view_information.cpp
        source/views/view_help.cpp
        source/views/view_tools.cpp
        source/views/view_strings.cpp
        source/views/view_data_inspector.cpp
        source/views/view_disassembler.cpp
        source/views/view_bookmarks.cpp
        source/views/view_patches.cpp
        source/views/view_command_palette.cpp

        ${imhex_icon}
        )

set_target_properties(imhex PROPERTIES CXX_VISIBILITY_PRESET hidden)

target_link_directories(imhex PRIVATE ${CRYPTO_LIBRARY_DIRS} ${CAPSTONE_LIBRARY_DIRS} ${MAGIC_LIBRARY_DIRS})

target_link_libraries(imhex magic crypto ${CMAKE_DL_LIBS} capstone LLVMDemangle imgui libimhex ${Python_LIBRARIES} nlohmann_json::nlohmann_json dl)

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