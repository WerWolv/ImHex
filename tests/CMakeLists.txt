project(unit_tests)

add_subdirectory(common)
target_compile_definitions(tests_common PUBLIC IMHEX_PROJECT_NAME="${PROJECT_NAME}")

add_subdirectory(helpers)
add_subdirectory(algorithms)
add_subdirectory(plugins)