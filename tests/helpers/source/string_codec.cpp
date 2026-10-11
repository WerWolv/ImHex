#include <hex/test/tests.hpp>

#include <hex/helpers/string_codec.hpp>

#include <vector>

TEST_SEQUENCE("EncodeStrictRefusesWhatItCannotRepresent") {
    const hex::PatternLanguageStringCodec codec;

    TEST_ASSERT(codec.encode("A", "UTF-16LE").value() == (std::vector<u8>{ 0x41, 0x00 }));
    TEST_ASSERT(codec.encode("A", "UTF-16BE").value() == (std::vector<u8>{ 0x00, 0x41 }));

    // Malformed source text has no encoding under any target.
    TEST_ASSERT(!codec.encode("\xFF", "UTF-8").has_value());
    TEST_ASSERT(!codec.encode("\xFF", "UTF-16LE").has_value());

    // An algorithmic name is answered by its own encoder alone, never a fallback table.
    TEST_ASSERT(!codec.encode("A", "no_such_encoding_exists").has_value());

    TEST_SUCCESS();
};

TEST_SEQUENCE("EncodeLossySubstitutesRatherThanFailing") {
    const hex::PatternLanguageStringCodec codec;

    // Representable text encodes the same as the strict path.
    TEST_ASSERT(codec.encodeLossy("A", "UTF-16LE") == (std::vector<u8>{ 0x41, 0x00 }));

    // Malformed source UTF-8 becomes U+FFFD instead of failing the write.
    TEST_ASSERT(codec.encodeLossy("\xFF", "UTF-8") == (std::vector<u8>{ 0xEF, 0xBF, 0xBD }));

    // Still nothing to write for an encoding that does not exist.
    TEST_ASSERT(codec.encodeLossy("A", "no_such_encoding_exists").empty());

    TEST_SUCCESS();
};

TEST_SEQUENCE("DecodeReportsMalformedBytes") {
    const hex::PatternLanguageStringCodec codec;

    const auto ok = codec.decode(std::vector<u8>{ 0x41, 0x00 }, "UTF-16LE");
    TEST_ASSERT(ok.text == "A");
    TEST_ASSERT(ok.stopReason == pl::core::DecodeStop::EndOfInput);

    // A lone low surrogate can never be valid.
    const auto bad = codec.decode(std::vector<u8>{ 0x00, 0xDC }, "UTF-16LE");
    TEST_ASSERT(bad.stopReason == pl::core::DecodeStop::MalformedBytes);

    // An unknown encoding cannot decode anything.
    const auto unknown = codec.decode(std::vector<u8>{ 0x41 }, "no_such_encoding_exists");
    TEST_ASSERT(unknown.stopReason == pl::core::DecodeStop::MalformedBytes);

    // The code point limit is what the tree view's display budget rides on.
    const auto limited = codec.decode(std::vector<u8>{ 0x41, 0x00, 0x42, 0x00 }, "UTF-16LE", 1);
    TEST_ASSERT(limited.text == "A");
    TEST_ASSERT(limited.stopReason == pl::core::DecodeStop::CodepointLimit);

    TEST_SUCCESS();
};
