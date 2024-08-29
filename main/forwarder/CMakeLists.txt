project(main-forwarder)

add_executable(main-forwarder
        source/main.cpp
        ${IMHEX_ICON}
)

target_link_libraries(main-forwarder PRIVATE wolv::io ${FMT_LIBRARIES})
add_dependencies(imhex_all main-forwarder)
set_target_properties(main-forwarder PROPERTIES
        OUTPUT_NAME "imhex"
        RUNTIME_OUTPUT_DIRECTORY ${IMHEX_MAIN_OUTPUT_DIRECTORY}
)