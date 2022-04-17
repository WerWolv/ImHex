project(unit_tests)

add_compile_definitions(IMHEX_PROJECT_NAME="${PROJECT_NAME}")

add_custom_target(unit_tests DEPENDS helpers algorithms)
add_subdirectory(common)

add_subdirectory(helpers)
add_subdirectory(algorithms)
