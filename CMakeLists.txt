cmake_minimum_required(VERSION 3.16)

project(imhex VERSION 1.5.0)

set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
set(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)
set(CMAKE_CXX_STANDARD 20)

# Enforce that we use non system Python 3 on macOS.
set(Python_FIND_FRAMEWORK NEVER)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
pkg_search_module(GLM REQUIRED glm)
pkg_search_module(CRYPTO REQUIRED libcrypto)
pkg_search_module(CAPSTONE REQUIRED capstone)
find_package(OpenGL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Python COMPONENTS Interpreter Development)

add_subdirectory(external/llvm/Demangle)
add_subdirectory(external/ImGui)
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
include_directories(include ${GLFW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS} ${CRYPTO_INCLUDE_DIRS} ${CAPSTONE_INCLUDE_DIRS} ${MAGIC_INCLUDE_DIRS} ${Python_INCLUDE_DIRS})

# Get Python major and minor
string(REPLACE "." ";" PYTHON_VERSION_MAJOR_MINOR ${Python_VERSION})
list(REMOVE_AT PYTHON_VERSION_MAJOR_MINOR 2)
list(JOIN PYTHON_VERSION_MAJOR_MINOR "." PYTHON_VERSION_MAJOR_MINOR)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS} -DPYTHON_VERSION_MAJOR_MINOR=\"\\\"${PYTHON_VERSION_MAJOR_MINOR}\"\\\"")

# Detect current OS / System
if (MSYS OR MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOS_WINDOWS -static-libstdc++ -static-libgcc -static")
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

add_executable(imhex
        source/main.cpp
        source/window.cpp

        source/helpers/utils.cpp
        source/helpers/crypto.cpp
        source/helpers/patches.cpp
        source/helpers/math_evaluator.cpp
        source/helpers/project_file_handler.cpp
        source/helpers/loader_script_handler.cpp

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

        resource.rc
        )

set_target_properties(imhex PROPERTIES CXX_VISIBILITY_PRESET hidden)

target_link_directories(imhex PRIVATE ${GLFW_LIBRARY_DIRS} ${CRYPTO_LIBRARY_DIRS} ${CAPSTONE_LIBRARY_DIRS} ${MAGIC_LIBRARY_DIRS})

if (WIN32)
    target_link_libraries(imhex libglfw3.a libgcc.a libstdc++.a libmagic.a libgnurx.a libtre.a libintl.a libiconv.a shlwapi.lib libcrypto.a libwinpthread.a libcapstone.a LLVMDemangle imgui libimhex ${Python_LIBRARIES} nlohmann_json::nlohmann_json dl)
elseif (UNIX)
    target_link_libraries(imhex glfw magic crypto ${CMAKE_DL_LIBS} capstone LLVMDemangle imgui libimhex ${Python_LIBRARIES} nlohmann_json::nlohmann_json dl)
endif()

install(TARGETS imhex DESTINATION bin)
