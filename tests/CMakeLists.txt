project(unit_tests)

add_subdirectory(common)
target_compile_definitions(tests_common PUBLIC IMHEX_PROJECT_NAME="${PROJECT_NAME}")

add_subdirectory(helpers)
add_subdirectory(algorithms)

if (IMHEX_ENABLE_PLUGIN_TESTS AND NOT IMHEX_STATIC_LINK_PLUGINS)
    add_subdirectory(plugins)
endif ()
