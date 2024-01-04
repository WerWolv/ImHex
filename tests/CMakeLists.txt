project(unit_tests)

add_custom_target(unit_tests DEPENDS helpers algorithms)

add_subdirectory(common)
target_compile_definitions(tests_common PUBLIC IMHEX_PROJECT_NAME="${PROJECT_NAME}")

add_subdirectory(helpers)
add_subdirectory(algorithms)
