project(${IMHEX_PLUGIN_NAME}_tests)

# Add new tests here #
set(AVAILABLE_TESTS
    Providers/ReadWrite
    Providers/InvalidResize
)

add_library(${PROJECT_NAME} OBJECT
    source/main.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
	${CMAKE_SOURCE_DIR}/plugins/builtin/include
    	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/pattern_language/lib/include>
    	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/types/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/io/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/utils/lib/jthread/includes>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/trace/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/third_party/imgui/imgui/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/utils/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/third_party/nlohmann_json/include>
	${CMAKE_INSTALL_PREFIX}/usr/include
	${FMT_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE libimhex ${FMT_LIBRARIES})

foreach (test IN LISTS AVAILABLE_TESTS)
    add_test(NAME "Plugin_${IMHEX_PLUGIN_NAME}/${test}" COMMAND $<TARGET_FILE:plugins_test> "${test}" WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach ()
