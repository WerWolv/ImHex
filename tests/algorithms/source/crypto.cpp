#include <hex/helpers/crypto.hpp>
#include "hex/helpers/logger.hpp"
#include "test_provider.hpp"
#include "tests.hpp"

#include <random>
#include <fmt/ranges.h>

struct EncodeChek {
    std::vector<u8> vec;
    std::string string;
};

TEST_SEQUENCE("EncodeDecode16") {

    std::array golden_samples = {
        // source: created by hand
        EncodeChek{ { }, "" },
        EncodeChek{ { 0x2a }, "2A" },
        //EncodeChek{ { 0x00, 0x2a }, "002A" }, // BUG: mbedtls_mpi_read_binary ignores initial null bytes
        EncodeChek{ { 0x2a, 0x00 }, "2A00" },
        EncodeChek{ { 0xde, 0xad, 0xbe, 0xef, 0x42, 0x2a, 0x00, 0xff}, "DEADBEEF422A00FF" },
    };

    for(auto& i: golden_samples) {
        std::string string;
        TEST_ASSERT((string = hex::crypt::encode16(i.vec)) == i.string, "string: '{}' i.string: '{}' from: {}", string, i.string, i.vec);

        std::vector<u8> vec;
        TEST_ASSERT((vec = hex::crypt::decode16(i.string)) == i.vec, "vec: {} i.vec: {} from: '{}'", vec, i.vec, i.string);
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dataLen(0, 1024);
    std::uniform_int_distribution<u8> data;

    for(int i = 0; i < 1000; i++) {
        std::vector<u8> original(dataLen(gen));
        std::ranges::generate(original, [&](){ return data(gen); });

        if ((original.size() > 0) && (original[0] == 0))
            continue; // BUG: mbedtls_mpi_read_binary ignores initial null bytes

        auto encoded = hex::crypt::encode16(original);
        auto decoded = hex::crypt::decode16(encoded);
        TEST_ASSERT(decoded == original, "decoded: {} encoded: '{}' original: {}", decoded, encoded, original);
    }

    if (hex::crypt::encode16({ 0x00, 0x2a }) == "2A") {
        hex::log::error("Known bug: in function hex::crypt::encode16 mbedtls_mpi_read_binary ingores initial null bytes");
        TEST_FAIL();
    }

    TEST_SUCCESS();
};

std::string vectorToString(std::vector<u8> in) {
    return std::string(reinterpret_cast<char*>(in.data()), in.size());
}

std::vector<u8> stringToVector(std::string in) {
    return std::vector<u8>(in.begin(), in.end());
}

TEST_SEQUENCE("EncodeDecode64") {

    std::array golden_samples = {
        // source: linux command base64 (from GNU coreutils)
        EncodeChek{ { }, "" },
        EncodeChek{ { 0x2a }, "Kg==" },
        EncodeChek{ { 0x00, 0x2a }, "ACo=" },
        EncodeChek{ { 0x2a, 0x00 }, "KgA=" },
        EncodeChek{ { 0x42, 0xff, 0x55 }, "Qv9V" },
        EncodeChek{ { 0xde, 0xad, 0xbe, 0xef, 0x42, 0x2a, 0x00, 0xff}, "3q2+70IqAP8=" },
    };

    for(auto& i: golden_samples) {
        std::string string;
        TEST_ASSERT((string = vectorToString(hex::crypt::encode64(i.vec))) == i.string, "string: '{}' i.string: '{}' from: {}", string, i.string, i.vec);

        std::vector<u8> vec;
        TEST_ASSERT((vec = hex::crypt::decode64(stringToVector(i.string))) == i.vec, "vec: {} i.vec: {} from: '{}'", vec, i.vec, i.string);
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dataLen(0, 1024);
    std::uniform_int_distribution<u8> data;

    for(int i = 0; i < 1000; i++) {
        std::vector<u8> original(dataLen(gen));
        std::ranges::generate(original, [&](){ return data(gen); });

        auto encoded = vectorToString(hex::crypt::encode64(original));
        auto decoded = hex::crypt::decode64(stringToVector(encoded));
        TEST_ASSERT(decoded == original, "decoded: {} encoded: '{}' original: {}", decoded, encoded, original);
    }

    TEST_SUCCESS();
};
