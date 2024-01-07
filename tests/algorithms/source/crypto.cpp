#include <hex/helpers/crypto.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/test/test_provider.hpp>
#include <hex/test/tests.hpp>

#include <random>
#include <vector>
#include <array>
#include <algorithm>
#include <fmt/ranges.h>

struct EncodeChek {
    std::vector<u8> vec;
    std::string string;
};

TEST_SEQUENCE("EncodeDecode16") {

    std::array golden_samples = {
  // source: created by hand
        EncodeChek {{},                                                  ""                },
        EncodeChek { { 0x2a },                                           "2A"              },
        EncodeChek { { 0x00, 0x2a },                                     "002A"            },
        EncodeChek { { 0x2a, 0x00 },                                     "2A00"            },
        EncodeChek { { 0xde, 0xad, 0xbe, 0xef, 0x42, 0x2a, 0x00, 0xff }, "DEADBEEF422A00FF"},
    };

    for (auto &i : golden_samples) {
        std::string string;
        TEST_ASSERT((string = hex::crypt::encode16(i.vec)) == i.string, "string: '{}' i.string: '{}' from: {}", string, i.string, i.vec);

        std::vector<u8> vec;
        TEST_ASSERT((vec = hex::crypt::decode16(i.string)) == i.vec, "vec: {} i.vec: {} from: '{}'", vec, i.vec, i.string);
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution dataLen(0, 1024);
    std::uniform_int_distribution<u8> data;

    for (int i = 0; i < 1000; i++) {
        std::vector<u8> original(dataLen(gen));
        std::generate(std::begin(original), std::end(original), [&] { return data(gen); });

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
    return std::string(reinterpret_cast<char *>(in.data()), in.size());
}

std::vector<u8> stringToVector(std::string in) {
    return std::vector<u8>(in.begin(), in.end());
}

TEST_SEQUENCE("EncodeDecode64") {

    std::array golden_samples = {
  // source: linux command base64 (from GNU coreutils)
        EncodeChek {{},                                                  ""            },
        EncodeChek { { 0x2a },                                           "Kg=="        },
        EncodeChek { { 0x00, 0x2a },                                     "ACo="        },
        EncodeChek { { 0x2a, 0x00 },                                     "KgA="        },
        EncodeChek { { 0x42, 0xff, 0x55 },                               "Qv9V"        },
        EncodeChek { { 0xde, 0xad, 0xbe, 0xef, 0x42, 0x2a, 0x00, 0xff }, "3q2+70IqAP8="},
    };

    for (auto &i : golden_samples) {
        std::string string;
        TEST_ASSERT((string = vectorToString(hex::crypt::encode64(i.vec))) == i.string, "string: '{}' i.string: '{}' from: {}", string, i.string, i.vec);

        std::vector<u8> vec;
        TEST_ASSERT((vec = hex::crypt::decode64(stringToVector(i.string))) == i.vec, "vec: {} i.vec: {} from: '{}'", vec, i.vec, i.string);
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution dataLen(0, 1024);
    std::uniform_int_distribution<u8> data;

    for (int i = 0; i < 1000; i++) {
        std::vector<u8> original(dataLen(gen));
        std::generate(std::begin(original), std::end(original), [&] { return data(gen); });

        auto encoded = vectorToString(hex::crypt::encode64(original));
        auto decoded = hex::crypt::decode64(stringToVector(encoded));
        TEST_ASSERT(decoded == original, "decoded: {} encoded: '{}' original: {}", decoded, encoded, original);
    }

    TEST_SUCCESS();
};

TEST_SEQUENCE("EncodeDecodeLEB128") {
    TEST_ASSERT(hex::crypt::encodeUleb128(0) == (std::vector<u8>{ 0 }));
    TEST_ASSERT(hex::crypt::encodeUleb128(0x7F) == (std::vector<u8>{ 0x7F }));
    TEST_ASSERT(hex::crypt::encodeUleb128(0xFF) == (std::vector<u8>{ 0xFF, 0x01 }));
    TEST_ASSERT(hex::crypt::encodeUleb128(0xF0F0) == (std::vector<u8>{ 0xF0, 0xE1, 0x03 }));

    TEST_ASSERT(hex::crypt::encodeSleb128(0) == (std::vector<u8>{ 0 }));
    TEST_ASSERT(hex::crypt::encodeSleb128(0x7F) == (std::vector<u8>{ 0xFF, 0x00 }));
    TEST_ASSERT(hex::crypt::encodeSleb128(0xFF) == (std::vector<u8>{ 0xFF, 0x01 }));
    TEST_ASSERT(hex::crypt::encodeSleb128(0xF0F0) == (std::vector<u8>{ 0xF0, 0xE1, 0x03 }));
    TEST_ASSERT(hex::crypt::encodeSleb128(-1) == (std::vector<u8>{ 0x7F }));
    TEST_ASSERT(hex::crypt::encodeSleb128(-128) == (std::vector<u8>{ 0x80, 0x7F }));

    TEST_ASSERT(hex::crypt::decodeUleb128({}) == 0);
    TEST_ASSERT(hex::crypt::decodeUleb128({ 1 }) == 0x01);
    TEST_ASSERT(hex::crypt::decodeUleb128({ 0x7F }) == 0x7F);
    TEST_ASSERT(hex::crypt::decodeUleb128({ 0xFF }) == 0x7F);
    TEST_ASSERT(hex::crypt::decodeUleb128({ 0xFF, 0x7F }) == 0x3FFF);
    TEST_ASSERT(hex::crypt::decodeUleb128({
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0x7F,
    }) == ((static_cast<u128>(0xFFFF'FFFF'FFFF) << 64) | 0xFFFF'FFFF'FFFF'FFFF));
    TEST_ASSERT(hex::crypt::decodeUleb128({ 0xAA, 0xBB, 0xCC, 0x00, 0xFF }) == 0x131DAA);

    TEST_ASSERT(hex::crypt::decodeSleb128({}) == 0);
    TEST_ASSERT(hex::crypt::decodeSleb128({ 1 }) == 0x01);
    TEST_ASSERT(hex::crypt::decodeSleb128({ 0x3F }) == 0x3F);
    TEST_ASSERT(hex::crypt::decodeSleb128({ 0x7F }) == -1);
    TEST_ASSERT(hex::crypt::decodeSleb128({ 0xFF }) == -1);
    TEST_ASSERT(hex::crypt::decodeSleb128({ 0xFF, 0x7F }) == -1);
    TEST_ASSERT(hex::crypt::decodeSleb128({
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0x7F,
    }) == -1);
    TEST_ASSERT(hex::crypt::decodeSleb128({ 0xAA, 0xBB, 0xCC, 0x00, 0xFF }) == 0x131DAA);
    TEST_ASSERT(hex::crypt::decodeSleb128({ 0xAA, 0xBB, 0x4C }) == -0xCE256);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<u8> data;

    for (int i = 0; i < 1000; i++) {
        std::vector<u8> original(sizeof(u128));
        std::generate(std::begin(original), std::end(original), [&] { return data(gen); });
        u128 u = *reinterpret_cast<u128*>(original.data());
        i128 s = *reinterpret_cast<i128*>(original.data());
        auto encodedS = hex::crypt::encodeSleb128(s);
        i128 decodedS = hex::crypt::decodeSleb128(encodedS);
        auto encodedU = hex::crypt::encodeUleb128(u);
        u128 decodedU = hex::crypt::decodeUleb128(encodedU);
        TEST_ASSERT(decodedS == s, "encoded: {0} decoded: {1:X} original: {2:X}", encodedS, static_cast<u128>(decodedS), static_cast<u128>(s));
        TEST_ASSERT(decodedU == u, "encoded: {0} decoded: {1:X} original: {2:X}", encodedU, decodedU, u);
    }

    TEST_SUCCESS();
};

struct CrcCheck {
    std::string name;
    int width;

    u64 poly;
    u64 init;
    u64 xorOut;
    bool refIn;
    bool refOut;

    u64 result;
    std::vector<u8> data;
};

template<std::invocable<hex::prv::Provider *&, u64, size_t, u32, u32, u32, bool, bool> Func, typename Range>
int checkCrcAgainstGondenSamples(Func func, Range golden_samples) {
    for (auto &i : golden_samples) {
        hex::test::TestProvider provider(&i.data);
        hex::prv::Provider *provider2 = &provider;
        auto crc                      = func(provider2, 0, i.data.size(), i.poly, i.init, i.xorOut, i.refIn, i.refOut);
        TEST_ASSERT(crc == i.result, "name: {} got: {:#x} expected: {:#x}", i.name, crc, i.result);
    }
    TEST_SUCCESS();
}

template<std::invocable<hex::prv::Provider *&, u64, size_t, u32, u32, u32, bool, bool> Func>
int checkCrcAgainstRandomData(Func func, int width) {
    // crc( message + crc(message) ) should be 0

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution distribLen(0, 1024);
    std::uniform_int_distribution<uint64_t> distribPoly(0, (0b10ull << (width - 1)) - 1);
    std::uniform_int_distribution<u8> distribData;

    for (int i = 0; i < 500; i++) {
        CrcCheck c { "", width, distribPoly(gen), distribPoly(gen), 0, false, false, 0, {} };
        c.data.resize(distribLen(gen));
        std::generate(std::begin(c.data), std::end(c.data), [&] { return distribData(gen); });

        hex::test::TestProvider testprovider(&c.data);
        hex::prv::Provider *provider = &testprovider;
        u32 crc1                     = func(provider, 0, c.data.size(), c.poly, c.init, c.xorOut, c.refIn, c.refOut);

        std::vector<u8> data2 = c.data;
        if (width >= 32) {
            data2.push_back((crc1 >> 24) & 0xff);
            data2.push_back((crc1 >> 16) & 0xff);
        }
        if (width >= 16)
            data2.push_back((crc1 >> 8) & 0xff);
        data2.push_back((crc1 >> 0) & 0xff);

        hex::test::TestProvider testprovider2(&data2);
        hex::prv::Provider *provider2 = &testprovider2;
        u32 crc2                      = func(provider2, 0, data2.size(), c.poly, c.init, c.xorOut, c.refIn, c.refOut);

        TEST_ASSERT(crc2 == 0, "got wrong crc2: {:#x}, crc1: {:#x}, "
                               "width: {:2d}, poly: {:#018x}, init: {:#018x}, xorout: {:#018x}, refin: {:5}, refout: {:5}, data: {}",
            crc2,
            crc1,
            c.width,
            c.poly,
            c.init,
            c.xorOut,
            c.refIn,
            c.refOut,
            data2);
    }

    TEST_SUCCESS();
}

TEST_SEQUENCE("CRC32") {
    std::array golden_samples = {
  // source: A Painless Guide to CRC Error Detection Algorithms [https://zlib.net/crc_v3.txt]
        CrcCheck {"CRC-32-CRC32-check", 32, 0x4C11DB7,          0xFFFFFFFF,         0xFFFFFFFF,         true,  true,  0xCBF43926,         { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 },

 // source: Sunshine's Homepage - Online CRC Calculator Javascript [http://www.sunshine2k.de/coding/javascript/crc/crc_js.html]
        CrcCheck { "CRC-32-1-check",    32, 0x4C11DB7,          0xFFFFFFFF,         0xFFFFFFFF,         true,  false, 0x649C2FD3,         { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 },
        CrcCheck { "CRC-32-2-check",    32, 0x4C11DB7,          0xFFFFFFFF,         0xFFFFFFFF,         false, true,  0x1898913F,         { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 },
        CrcCheck { "CRC-32-3-check",    32, 0x4C11DB7,          0xFFFFFFFF,         0xFFFFFFFF,         false, false, 0xFC891918,         { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 },
        CrcCheck { "CRC-32-4-check",    32, 0x4C11DB7,          0x55422a00,         0xaa004422,         false, false, 0x41A1D8EE,         { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 },
        CrcCheck { "CRC-32-5-check",    32, 0x4C11DB7,          0x55422a00,         0xaa004422,         false, false, 0xFF426E22,         {}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       },

 // source: generated by Boost CRC from random data and random parameters
        CrcCheck { "CRC-32-RANDOM-170", 32, 0x000000005c0dd7fd, 0x000000001c8be2e1, 0x00000000efdedd60, false, true,  0x000000004e9b67a8, { 181, 235, 196, 140, 43, 8, 101, 39, 17, 128, 187, 117, 118, 75, 41, 240, 228, 60, 93, 101, 228, 235, 36, 117, 208, 54, 218, 57, 24, 84, 54, 173, 13, 66, 42, 232, 206, 49, 210, 165, 146, 145, 234, 88, 76, 130, 154, 231, 247, 66, 73, 150, 163, 104, 42, 77, 214, 16, 53, 120, 210, 74, 215, 54, 88, 171, 137, 133, 26, 29, 134, 0, 103, 240, 146, 220, 169, 64, 155, 162, 23, 73, 73, 87, 224, 106, 121, 58, 66, 146, 158, 101, 196, 62, 153, 143, 86, 87, 147, 4, 36, 248, 41, 6, 213, 233, 27, 24, 42, 207, 24, 167, 72, 216, 24, 27, 59, 205, 184, 0, 101, 102, 34, 32, 248, 213, 53, 244, 83, 60, 8, 249, 115, 214, 144, 109, 245, 119, 137, 225, 156, 247, 250, 230, 147, 201, 1, 14, 111, 148, 214, 90, 80, 156, 31, 85, 186, 165, 218, 127, 66, 9, 191, 215, 17, 253, 32, 162, 28, 223, 61, 7, 115, 177, 58 }                                                                                                                                                                                                                                                                                                                },
        CrcCheck { "CRC-32-RANDOM-171", 32, 0x00000000380bb4f5, 0x00000000c6c652b3, 0x000000003a5ee7d1, false, false, 0x000000002f5a76b0, { 59, 215, 138, 110, 177, 211, 25, 172, 77, 145, 155, 166, 99, 202, 132, 92, 179, 249, 223, 254, 103, 9, 16, 218, 42 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   },
        CrcCheck { "CRC-32-RANDOM-172", 32, 0x000000000dc7ba53, 0x00000000acfa5319, 0x00000000ee250595, true,  true,  0x00000000b3e56ef4, { 218, 89, 16, 112, 197, 97, 69, 29, 33, 173, 8, 121, 78, 23, 131, 152, 82, 174, 94, 206, 33, 228, 35, 205, 83, 71, 219, 99, 13, 48, 105, 180, 187, 246, 101, 249, 91, 67, 207, 177, 61, 108, 144, 73, 209, 201, 166, 115, 2, 110, 70, 67, 25, 31, 0, 20, 83, 9, 152, 169, 125, 74, 246, 183, 186, 70, 199, 106, 38, 127, 230, 44, 43, 64, 119, 14, 97, 127, 127, 166, 98, 157, 71, 109, 3, 15, 197, 223 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               },
        CrcCheck { "CRC-32-RANDOM-173", 32, 0x00000000fd5a3b2e, 0x00000000580018a2, 0x000000002dbfb987, true,  true,  0x0000000040c086a9, { 0, 90, 253, 254, 61, 67, 185, 88, 110, 58, 243, 86, 43, 183, 21, 161, 192, 81, 10, 83, 147, 21, 235, 250, 195, 201, 199, 36, 254, 107, 191, 212, 27, 30, 173, 247, 174, 219, 240, 39, 0, 72, 146, 155, 72, 250, 252, 51, 250, 195, 161, 241, 75, 244, 13, 85, 233, 204, 70, 89, 110, 193, 25, 199, 179, 92, 169, 179, 75, 124, 142, 31, 36, 167, 16, 166, 119, 148, 68, 74, 8, 5, 60, 164, 217, 168, 231, 99, 214, 171, 239, 23, 36, 219, 176, 111, 210, 96, 111, 57, 231, 160, 5, 119, 76, 19, 197, 197, 3, 11, 121, 140, 182, 150, 30, 90, 160, 30, 114, 114, 214, 57, 118, 70, 219, 201, 223, 143, 0, 126, 14, 223, 175, 212, 208, 135, 104, 173, 169, 189, 5, 228, 232, 170, 191, 137, 45, 98, 43, 153, 180, 186, 46, 53, 167, 166, 99, 154, 188, 234, 37, 137, 37, 132, 251, 122, 143, 230, 151, 227, 41, 111, 6, 168, 135, 0, 239, 141, 125, 5, 199, 48, 161, 53, 186, 91, 56, 41, 227, 49, 28, 132, 88, 2, 22, 33, 21, 155, 209, 82, 116, 17, 142, 55, 87, 156, 134, 165, 153, 38, 125, 40, 80, 229, 233, 28, 23, 197 }                                                                                                         },
        CrcCheck { "CRC-32-RANDOM-174", 32, 0x000000006eae3222, 0x0000000097093735, 0x000000000460e363, false, false, 0x0000000096bf93cd, { 98, 234, 52, 152, 123 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                },
        CrcCheck { "CRC-32-RANDOM-175", 32, 0x000000002ac3bed5, 0x00000000c2a0964a, 0x0000000019ee4b3b, true,  true,  0x00000000b7f1e6b7, { 108, 151, 223, 46, 204, 105, 253, 2, 101, 184, 48, 186, 204, 86, 230, 246, 222, 137, 136, 207, 197, 195, 33, 165, 239, 55, 92, 9, 54, 29, 189, 126, 123, 106, 19, 1, 176, 52, 87, 178, 246, 110, 75, 220, 204, 8, 11, 22 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             },
        CrcCheck { "CRC-32-RANDOM-176", 32, 0x000000006dc26b9c, 0x00000000096e9400, 0x000000005e4839bf, false, true,  0x0000000063d4b648, { 88, 185, 144, 84, 86, 77, 217, 85, 61, 80, 21, 84, 81, 120, 14, 247, 106, 56, 193, 3, 185, 118, 131, 196, 51, 249, 79, 252, 145, 43, 243, 120, 56, 184, 242, 226, 80, 73, 102, 179, 20, 7, 208, 70, 242, 20, 208, 180, 21, 128, 175, 195, 248, 174, 45, 187, 142, 76, 2, 6, 58, 56, 155, 28, 37, 35, 134, 50, 34, 174, 204, 170, 163, 94, 68, 161, 124, 23, 224, 38, 137, 255, 92, 228, 77, 53, 42, 145, 147, 12, 246, 23, 205, 143, 241, 201, 227, 79, 215, 65, 55, 247, 219, 209, 30, 19, 11, 211, 145, 150, 45, 200, 90, 69, 55, 234, 7, 11, 6, 113, 158, 229, 56, 131, 220, 14, 236, 127, 249, 191, 182, 108, 23, 197, 148, 247, 115, 85, 0, 86, 63, 210, 153, 112, 235, 146, 53, 249, 216, 42, 169, 18, 54, 245, 60, 232, 224, 9, 187, 125, 27, 180, 171, 138, 138, 13, 45, 125, 220, 158, 164, 21, 109, 23, 8, 2, 41, 178, 245, 226, 211, 202, 134, 4, 133, 192, 125 }                                                                                                                                                                                                                                                           },
        CrcCheck { "CRC-32-RANDOM-177", 32, 0x00000000de2c5518, 0x000000003e12e6ec, 0x00000000474d1134, true,  false, 0x00000000623e70f4, { 239, 114, 198, 23, 88, 133, 118, 9, 184, 162, 78, 142, 128, 165, 196, 6, 148, 0, 56, 249, 4, 126, 152, 67, 17, 48, 153, 151, 46, 183, 92, 242, 155, 233, 216, 166, 22, 107, 139, 93, 3, 239, 154, 20, 15, 69, 41, 126, 65, 76, 229, 133, 161, 181, 201, 134, 213, 158, 174, 17, 32, 45, 223, 74, 56, 194, 228, 37, 71, 128, 160, 202, 219, 173, 55, 223, 104, 90, 176, 152, 113, 160, 224, 36, 111, 170, 64, 28, 29, 239, 23, 135, 254, 240, 117, 147, 125, 138, 86, 206, 28, 48, 169, 107, 193, 186, 197, 219, 180, 83, 108, 250, 172, 21, 18, 121, 154, 77, 36, 48, 88, 167, 42, 3, 91, 172, 235, 166, 85, 93, 42, 254, 47, 37, 193, 104, 32, 171, 172, 225, 194, 80, 120, 61, 198, 108, 105, 21, 188, 51, 101, 49, 88, 97, 51, 168, 251, 4, 226, 114, 202, 53, 7, 171, 41, 72, 138, 161, 227, 182, 223, 92, 96, 196, 203, 255, 72, 190, 6, 106, 69, 172, 41, 131, 241, 34, 147, 155, 27, 67, 109, 39, 202, 82, 184, 160, 167, 163, 66, 222, 172, 65, 24, 46, 181, 217, 1, 249, 206, 171, 27, 87, 88, 40, 191, 153, 121, 206, 89, 48, 7, 86, 82, 68, 129, 224, 181, 108, 144, 1, 14, 204, 79, 183, 129, 116, 124, 175, 158, 98, 197 }},
        CrcCheck { "CRC-32-RANDOM-178", 32, 0x00000000983395ff, 0x00000000b8a8a4fe, 0x00000000c0996c7c, true,  true,  0x0000000003def4ee, { 81, 82, 118, 44, 112, 193, 97, 94, 233, 4, 105, 223, 158, 176, 91, 215, 162, 197, 79, 59, 191, 152, 87, 68, 79, 122, 35, 78, 180, 40, 151, 82, 199, 227 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              },
        CrcCheck { "CRC-32-RANDOM-179", 32, 0x0000000096345585, 0x0000000098436ef2, 0x000000000373eba2, true,  false, 0x000000001a7ca97a, { 22, 29, 92, 40, 254, 225, 67, 92, 243, 28, 191, 168, 25, 228, 67, 240, 230, 20, 1, 165, 223, 154, 244, 100, 127, 254, 103, 233, 105, 139, 3, 232, 31, 57, 84, 99, 144, 1, 105, 240, 103, 118, 146, 128, 216, 43, 115, 59, 233, 56, 3, 9, 139, 64, 229, 52, 116, 210, 173, 55, 190, 126, 168, 10, 4, 72, 62, 134, 152, 151, 143, 153, 217, 50, 134, 15, 251, 158, 241, 253, 161, 36, 44, 60, 75, 74, 253, 170, 39, 43, 255, 183, 194, 176, 95, 255, 21, 122, 83, 200, 201, 249, 175, 245, 166, 128, 54, 253, 234, 106, 122, 177, 169, 162, 71, 99, 135, 204, 72, 24, 22, 170, 97, 11, 5, 165, 88, 173, 43, 138, 143, 5, 68, 12, 178, 125, 66, 132, 28, 215, 49, 47, 146, 193, 0, 193, 65, 103, 53, 81, 58, 99, 38, 224, 34, 124, 44, 165, 95, 129, 192, 160, 110, 76, 131, 157, 49, 76, 100, 83, 220, 101, 253, 108, 11, 21, 88, 178, 114, 163, 58, 200, 57, 232, 252, 216, 217, 154, 122, 251, 200, 216, 238, 165, 94, 97, 76, 112, 39, 243, 77, 81, 189, 10, 48, 10, 65, 180, 252, 15, 132, 86, 68, 199, 185, 4, 19, 19, 61, 249, 133, 80, 45, 206, 49, 16, 107, 176 }                                                                },
    };

    TEST_ASSERT(!checkCrcAgainstGondenSamples(hex::crypt::crc32, golden_samples));

    TEST_SUCCESS();
};

TEST_SEQUENCE("CRC32Random") {
    TEST_ASSERT(!checkCrcAgainstRandomData(hex::crypt::crc32, 32));

    TEST_SUCCESS();
};

TEST_SEQUENCE("CRC16") {
    std::array golden_samples = {
  // source: A Painless Guide to CRC Error Detection Algorithms [https://zlib.net/crc_v3.txt]
        CrcCheck {"CRC-16-CRC16-check", 16, 0x8005,             0x0000,             0x0000,             true,  true,  0xBB3D,             { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       },

 // source: Sunshine's Homepage - Online CRC Calculator Javascript [http://www.sunshine2k.de/coding/javascript/crc/crc_js.html]
        CrcCheck { "CRC-16-1-check",    16, 0x8005,             0x0000,             0x0000,             true,  false, 0xBCDD,             { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       },
        CrcCheck { "CRC-16-2-check",    16, 0x8005,             0x0000,             0x0000,             false, true,  0x177F,             { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       },
        CrcCheck { "CRC-16-3-check",    16, 0x8005,             0x0000,             0x0000,             false, false, 0xFEE8,             { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       },
        CrcCheck { "CRC-16-3-check",    16, 0x8005,             0x5042,             0xfc2a,             false, false, 0xDD50,             { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       },

 // source: generated by Boost CRC from random data and random parameters
        CrcCheck { "CRC-16-RANDOM-10",  16, 0x000000000000afbb, 0x00000000000091ea, 0x0000000000000ea8, true,  true,  0x0000000000000670, { 239, 127, 45, 34, 24, 9, 68, 49, 206, 206, 71, 116, 233, 144, 237, 184, 241, 86, 244, 237, 163, 167, 42, 194, 69, 147, 236, 136, 245, 183, 254, 2, 67, 220, 111, 241, 168, 255, 36, 248, 147, 137, 75, 137, 201, 100, 215, 161, 36, 13, 54, 235, 34, 187, 75, 82, 227, 97, 240, 137, 173, 165, 246, 129, 30, 174, 42, 21, 185, 94, 43, 218, 126, 90, 197, 205, 15, 21, 115, 50, 103, 38, 178, 124, 27, 24, 208, 157, 41, 53, 204, 158, 198, 238, 133, 61, 164, 203, 159, 6, 94, 213, 225, 145, 61, 245, 86, 157, 126, 41, 130, 195, 130, 11, 48, 29, 193, 187, 127, 135, 83, 44, 232, 66, 169, 147, 106, 11, 118, 124, 189, 114, 131, 148, 106, 45, 250, 134, 11, 189, 179, 74, 92, 43, 8, 116, 18, 241, 53, 218, 160, 169, 65, 112, 161, 63, 208, 61, 223, 18, 254, 51, 87, 101, 180, 244, 149, 78, 135, 54, 222, 122, 244, 184, 44 }                                       },
        CrcCheck { "CRC-16-RANDOM-11",  16, 0x0000000000001b6c, 0x0000000000005bce, 0x000000000000e29c, true,  true,  0x000000000000dfa2, { 92, 73, 175, 57, 17, 7, 61, 3, 7, 81, 172, 188, 91, 214, 51, 201, 52, 249, 51, 206, 210, 79, 156, 42, 36, 28, 235, 71, 83, 127, 30, 123, 200, 55, 127, 217, 218, 71, 203, 29, 223, 222, 198, 56, 138, 207, 196, 46, 195, 105, 28, 45, 5, 138, 168, 54, 239, 203, 1, 0, 105, 110, 21, 193, 207 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              },
        CrcCheck { "CRC-16-RANDOM-12",  16, 0x000000000000477b, 0x000000000000afa4, 0x0000000000003d01, false, false, 0x0000000000000b96, { 63, 84, 254, 94, 21, 194, 88, 199, 189, 117, 111, 234, 231, 51, 119, 117, 203, 239, 210, 109, 162, 58, 158, 239, 163, 18, 68, 233, 37, 120, 48, 205, 17, 188, 141, 44, 143, 147, 173, 105 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  },
        CrcCheck { "CRC-16-RANDOM-13",  16, 0x0000000000004438, 0x0000000000008e25, 0x0000000000006c55, false, false, 0x0000000000004a6d, { 179, 169, 67, 230, 228, 213, 173, 155, 152, 64, 85, 170, 20, 177, 38, 127, 169, 186, 44, 163, 153, 153, 11, 112, 63, 24, 127, 25, 135, 40, 214, 33, 88, 132, 14, 84, 82, 66, 216, 75, 55, 231, 101, 114, 68, 244, 56, 140, 100, 196, 226, 60, 0, 177, 187, 164, 237, 1, 199, 119, 249, 148, 102, 175, 32, 62, 232, 179, 30, 102, 85, 8, 188, 61, 28, 156, 74, 71, 11, 102, 51, 243, 120, 60, 146, 207, 116, 156, 219, 237, 157, 25, 0, 149, 7, 137, 248, 102, 157, 171, 60, 76, 117, 29, 34, 117, 148, 241, 142, 18, 251, 240, 37, 213, 171, 120, 85, 145, 50, 209, 130, 225, 28, 27, 170, 195, 148, 102 }                                                                                                                                                                                                                                                                   },
        CrcCheck { "CRC-16-RANDOM-14",  16, 0x000000000000f461, 0x0000000000004d96, 0x0000000000003e1d, true,  true,  0x0000000000009eef, { 38, 252, 182, 80, 159, 97, 166, 150, 29, 9, 45, 216, 186, 165, 148, 128, 60, 170, 243, 69, 177, 203, 17, 191, 5, 60, 209, 41, 20, 42, 23, 147, 126, 209, 125, 157, 30, 45, 94, 157, 146, 7, 20, 234, 70, 23, 141, 87, 88, 93, 184, 169, 69, 88, 108, 253, 58, 157, 175, 88, 177, 154, 181, 127, 216, 82, 202, 16, 164, 227, 188, 243, 140, 84, 24, 213, 31, 130, 185, 234, 215, 248, 169, 233, 4, 208, 67, 102, 248, 13, 114, 162, 175, 187, 120, 228, 213, 93 }                                                                                                                                                                                                                                                                                                                                                                                                             },
        CrcCheck { "CRC-16-RANDOM-15",  16, 0x000000000000bbf3, 0x000000000000e279, 0x000000000000a01c, false, false, 0x000000000000f294, { 251, 1, 172, 207, 75, 242, 148, 19, 255, 106, 41, 114, 213, 142, 229, 239, 156, 23, 225, 4, 181, 190, 130, 111, 160, 59, 145, 253, 181, 114, 17, 118, 65, 201, 206, 61, 137, 118, 87, 156, 205, 110, 6, 63, 153, 254, 163, 225, 66, 88, 232, 189, 126, 92, 228, 204, 0, 243, 78, 239, 62, 193, 27, 197, 106, 96, 215, 1, 143, 116, 114, 112, 6, 150, 209, 152, 254, 66, 54, 94, 123, 109, 220, 31, 156, 118, 201, 119, 232, 181, 49, 140, 82, 192, 65, 167, 94, 196, 10, 162, 138, 163, 9, 240, 203, 230, 23, 117, 118, 217, 35, 59, 80, 150, 105, 253, 127, 105, 53, 54, 134, 90, 78, 161, 95, 123, 164, 235, 209, 143, 12, 199, 20, 167, 53, 246, 87, 5, 76, 164, 90, 230, 19, 34, 24, 30, 133, 190, 136, 129, 68, 208, 98, 110, 170, 174, 135, 152, 155, 76, 215, 26, 189, 63, 72, 14, 57, 186, 173, 44, 212, 212, 66, 120, 155, 51, 62, 116, 210, 218, 49, 125, 23, 134 }},
        CrcCheck { "CRC-16-RANDOM-16",  16, 0x000000000000e5dd, 0x0000000000009239, 0x00000000000006f7, false, false, 0x0000000000005351, { 185, 105, 153, 99, 108, 57, 120, 51, 20, 3, 200, 10, 175, 75, 171, 152, 175, 99, 174, 14, 48, 148, 220, 47, 84, 168, 249, 218, 35, 74, 212, 106, 182, 241, 40, 210, 59, 193, 243, 1, 225, 152, 167, 139, 119, 252, 61, 192, 71, 32, 236, 161, 110, 30, 151, 179, 147, 225, 190, 238, 30, 131, 165, 128, 141, 6, 84, 62, 13, 147, 135, 190, 42, 97, 140, 154, 231, 162, 125, 98, 239, 156, 248, 149, 43, 112, 164, 127, 103, 1, 59, 30, 210, 140, 174, 72, 121, 187, 29, 204, 32, 120, 108, 243, 54, 124, 30, 88, 116, 179, 188, 230, 16, 139, 153, 151, 128, 109, 155, 131, 56, 83, 125, 11, 178, 79, 68, 209, 198, 216, 81, 133, 171, 184, 222, 68, 99, 153, 34, 93, 135, 148, 128, 21, 110, 248, 141, 92, 92, 117, 154, 56, 250, 210, 126, 109, 113, 233, 143, 253, 8, 184, 61, 223, 170, 131, 215, 150, 57, 91, 95, 200, 151, 185, 234, 166, 113, 73, 34, 83, 204, 6 }    },
        CrcCheck { "CRC-16-RANDOM-17",  16, 0x000000000000b4a3, 0x000000000000b94e, 0x000000000000744e, true,  true,  0x000000000000cd8b, { 55, 240, 81, 130, 195, 14, 15, 70, 94, 190, 211, 82, 239, 29, 140, 56, 29, 155, 47, 100, 41, 110, 50, 185, 94, 203, 192, 11, 78, 245, 44, 158, 244, 176, 132, 85, 193, 94, 32, 74, 6, 224, 248, 2, 61, 8, 227, 112, 10, 58, 81, 76, 56, 252, 147, 99, 226, 82, 203, 87, 9, 216, 201, 189, 195, 142, 216, 248, 73, 157, 62 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  },
        CrcCheck { "CRC-16-RANDOM-18",  16, 0x0000000000009c67, 0x0000000000006327, 0x0000000000008e39, false, false, 0x000000000000d9e0, { 37, 181, 10, 26, 177, 9, 181, 162, 61, 13, 117, 143, 203, 86, 77, 104, 107, 0, 187, 12, 243, 73, 117, 131, 36, 34, 68, 180, 221, 2, 10, 104, 42, 247, 230, 199, 208, 83, 55, 235, 33, 104, 10, 91, 250, 88, 16, 24, 191, 252, 94, 152, 208, 179, 216, 41, 101, 64, 217, 76, 33, 231 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        },
    };

    TEST_ASSERT(!checkCrcAgainstGondenSamples(hex::crypt::crc16, golden_samples));

    TEST_SUCCESS();
};

TEST_SEQUENCE("CRC16Random") {
    TEST_ASSERT(!checkCrcAgainstRandomData(hex::crypt::crc16, 16));

    TEST_SUCCESS();
};


TEST_SEQUENCE("CRC8") {
    std::array golden_samples = {
  // source: Sunshine's Homepage - Online CRC Calculator Javascript [http://www.sunshine2k.de/coding/javascript/crc/crc_js.html]
        CrcCheck {"CRC-8-0-check",   8, 0xD5,               0xff,               0x00,               true,  true,  0x7f,               { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               },
        CrcCheck { "CRC-8-1-check",  8, 0xD5,               0xff,               0x00,               true,  false, 0xfe,               { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               },
        CrcCheck { "CRC-8-2-check",  8, 0xD5,               0xff,               0x00,               false, true,  0x3e,               { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               },
        CrcCheck { "CRC-8-3-check",  8, 0xD5,               0xff,               0x00,               false, false, 0x7c,               { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               },
        CrcCheck { "CRC-8-3-check",  8, 0xD5,               0x42,               0x5a,               false, false, 0x4a,               { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               },

 // source: generated by Boost CRC from random data and random parameters
        CrcCheck { "CRC-8-RANDOM-0", 8, 0x000000000000008b, 0x00000000000000d4, 0x00000000000000c7, true,  false, 0x0000000000000093, { 195, 137, 209, 107, 84, 196, 218, 41, 155, 11, 48, 19, 105, 74, 207, 198, 134, 17, 172, 76, 89, 18, 81, 236, 101, 109, 222, 62, 254, 170, 66, 240, 56, 184, 199, 187, 253, 115, 251, 59, 115, 2, 105, 234, 91, 110, 86, 36, 31, 129, 146, 217, 16, 90, 115, 35, 27, 17, 81, 247, 215, 8, 67, 77, 103, 141, 9, 101, 90, 36, 155, 193, 106, 186, 134, 46, 182, 124, 220, 46, 4, 203, 171, 215, 56, 132, 110, 146, 77, 231, 214, 233, 17, 49, 77, 119, 80, 77, 158, 253, 255, 74, 94, 232, 77, 94, 81, 48, 164, 29, 51, 81, 122, 71, 23, 57, 126, 176, 129, 250, 163, 6, 1, 191, 5, 93, 172, 176, 128, 202, 52, 89, 104, 36, 50, 30, 64, 216, 19, 140, 229, 7, 214, 168, 155 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          },
        CrcCheck { "CRC-8-RANDOM-1", 8, 0x000000000000005d, 0x0000000000000077, 0x0000000000000005, false, false, 0x000000000000009a, { 40, 210, 96, 74, 179, 97, 240, 65, 23, 50, 222, 233, 252, 131, 110, 135, 141, 161, 239, 91, 108, 132, 166, 169, 82, 187, 251, 92, 125, 57, 64, 207, 238, 108, 243, 72, 50, 229, 127, 224, 235, 179, 59, 107, 36, 48, 15, 165, 24, 196, 221, 5, 116, 57, 5, 124, 1, 64, 141, 134, 82, 159, 200, 171, 19, 10, 196, 70, 80, 39, 2, 188, 230, 165, 138, 178, 38, 44, 26, 225, 212, 32, 44, 139, 39, 125, 231, 94, 224, 89, 47, 125, 6, 46, 254, 49, 101, 225, 23, 44, 89, 16, 76, 50, 23, 115, 188, 185, 76, 100, 122, 1, 57, 239, 100, 180, 63, 158, 205, 6 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           },
        CrcCheck { "CRC-8-RANDOM-2", 8, 0x00000000000000ea, 0x00000000000000d9, 0x0000000000000000, false, false, 0x0000000000000092, { 215, 10, 66, 226, 48, 21, 189, 238, 141, 93, 174, 19, 109, 196, 154, 78, 215 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       },
        CrcCheck { "CRC-8-RANDOM-3", 8, 0x00000000000000f3, 0x000000000000007b, 0x000000000000007f, true,  true,  0x00000000000000de, { 120, 75, 112, 57, 59, 218, 44, 68, 242, 0, 155, 24, 95, 210, 134, 36, 136, 139, 106, 190, 215, 23, 15, 45, 185, 217, 72, 219, 214, 170, 89, 93, 179, 61, 71, 162, 221, 10, 37, 163, 205, 10, 136, 200, 77, 102, 51, 188, 170, 232, 196, 184, 200, 98, 79, 150, 249, 253, 188, 27, 53, 169, 239, 246, 167, 28, 100, 86, 224, 197, 201, 8, 176, 114, 195, 40, 181, 52, 77, 27, 151, 45, 44, 205, 245, 240, 182, 223, 205, 182, 57, 102, 44, 72, 201, 233, 168, 241, 30, 253, 104, 7, 72, 227, 135, 49, 63, 209, 187, 174, 29, 255, 237, 107, 77, 22, 187, 148, 64, 207, 175, 218, 201, 104, 45, 54, 204, 65, 80, 6, 185, 187, 10, 246, 222, 62, 115, 88, 250, 65, 148, 127, 28, 93, 121, 161, 65, 87, 150, 151, 117, 199, 229, 98, 31, 145, 34, 242, 145, 146, 9, 24, 176, 248, 104, 180, 208, 181, 64, 223, 171, 144, 156, 80, 234, 169, 218, 107, 68, 62, 147, 7, 61, 102, 75, 112, 168, 33, 13, 132, 56, 46, 181, 219, 84, 137, 64, 84, 228, 172, 143 }                                                                                                                                                                                                                                             },
        CrcCheck { "CRC-8-RANDOM-4", 8, 0x00000000000000a1, 0x0000000000000035, 0x0000000000000013, true,  true,  0x00000000000000e8, { 132, 238, 232, 49, 230, 205, 207, 227, 227, 111, 23, 5, 192, 33, 32, 227, 219, 48, 97, 228, 184, 213, 25, 66, 188, 16, 190, 115, 253, 113, 144, 222, 9, 120, 159, 187, 23, 146, 37, 212, 214, 3, 54, 190, 246, 3, 55, 19, 254, 150, 31, 36, 112, 89, 32, 78, 42, 171, 124, 3, 229, 191, 144, 10, 60, 209, 46, 54, 11, 205, 109, 52, 142, 67, 189, 186, 147, 219, 91, 21, 1, 61, 143, 77, 38, 15, 150, 126, 140, 139, 233, 83, 103, 162, 1, 79, 30, 223, 51, 93, 43, 131, 8, 123, 228, 37 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           },
        CrcCheck { "CRC-8-RANDOM-5", 8, 0x0000000000000095, 0x00000000000000d8, 0x0000000000000043, false, false, 0x0000000000000008, { 25, 135, 154, 213, 104, 181, 83, 188, 128, 84, 4, 175, 2, 108, 206, 235, 11, 252, 150, 39, 125, 195, 172, 130, 109, 181, 73, 171, 211, 35, 162, 82, 207, 1, 73, 78, 24, 102, 151, 170, 234, 242, 127, 77, 161, 43, 80, 176, 5, 89, 11, 51, 24, 60, 144, 187, 182, 70, 177, 218, 126, 230, 188, 108, 205, 181, 17, 55, 154, 207, 228, 209, 77, 99, 122, 146, 209, 199, 47, 177, 200, 178, 139, 239, 27, 56, 183, 228, 153, 127, 47, 34, 111, 78, 161, 54, 86, 110, 244, 126, 108, 95, 7, 100, 160, 26, 133, 76, 101, 59, 25, 54, 23, 83, 148, 90, 26, 252, 213, 37, 9, 97, 10, 56, 53, 213, 152, 111, 126, 254, 101, 232, 71, 1, 166, 14, 159, 196, 71, 113, 20, 232, 138, 115, 126, 64, 140, 11, 52, 78, 240, 45, 160, 103, 212, 19, 188, 238, 141, 92, 126, 36, 160, 44, 72, 121, 60, 8, 211, 112, 192, 198, 50, 83, 177, 80, 166, 107, 96, 205, 183, 126, 229, 254, 128, 154, 191, 242, 251, 248, 122, 174, 162, 89, 136, 83, 217, 220, 224, 106, 23, 22, 33, 63, 142, 226, 83, 247, 60, 102, 193, 36, 63, 235, 97, 182, 86, 229, 85, 98, 90, 17, 253, 134, 201, 253, 64, 39, 33, 223, 8, 110, 55, 10, 223, 136, 14, 229, 66, 179, 79, 203, 110, 41, 151, 194, 85, 190, 122, 114, 109, 209, 59, 8 }},
        CrcCheck { "CRC-8-RANDOM-6", 8, 0x0000000000000081, 0x0000000000000083, 0x00000000000000d7, true,  true,  0x000000000000006d, { 7, 210, 211, 232, 246, 26, 147, 95, 236, 136, 206, 194, 251, 106, 140, 115, 125, 183, 176, 39, 84, 236, 236, 111, 120, 165, 211, 12, 217, 60, 139, 2, 182, 81, 158, 49, 38, 96, 93, 49, 197, 88, 114, 50, 235, 119, 196, 122, 165, 157, 234, 65, 166, 237, 217, 3, 247, 96, 50, 108, 153, 156, 123, 252, 224, 187, 215, 151, 52, 160, 149, 74, 50, 125, 233, 96, 242, 124, 176, 78, 178, 23, 232, 133, 191, 213, 121, 225, 34, 220, 87, 25, 187, 26, 22, 92, 92, 249, 175, 216, 162, 190, 191, 198, 166, 49, 225, 161, 117, 215, 227, 218, 80, 32, 253, 0, 19, 26, 235, 9, 23, 198, 23, 181, 161, 152, 121, 166, 57, 189, 66, 197, 72, 229, 18, 34, 146, 179, 93, 148, 184, 51, 143, 140, 138, 94, 45, 100, 194, 200, 80, 224, 15, 154, 31, 142, 55, 72, 252, 47, 76, 235, 189, 249, 27, 126, 101, 245, 232, 46, 46, 152, 208, 23, 9, 206, 76, 174, 133, 229, 221, 146, 243, 126, 73, 8, 98, 83 }                                                                                                                                                                                                                                                                                                    },
        CrcCheck { "CRC-8-RANDOM-7", 8, 0x00000000000000e5, 0x000000000000001e, 0x00000000000000ca, false, true,  0x00000000000000ac, { 207, 120, 96, 152, 93, 112, 171, 102, 62, 189, 137, 61, 204, 42, 249, 226, 131, 164, 162, 33, 222, 75, 84, 174, 63, 71, 125, 255, 254, 135, 241, 176, 17, 184, 193, 248, 167, 247, 117, 192, 182 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   },
        CrcCheck { "CRC-8-RANDOM-8", 8, 0x0000000000000003, 0x0000000000000035, 0x0000000000000033, true,  false, 0x00000000000000d9, { 96, 249, 185, 15, 247, 136, 115, 115, 87, 117, 90, 120, 18, 197, 112, 61, 70, 87, 22, 98, 103, 241, 49, 87, 120, 119, 201, 92, 192, 109, 175, 86, 135, 157, 183, 66, 43, 21, 76, 201 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               },
        CrcCheck { "CRC-8-RANDOM-9", 8, 0x000000000000002c, 0x0000000000000094, 0x0000000000000001, false, false, 0x0000000000000095, { 52, 156, 20, 14, 1, 178, 132, 57, 220, 251, 1, 215, 195, 236, 197, 102, 193, 157, 140, 196, 132, 204, 155, 140, 185, 73, 13, 252, 175, 141, 171, 139, 221, 14, 156, 253, 107, 24, 153, 166, 217, 181, 203, 39, 172, 114, 160, 88, 197, 221, 51, 241, 70, 152, 181, 31, 88, 165, 30, 123, 231, 163, 75, 107, 55, 95, 2, 13, 70, 128, 165, 27, 224, 105, 51, 97, 76, 160, 100, 245, 174, 32, 109, 251, 43, 55, 139, 88, 89, 122, 194, 92, 245, 188, 236, 38, 211, 19, 252, 17, 209, 60, 133, 227, 36, 69, 213, 161, 162, 187, 161, 202, 3, 71, 32, 29, 131, 167, 43, 99, 175, 141, 70, 62, 3, 56, 100, 107, 165, 123, 239, 252, 219, 111, 11, 31, 216, 22, 111, 27, 7, 44, 168, 68, 216, 58, 207, 231, 94, 58, 178, 210, 149 }                                                                                                                                                                                                                                                                                                                                                                                                                                                                         },
    };

    TEST_ASSERT(!checkCrcAgainstGondenSamples(hex::crypt::crc8, golden_samples));

    TEST_SUCCESS();
};

TEST_SEQUENCE("CRC8Random") {
    TEST_ASSERT(!checkCrcAgainstRandomData(hex::crypt::crc8, 8));

    TEST_SUCCESS();
};

struct HashCheck {
    std::string data;
    std::string result;
};

template<typename Ret, typename Range>
int checkHashProviderAgainstGondenSamples(Ret (*func)(hex::prv::Provider *&, u64, size_t), Range golden_samples) {
    for (auto &i : golden_samples) {
        std::vector<u8> data(i.data.data(), i.data.data() + i.data.size());
        hex::test::TestProvider provider(&data);
        hex::prv::Provider *provider2 = &provider;
        auto res                      = func(provider2, 0, i.data.size());
        TEST_ASSERT(std::equal(std::begin(res), std::end(res), hex::crypt::decode16(i.result).begin()),
            "data: '{}' got: {} expected: {}",
            i.data,
            hex::crypt::encode16(std::vector(res.begin(), res.end())),
            i.result);
    }
    TEST_SUCCESS();
}

template<typename Ret, typename Range>
int checkHashVectorAgainstGondenSamples(Ret (*func)(const std::vector<u8> &), Range golden_samples) {
    for (auto &i : golden_samples) {
        std::vector<u8> data(i.data.data(), i.data.data() + i.data.size());
        auto res = func(data);
        TEST_ASSERT(std::equal(std::begin(res), std::end(res), hex::crypt::decode16(i.result).begin()),
            "data: '{}' got: {} expected: {}",
            i.data,
            hex::crypt::encode16(std::vector(res.begin(), res.end())),
            i.result);
    }
    TEST_SUCCESS();
}

TEST_SEQUENCE("md5") {
    std::array golden_samples = {
  // source: RFC 1321: The MD5 Message-Digest Algorithm [https://datatracker.ietf.org/doc/html/rfc1321#appendix-A.5]
        HashCheck {"",
                   "d41d8cd98f00b204e9800998ecf8427e"},
        HashCheck { "a",
                   "0cc175b9c0f1b6a831c399e269772661"},
        HashCheck { "abc",
                   "900150983cd24fb0d6963f7d28e17f72"},
        HashCheck { "message digest",
                   "f96b697d7cb7938d525a2f31aaf161d0"},
        HashCheck { "abcdefghijklmnopqrstuvwxyz",
                   "c3fcd3d76192e4007dfb496cca67e13b"},
        HashCheck { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
                   "d174ab98d277d9f5a5611c2c9f419d9f"},
        HashCheck { "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
                   "57edf4a22be3c955ac49da2e2107b67a"},
    };

    TEST_ASSERT(!checkHashProviderAgainstGondenSamples(hex::crypt::md5, golden_samples));
    TEST_ASSERT(!checkHashVectorAgainstGondenSamples(hex::crypt::md5, golden_samples));


    TEST_SUCCESS();
};

TEST_SEQUENCE("sha1") {
    std::array golden_samples = {
  // source: RFC 3174: US Secure Hash Algorithm 1 (SHA1) [https://datatracker.ietf.org/doc/html/rfc3174#section-7.3]
        HashCheck {"abc",
                   "A9993E364706816ABA3E25717850C26C9CD0D89D"},
        HashCheck { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                   "84983E441C3BD26EBAAE4AA1F95129E5E54670F1"},
    };

    TEST_ASSERT(!checkHashProviderAgainstGondenSamples(hex::crypt::sha1, golden_samples));
    TEST_ASSERT(!checkHashVectorAgainstGondenSamples(hex::crypt::sha1, golden_samples));

    TEST_SUCCESS();
};

TEST_SEQUENCE("sha224") {
    std::array golden_samples = {
  // source: RFC 3874: A 224-bit One-way Hash Function: SHA-224 [https://datatracker.ietf.org/doc/html/rfc3874#section-3]
        HashCheck {"abc",
                   "23097D223405D8228642A477BDA255B32AADBCE4BDA0B3F7E36C9DA7"},
        HashCheck { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                   "75388B16512776CC5DBA5DA1FD890150B0C6455CB4F58B1952522525"},
    };

    TEST_ASSERT(!checkHashProviderAgainstGondenSamples(hex::crypt::sha224, golden_samples));
    TEST_ASSERT(!checkHashVectorAgainstGondenSamples(hex::crypt::sha224, golden_samples));

    TEST_SUCCESS();
};

TEST_SEQUENCE("sha256") {
    std::array golden_samples = {
  // source: RFC 4634: US Secure Hash Algorithms (SHA and HMAC-SHA) [https://datatracker.ietf.org/doc/html/rfc4634#section-8.4]
        HashCheck {"abc",
                   "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"},
        HashCheck { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                   "248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1"},
    };

    TEST_ASSERT(!checkHashProviderAgainstGondenSamples(hex::crypt::sha256, golden_samples));
    TEST_ASSERT(!checkHashVectorAgainstGondenSamples(hex::crypt::sha256, golden_samples));

    TEST_SUCCESS();
};

TEST_SEQUENCE("sha384") {
    std::array golden_samples = {
  // source: RFC 4634: US Secure Hash Algorithms (SHA and HMAC-SHA) [https://datatracker.ietf.org/doc/html/rfc4634#section-8.4]
        HashCheck {"abc",
                   "CB00753F45A35E8BB5A03D699AC65007272C32AB0EDED1631A8B605A43FF5BED8086072BA1E7CC2358BAECA134C825A7"},
        HashCheck { "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
                   "09330C33F71147E83D192FC782CD1B4753111B173B3B05D22FA08086E3B0F712FCC7C71A557E2DB966C3E9FA91746039"},
    };

    TEST_ASSERT(!checkHashProviderAgainstGondenSamples(hex::crypt::sha384, golden_samples));
    TEST_ASSERT(!checkHashVectorAgainstGondenSamples(hex::crypt::sha384, golden_samples));

    TEST_SUCCESS();
};

TEST_SEQUENCE("sha512") {
    std::array golden_samples = {
  // source: RFC 4634: US Secure Hash Algorithms (SHA and HMAC-SHA) [https://datatracker.ietf.org/doc/html/rfc4634#section-8.4]
        HashCheck {"abc",
                   "DDAF35A193617ABACC417349AE20413112E6FA4E89A97EA20A9EEEE64B55D39A2192992A274FC1A836BA3C23A3FEEBBD454D4423643CE80E2A9AC94FA54CA49F"},
        HashCheck { "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
                   "8E959B75DAE313DA8CF4F72814FC143F8F7779C6EB9F7FA17299AEADB6889018501D289E4900F7E4331B99DEC4B5433AC7D329EEB6DD26545E96E55B874BE909"},
    };

    TEST_ASSERT(!checkHashProviderAgainstGondenSamples(hex::crypt::sha512, golden_samples));
    TEST_ASSERT(!checkHashVectorAgainstGondenSamples(hex::crypt::sha512, golden_samples));

    TEST_SUCCESS();
};
