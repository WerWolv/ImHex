project(unit_tests)

add_subdirectory(pattern_language)
add_subdirectory(algorithms)

add_custom_target(unit_tests
        DEPENDS pattern_language_tests algorithms_test
)
