project(c_api)

add_library(c_api OBJECT
    source/script_api/v1/bookmarks.cpp
    source/script_api/v1/logger.cpp
    source/script_api/v1/mem.cpp
    source/script_api/v1/ui.cpp
)

target_include_directories(c_api PUBLIC
    include
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/third_party/imgui/imgui/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/third_party/imgui/backend/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/types/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/utils/lib/jthread/includes>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/io/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/trace/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/utils/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/pattern_language/lib/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/third_party/imgui/implot/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/third_party/imgui/implot3d/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/third_party/imgui/imnodes/include>
    ${FMT_INCLUDE_DIRS}
)
target_link_libraries(c_api PRIVATE libimhex ui ${FMT_LIBRARIES})
target_compile_definitions(c_api PRIVATE IMHEX_PROJECT_NAME="Script")
