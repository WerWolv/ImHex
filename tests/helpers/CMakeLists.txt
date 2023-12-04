cmake_minimum_required(VERSION 3.16)

project(helpers_test)
set(TEST_CATEGORY Helpers)

# Add new tests here #
set(AVAILABLE_TESTS
    # Common
        TestSucceeding
        TestFailing
        TestProvider_read
        TestProvider_write

    # File
        FileAccess

    # Utils
        SplitStringAtChar
        SplitStringAtString
        ExtractBits
)

if (NOT IMHEX_OFFLINE_BUILD)
    set(AVAILABLE_TESTS ${AVAILABLE_TESTS}
        # Net
        StoreAPI
        TipsAPI
        ContentAPI
    )
endif ()


add_executable(${PROJECT_NAME}
        source/common.cpp
        source/file.cpp
        source/net.cpp
        source/utils.cpp
)


# ---- No need to change anything from here downwards unless you know what you're doing ---- #

target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} PRIVATE libimhex tests_common ${FMT_LIBRARIES})

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

foreach (test IN LISTS AVAILABLE_TESTS)
    add_test(NAME "${TEST_CATEGORY}/${test}" COMMAND ${PROJECT_NAME} "${test}" WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach ()
add_dependencies(unit_tests ${PROJECT_NAME})
