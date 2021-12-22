cmake_minimum_required(VERSION 3.16)
project(libimhex)

set(CMAKE_CXX_STANDARD 20)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/ImGui ${CMAKE_CURRENT_BINARY_DIR}/external/ImGui)
set_target_properties(imgui PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/microtar ${CMAKE_CURRENT_BINARY_DIR}/external/microtar EXCLUDE_FROM_ALL)
set_target_properties(microtar PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/nativefiledialog ${CMAKE_CURRENT_BINARY_DIR}/external/nativefiledialog EXCLUDE_FROM_ALL)
set_target_properties(nfd PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(LIBROMFS_RESOURCE_LOCATION ${CMAKE_SOURCE_DIR}/res/resources)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/libromfs ${CMAKE_CURRENT_BINARY_DIR}/external/libromfs EXCLUDE_FROM_ALL)
set_target_properties(libromfs PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(XDGPP_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../external/xdgpp")
set(CMAKE_USE_MBEDTLS ON)
set(BUILD_CURL_EXE OFF)
set(FPHSA_NAME_MISMATCHED ON CACHE BOOL "")

# Find packages
find_package(PkgConfig REQUIRED)

if(NOT USE_SYSTEM_NLOHMANN_JSON)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/nlohmann_json ${CMAKE_CURRENT_BINARY_DIR}/external/nlohmann_json EXCLUDE_FROM_ALL)
  set(NLOHMANN_JSON_LIBRARIES nlohmann_json)
else()
  find_package(nlohmann_json 3.10.2 REQUIRED)
  set(NLOHMANN_JSON_LIBRARIES nlohmann_json::nlohmann_json)
endif()

if(NOT USE_SYSTEM_FMT)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/fmt ${CMAKE_CURRENT_BINARY_DIR}/external/fmt EXCLUDE_FROM_ALL)
  set(FMT_LIBRARIES fmt-header-only)
else()
  find_package(fmt 8.0.0 REQUIRED)
  set(FMT_LIBRARIES fmt::fmt)
endif()

if(NOT USE_SYSTEM_CURL)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/curl ${CMAKE_CURRENT_BINARY_DIR}/external/curl EXCLUDE_FROM_ALL)
  set_target_properties(libcurl PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set(LIBCURL_LIBRARIES libcurl)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBCURL REQUIRED IMPORTED_TARGET libcurl>=7.78.0)
endif()

if (NOT USE_SYSTEM_LLVM)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/llvm ${CMAKE_CURRENT_BINARY_DIR}/external/llvm EXCLUDE_FROM_ALL)
    set_target_properties(LLVMDemangle PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    find_package(LLVM REQUIRED Demangle)
endif()

if (NOT USE_SYSTEM_YARA)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/yara ${CMAKE_CURRENT_BINARY_DIR}/external/yara EXCLUDE_FROM_ALL)
    set_target_properties(libyara PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(YARA_LIBRARIES libyara)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(YARA REQUIRED IMPORTED_TARGET yara)
endif()

if (NOT USE_SYSTEM_CAPSTONE)
    set(CAPSTONE_BUILD_STATIC_RUNTIME OFF)
    set(CAPSTONE_BUILD_SHARED OFF)
    set(CAPSTONE_BUILD_TESTS OFF)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/capstone ${CMAKE_CURRENT_BINARY_DIR}/external/capstone EXCLUDE_FROM_ALL)
    set_target_properties(capstone-static PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(CAPSTONE_LIBRARIES "capstone-static")
    set(CAPSTONE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../../external/capstone/include/capstone)
else()
    find_package(PkgConfig REQUIRED)
    pkg_search_module(CAPSTONE 4.0.2 REQUIRED capstone)
endif()



find_package(mbedTLS 2.26.0 REQUIRED)
configurePython()

pkg_search_module(MAGIC libmagic>=5.39)
if(NOT MAGIC_FOUND)
    find_library(MAGIC 5.39 magic REQUIRED)
else()
    set(MAGIC_INCLUDE_DIRS ${MAGIC_INCLUDEDIR})
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

if (WIN32)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-all-symbols")
endif()

set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(LIBIMHEX_SOURCES
    source/api/event.cpp
    source/api/imhex_api.cpp
    source/api/content_registry.cpp
    source/api/task.cpp

    source/data_processor/attribute.cpp
    source/data_processor/link.cpp
    source/data_processor/node.cpp

    source/helpers/utils.cpp
    source/helpers/paths.cpp
    source/helpers/magic.cpp
    source/helpers/shared_data.cpp
    source/helpers/crypto.cpp
    source/helpers/lang.cpp
    source/helpers/net.cpp
    source/helpers/file.cpp
    source/helpers/socket.cpp
    source/helpers/patches.cpp
    source/helpers/project_file_handler.cpp
    source/helpers/encoding_file.cpp
    source/helpers/loader_script_handler.cpp

    source/pattern_language/pattern_language.cpp
    source/pattern_language/preprocessor.cpp
    source/pattern_language/lexer.cpp
    source/pattern_language/parser.cpp
    source/pattern_language/validator.cpp
    source/pattern_language/evaluator.cpp
    source/pattern_language/log_console.cpp

    source/providers/provider.cpp

    source/views/view.cpp

        )

if (APPLE)
    set(OSX_11_0_SDK_PATH  /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.0.sdk)
    if (NOT CMAKE_OSX_SYSROOT)
        if (IS_DIRECTORY ${OSX_11_0_SDK_PATH})
            set(CMAKE_OSX_SYSROOT ${OSX_11_0_SDK_PATH})
        else ()
            message(WARNING "CMAKE_OSX_SYSROOT not set and macOS 10.9 SDK not found! Using default one.")
        endif ()
    endif ()

    set(LIBIMHEX_SOURCES ${LIBIMHEX_SOURCES} source/helpers/paths_mac.mm)
endif ()

add_library(libimhex SHARED ${LIBIMHEX_SOURCES})
set_target_properties(libimhex PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(libimhex PUBLIC include ${XDGPP_INCLUDE_DIRS} ${MBEDTLS_INCLUDE_DIRS} ${CAPSTONE_INCLUDE_DIRS} ${MAGIC_INCLUDE_DIRS} ${Python_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS} ${FMT_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${YARA_INCLUDE_DIRS})
target_link_directories(libimhex PUBLIC ${MBEDTLS_LIBRARY_DIR} ${CAPSTONE_LIBRARY_DIRS} ${MAGIC_LIBRARY_DIRS})

if (APPLE)
    find_library(FOUNDATION NAMES Foundation)
    target_link_libraries(libimhex PUBLIC ${FOUNDATION})
endif ()

target_link_libraries(libimhex PUBLIC imgui nfd magic ${CAPSTONE_LIBRARIES} LLVMDemangle microtar ${NLOHMANN_JSON_LIBRARIES} ${YARA_LIBRARIES} ${LIBCURL_LIBRARIES} ${MBEDTLS_LIBRARIES} ${FMT_LIBRARIES} ${Python_LIBRARIES} libromfs)
