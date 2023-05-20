cmake_minimum_required(VERSION 3.16)

project(algorithms_test)
set(TEST_CATEGORY Algorithms)

# Add new tests here #
set(AVAILABLE_TESTS
    # Endian
        32BitIntegerEndianSwap
        64BitFloatEndianSwap

    # Crypto
        EncodeDecode16
        EncodeDecode64
        EncodeDecodeLEB128
        CRC32
        CRC32Random
        CRC16
        CRC16Random
        CRC8
        CRC8Random
        md5
        sha1
        sha224
        sha256
        sha384
        sha512
)


add_executable(${PROJECT_NAME}
        source/endian.cpp
        source/crypto.cpp
)


# ---- No need to change anything from here downwards unless you know what you're doing ---- #

target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} PRIVATE libimhex tests_common ${FMT_LIBRARIES})

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

foreach (test IN LISTS AVAILABLE_TESTS)
    add_test(NAME "${TEST_CATEGORY}/${test}" COMMAND ${PROJECT_NAME} "${test}" WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach ()
add_dependencies(unit_tests ${PROJECT_NAME})
