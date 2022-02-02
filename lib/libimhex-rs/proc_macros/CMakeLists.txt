cmake_minimum_required(VERSION 3.16)

# Change this to the name of your plugin #
project(proc_macros)

# ---- No need to change anything from here downwards unless you know what you're doing ---- #

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_CMD cargo build)
    set(TARGET_DIR "debug")
else ()
    set(CARGO_CMD cargo build --release)
    set(TARGET_DIR "release")
endif ()

add_custom_target(${PROJECT_NAME} ALL
        COMMENT "Compiling client module"
        COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR} ${CARGO_CMD}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(${PROJECT_NAME} PROPERTIES LOCATION ${CMAKE_CURRENT_BINARY_DIR})

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE Rust)

if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--allow-multiple-definition -fvisibility=hidden")
endif()

add_compile_definitions(IMHEX_PROJECT_NAME="${PROJECT_NAME}")

if (NOT TARGET libimhex)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libimhex ${CMAKE_CURRENT_BINARY_DIR}/plugins/libimhex)
endif()