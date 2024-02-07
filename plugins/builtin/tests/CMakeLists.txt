project(${IMHEX_PLUGIN_NAME}_tests)

# Add new tests here #
set(AVAILABLE_TESTS
    Providers/ReadWrite
)

add_library(${PROJECT_NAME} SHARED
    source/main.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/plugins/builtin/include)

target_link_libraries(${PROJECT_NAME} PRIVATE libimhex)

foreach (test IN LISTS AVAILABLE_TESTS)
    add_test(NAME "Plugin_${IMHEX_PLUGIN_NAME}/${test}" COMMAND $<TARGET_FILE:plugins_test> "${test}" WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach ()