project(updater) 

add_executable(updater
    source/main.cpp
)

target_compile_definitions(updater PRIVATE IMHEX_PROJECT_NAME="${PROJECT_NAME}")
target_link_libraries(updater PRIVATE libimhex ${FMT_LIBRARIES})
add_dependencies(main updater)

if (APPLE)
    set(OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${BUNDLE_NAME}/Contents/MacOS")
else ()
    set(OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

set_target_properties(updater PROPERTIES
        OUTPUT_NAME "imhex-updater"
        RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY}
)