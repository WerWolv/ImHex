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
        EncodingLineStartAddressCache

    # File
        FileAccess
        FileBackedProviderData

    # Utils
        ExtractBits
        EncodeByteString
        EncodeByteStringRoundTrip

    # Encoding
        DecodeUnicodeText
        DecodeUnicodeTextBounded
        EscapeCodepoints
        EncodingFileTable
        EncodingFileIncludes
        EncodingFileIncludeContext
        EncodingFileAliases
        EncodingFileAliasRejectsPathTraversal
        EncodingFileMetadata
        EncodingFileAmbiguity
        EncodingFileCodepointEscapes
        SingleCharacterAndControlCodes
        EncodingFileNames
        EncodingLookupRejectsPathTraversal
        CodepageFromEncoding
        DecodeByteStringEscapes

    # Decoded string
        DecodeThroughSelectionStopsAtTheSelection
        DecodeThroughSelectionFinishesAStraddlingCodePoint
        DecodeThroughSelectionReportsMalformedBytes
        DecodeThroughSelectionHonoursTheDisplayBudget
        ExtendToWholeCodePoints
        FormatDecodedStringMarksTruncation
        NulPictureRoundTrip
        FormatCodePoint
        CodePointSize

    # String codec
        EncodeStrictRefusesWhatItCannotRepresent
        EncodeLossySubstitutesRatherThanFailing
        DecodeReportsMalformedBytes
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
        source/decoded_string.cpp
        source/encoding.cpp
        source/encoding_line_cache.cpp
        source/file.cpp
        source/net.cpp
        source/string_codec.cpp
        source/utils.cpp

        # A test for plugin code needs the plugin's own implementation compiled in too.
        ${CMAKE_SOURCE_DIR}/plugins/builtin/source/content/helpers/decoded_string.cpp
)


# ---- No need to change anything from here downwards unless you know what you're doing ---- #

target_include_directories(${PROJECT_NAME} PRIVATE
        include
        ${CMAKE_SOURCE_DIR}/plugins/builtin/include
        ${CMAKE_SOURCE_DIR}/plugins/ui/include)
target_link_libraries(${PROJECT_NAME} PRIVATE libimhex tests_common ${FMT_LIBRARIES})

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${IMHEX_MAIN_OUTPUT_DIRECTORY})

foreach (test IN LISTS AVAILABLE_TESTS)
    add_test(NAME "${TEST_CATEGORY}/${test}" COMMAND ${PROJECT_NAME} "${test}" WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach ()
add_dependencies(unit_tests ${PROJECT_NAME})
