cmake_minimum_required(VERSION 3.16)

project(plugins_test)

add_executable(${PROJECT_NAME}
        source/plugins.cpp
)


# ---- No need to change anything from here downwards unless you know what you're doing ---- #

target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} PRIVATE libimhex tests_common ${FMT_LIBRARIES})

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_dependencies(unit_tests ${PROJECT_NAME})
