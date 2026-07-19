project(updater) 

add_executable(updater
    source/main.cpp
)

find_package(mbedTLS 3.6.0 REQUIRED)

target_compile_definitions(updater PRIVATE IMHEX_PROJECT_NAME="${PROJECT_NAME}")
target_include_directories(updater PRIVATE
    	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/types/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/utils/lib/jthread/includes>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/io/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/trace/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/third_party/imgui/imgui/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/utils/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/pattern_language/lib/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/third_party/nlohmann_json/include>
	libimhex
)
target_link_libraries(updater PRIVATE libimhex libwolv ${MBEDTLS_LIBRARY} ${FMT_LIBRARIES} ${MBEDTLS_INCLUDE})
if (UNIX OR BSD)
    target_link_libraries(updater PRIVATE pthread)
endif()

add_dependencies(main updater)
setupCompilerFlags(updater)


set_target_properties(updater PROPERTIES
        OUTPUT_NAME "imhex-updater"
        RUNTIME_OUTPUT_DIRECTORY ${IMHEX_MAIN_OUTPUT_DIRECTORY}
)
