project(main-forwarder)

add_executable(main-forwarder
        source/main.cpp
        ${IMHEX_ICON}
)
target_link_libraries(main-forwarder PRIVATE libwolv-io ${FMT_LIBRARIES})
set_target_properties(main-forwarder PROPERTIES
        OUTPUT_NAME "imhex"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/..
        CXX_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON)