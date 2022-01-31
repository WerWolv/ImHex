project(unit_tests)

enable_testing()

add_custom_target(unit_tests)
add_subdirectory(common)

add_subdirectory(helpers)
add_subdirectory(algorithms)
add_subdirectory(pattern_language)