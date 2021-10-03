cmake_minimum_required(VERSION 3.16)

# Change this to the name of your plugin #
project(example_rust)

# ---- No need to change anything from here downwards unless you know what you're doing ---- #

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_CMD ${RUST_PATH}cargo build --quiet)
    set(TARGET_DIR "debug")
else ()
    set(CARGO_CMD ${RUST_PATH}cargo build --release --quiet)
    set(TARGET_DIR "release")
endif ()

if (WIN32)
    set(PLUGIN_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_DIR}/${PROJECT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
else ()
    set(PLUGIN_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif ()

get_target_property(LIBIMHEX_SOURCE_DIRECTORY libimhex SOURCE_DIR)

add_custom_target(${PROJECT_NAME} ALL
        COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR} LIBIMHEX_SOURCE_DIRECTORY=${LIBIMHEX_SOURCE_DIRECTORY} LIBIMHEX_OUTPUT_DIRECTORY=$<TARGET_FILE_DIR:libimhex> ${CARGO_CMD}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PLUGIN_OUTPUT_PATH} "${CMAKE_CURRENT_BINARY_DIR}/../${PROJECT_NAME}.hexplug"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
        USES_TERMINAL
)
set_target_properties(${PROJECT_NAME} PROPERTIES LOCATION ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(${PROJECT_NAME} PROPERTIES RUST_PROJECT 1)
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE Rust)

add_compile_definitions(IMHEX_PLUGIN_NAME=${PROJECT_NAME})

if (NOT TARGET libimhex)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libimhex ${CMAKE_CURRENT_BINARY_DIR}/plugins/libimhex)
endif()