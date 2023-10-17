project(updater)

add_executable(updater
    source/main.cpp
)

add_compile_definitions(IMHEX_PROJECT_NAME="${PROJECT_NAME}")
target_link_libraries(updater PRIVATE libimhex libwolv-io ${FMT_LIBRARIES})
add_dependencies(imhex_all updater)
set_target_properties(updater PROPERTIES
        OUTPUT_NAME "updater"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../..
        CXX_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON
)