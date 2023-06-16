set(CoreClrEmbed_FOUND FALSE)

set(CORECLR_ARCH "linux-x64")
set(CORECLR_SUBARCH "x64")
if (WIN32)
    set(CORECLR_ARCH "win-x64")
endif()
if (UNIX)
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(CORECLR_ARCH "linux-arm64")
        set(CORECLR_SUBARCH "arm64")
    endif()
endif()

if (NOT DOTNET_EXECUTABLE)
    set(DOTNET_EXECUTABLE dotnet)
endif ()

set(CORECLR_VERSION "7.0")

execute_process(COMMAND ${DOTNET_EXECUTABLE} "--list-runtimes" OUTPUT_VARIABLE CORECLR_LIST_RUNTIMES_OUTPUT OUTPUT_STRIP_TRAILING_WHITESPACE)
if (CORECLR_LIST_RUNTIMES_OUTPUT STREQUAL "")
    message(STATUS "[.Net]: Unable to find any .Net runtimes")
    return()
endif ()

message(STATUS "[.Net]: CORECLR_LIST_RUNTIMES_OUTPUT = ${CORECLR_LIST_RUNTIMES_OUTPUT}")
set(_ALL_RUNTIMES ${CORECLR_LIST_RUNTIMES_OUTPUT})
string(REPLACE "\n" ";" _ALL_RUNTIMES_LIST ${_ALL_RUNTIMES})

foreach(X ${_ALL_RUNTIMES_LIST})
    string(REGEX MATCH "Microsoft\.NETCore\.App ([0-9]+)\.([0-9]+)\.([a-zA-Z0-9.-]+) [\[](.*)Microsoft\.NETCore\.App[\]]"
    CORECLR_VERSION_REGEX_MATCH ${X})

    set(_RUNTIME_VERSION ${CMAKE_MATCH_1}.${CMAKE_MATCH_2})

    if (CMAKE_MATCH_1 AND CMAKE_MATCH_4)
        if (${_RUNTIME_VERSION} STREQUAL ${CORECLR_VERSION})
            set(CORECLR_RUNTIME_VERSION ${_RUNTIME_VERSION})
            set(CORECLR_RUNTIME_VERSION_FULL ${CORECLR_VERSION}.${CMAKE_MATCH_3})
            set(CORECLR_RUNTIME_ROOT_PATH ${CMAKE_MATCH_4})
            message(STATUS "[.Net]: Found matching runtime version '${CORECLR_RUNTIME_VERSION_FULL}' path='${CORECLR_RUNTIME_ROOT_PATH}'")
        endif()
    endif()
endforeach()

if (CORECLR_RUNTIME_ROOT_PATH)
    get_filename_component(CORECLR_RUNTIME_ROOT_PATH ${CORECLR_RUNTIME_ROOT_PATH} DIRECTORY)
endif()
set(CoreClrEmbed_ROOT_PATH "${CORECLR_RUNTIME_ROOT_PATH}")

message(STATUS "[.Net]: CORECLR_RUNTIME_VERSION = '${CORECLR_RUNTIME_VERSION}'")
message(STATUS "[.Net]: CORECLR_RUNTIME_VERSION_FULL = '${CORECLR_RUNTIME_VERSION_FULL}'")
message(STATUS "[.Net]: CORECLR_RUNTIME_ROOT_PATH = '${CORECLR_RUNTIME_ROOT_PATH}'")

message(STATUS "[.Net]: CORECLR_SUBARCH = '${CORECLR_SUBARCH}'")


file(GLOB _CORECLR_HOST_ARCH_PATH "${CORECLR_RUNTIME_ROOT_PATH}/packs/Microsoft.NETCore.App.Host.*-${CORECLR_SUBARCH}")
message(STATUS "[CoreClrEmbed] _CORECLR_HOST_ARCH_PATH = '${_CORECLR_HOST_ARCH_PATH}'")
if (_CORECLR_HOST_ARCH_PATH)
    get_filename_component(_CORECLR_HOST_ARCH_FILENAME ${_CORECLR_HOST_ARCH_PATH} NAME)
    message(STATUS "[CoreClrEmbed] _CORECLR_HOST_ARCH_FILENAME = '${_CORECLR_HOST_ARCH_FILENAME}'")
    string(REPLACE "Microsoft.NETCore.App.Host." "" _CORECLR_COMPUTED_ARCH "${_CORECLR_HOST_ARCH_FILENAME}")
    message(STATUS "[CoreClrEmbed] _CORECLR_COMPUTED_ARCH = '${_CORECLR_COMPUTED_ARCH}'")
    if (_CORECLR_COMPUTED_ARCH)
        set(CORECLR_ARCH "${_CORECLR_COMPUTED_ARCH}")
        message(STATUS "[CoreClrEmbed] Set CORECLR_ARCH to '${_CORECLR_COMPUTED_ARCH}'")
    endif()
endif()

set(CORECLR_HOST_BASE_PATH "${CORECLR_RUNTIME_ROOT_PATH}/packs/Microsoft.NETCore.App.Host.${CORECLR_ARCH}/${CORECLR_RUNTIME_VERSION_FULL}")
message(STATUS "[CoreClrEmbed] searching runtime path '${CORECLR_HOST_BASE_PATH}'")
file(GLOB _CORECLR_FOUND_PATH ${CORECLR_HOST_BASE_PATH})
message(STATUS "[CoreClrEmbed] _CORECLR_FOUND_PATH = ${_CORECLR_FOUND_PATH}")
if (_CORECLR_FOUND_PATH)
    set(CORECLR_NETHOST_ROOT "${_CORECLR_FOUND_PATH}/runtimes/${CORECLR_ARCH}/native")
endif()

find_library(CoreClrEmbed_LIBRARY nethost PATHS
    ${CORECLR_NETHOST_ROOT}
)
find_path(CoreClrEmbed_INCLUDE_DIR nethost.h PATHS
    ${CORECLR_NETHOST_ROOT}
)
find_file(CoreClrEmbed_SHARED_LIBRARY nethost.dll nethost.so libnethost.so nethost.dylib libnethost.dylib PATHS
    ${CORECLR_NETHOST_ROOT})

message(STATUS "[CoreClrEmbed] CoreClrEmbed_LIBRARY = ${CoreClrEmbed_LIBRARY}")
message(STATUS "[CoreClrEmbed] CoreClrEmbed_INCLUDE_DIR = ${CoreClrEmbed_INCLUDE_DIR}")

if (CoreClrEmbed_INCLUDE_DIR AND CoreClrEmbed_LIBRARY)
    set(CoreClrEmbed_FOUND TRUE)
    set(CoreClrEmbed_LIBRARIES "${CoreClrEmbed_LIBRARY}")
    set(CoreClrEmbed_SHARED_LIBRARIES "${CoreClrEmbed_SHARED_LIBRARY}")
    set(CoreClrEmbed_INCLUDE_DIRS "${CoreClrEmbed_INCLUDE_DIR}")
endif()