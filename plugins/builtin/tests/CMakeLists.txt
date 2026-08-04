project(${IMHEX_PLUGIN_NAME}_tests)

# Add new tests here #
set(AVAILABLE_TESTS
    Providers/ReadWrite
    Providers/InvalidResize
)

add_library(${PROJECT_NAME} OBJECT
    source/main.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/plugins/builtin/include)

target_link_libraries(${PROJECT_NAME} PRIVATE libimhex)

foreach (test IN LISTS AVAILABLE_TESTS)
    add_test(NAME "Plugin_${IMHEX_PLUGIN_NAME}/${test}" COMMAND $<TARGET_FILE:plugins_test> "${test}" WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties("Plugin_${IMHEX_PLUGIN_NAME}/${test}" PROPERTIES
        ENVIRONMENT "IMHEX_TEST_PLUGIN_PATH=$<TARGET_FILE_DIR:${IMHEX_PLUGIN_NAME}>"
    )
endforeach ()
