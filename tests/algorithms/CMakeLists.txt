cmake_minimum_required(VERSION 3.16)

project(algorithms_test)


# Add new tests here #
set(AVAILABLE_TESTS
    # Common
        TestSucceeding
        TestFailing
        TestProvider_read
        TestProvider_write

    # Endian
        32BitIntegerEndianSwap
        64BitFloatEndianSwap

    # Crypto
        EncodeDecode16
)


add_executable(algorithms_test
        source/main.cpp

        source/common.cpp
        source/endian.cpp
        source/crypto.cpp
)
target_include_directories(algorithms_test PRIVATE include)
target_link_libraries(algorithms_test libimhex)

set_target_properties(algorithms_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

foreach (test IN LISTS AVAILABLE_TESTS)
    add_test(NAME "Algorithms/${test}" COMMAND algorithms_test "${test}" WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach ()