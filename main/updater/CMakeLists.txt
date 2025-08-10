project(updater) 

add_executable(updater
    source/main.cpp
)

target_compile_definitions(updater PRIVATE IMHEX_PROJECT_NAME="${PROJECT_NAME}")

target_link_libraries(updater PRIVATE libimhex ${FMT_LIBRARIES})
if (UNIX OR BSD)
    target_link_libraries(updater PRIVATE pthread)
endif()

add_dependencies(main updater)
setupCompilerFlags(updater)


set_target_properties(updater PROPERTIES
        OUTPUT_NAME "imhex-updater"
        RUNTIME_OUTPUT_DIRECTORY ${IMHEX_MAIN_OUTPUT_DIRECTORY}
)