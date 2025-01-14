project(${IMHEX_PLUGIN_NAME}_tests)

# Add new tests here #
set(AVAILABLE_TESTS
    Providers/ReadWrite
    Providers/InvalidResize
)

add_library(${PROJECT_NAME} OBJECT
    source/main.cpp
)

# link into plugins_test, so that test registration works
target_link_libraries(plugins_test PUBLIC ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/plugins/builtin/include)

target_link_libraries(${PROJECT_NAME} PRIVATE libimhex)

foreach (test IN LISTS AVAILABLE_TESTS)
    add_test(NAME "Plugin_${IMHEX_PLUGIN_NAME}/${test}" COMMAND $<TARGET_FILE:plugins_test> "${test}" WORKING_DIRECTORY ${IMHEX_MAIN_OUTPUT_DIRECTORY})
endforeach ()

# remember to add dependency on the plugin itself to the unit_test target, so that it also gets built
add_dependencies(unit_tests ${IMHEX_PLUGIN_NAME})
target_compile_definitions(${PROJECT_NAME} PRIVATE IMHEX_PROJECT_NAME="${IMHEX_PLUGIN_NAME}-tests")