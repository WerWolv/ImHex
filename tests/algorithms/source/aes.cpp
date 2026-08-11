#include <hex/helpers/crypto.hpp>
#include <hex/test/tests.hpp>

#include <array>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace {

    struct AesDecryptVector {
        hex::crypt::AESMode mode;
        hex::crypt::KeyLength keyLength;
        std::vector<u8> key;
        std::array<u8, 8> nonce;
        std::array<u8, 8> iv;
        std::vector<u8> input;
        std::vector<u8> expected;
    };

    u8 fromHexDigit(char digit) {
        if (digit >= '0' && digit <= '9')
            return digit - '0';
        if (digit >= 'a' && digit <= 'f')
            return digit - 'a' + 10;
        if (digit >= 'A' && digit <= 'F')
            return digit - 'A' + 10;

        std::abort();
    }

    std::vector<u8> fromHex(std::string_view value) {
        if ((value.size() % 2) != 0)
            std::abort();

        std::vector<u8> result(value.size() / 2);
        for (size_t index = 0; index < result.size(); ++index)
            result[index] = (fromHexDigit(value[index * 2]) << 4) | fromHexDigit(value[index * 2 + 1]);

        return result;
    }

    AesDecryptVector makeSp80038aVector(
        hex::crypt::AESMode mode,
        hex::crypt::KeyLength keyLength,
        std::string_view key,
        std::string_view input) {
        AesDecryptVector vector = {
            .mode = mode,
            .keyLength = keyLength,
            .key = fromHex(key),
            .input = fromHex(input),
            .expected = fromHex(
                "6bc1bee22e409f96e93d7e117393172a"
                "ae2d8a571e03ac9c9eb76fac45af8e51"
                "30c81c46a35ce411e5fbc1191a0a52ef"
                "f69f2445df4f9b17ad2b417be66c3710")
        };

        if (mode == hex::crypt::AESMode::CTR) {
            vector.nonce = { 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7 };
            vector.iv = { 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF };
        } else if (mode != hex::crypt::AESMode::ECB) {
            vector.nonce = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
            vector.iv = { 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };
        }

        return vector;
    }

    const AesDecryptVector Ecb = {
        .mode = hex::crypt::AESMode::ECB,
        .keyLength = hex::crypt::KeyLength::Key128Bits,
        .key = fromHex("2b7e151628aed2a6abf7158809cf4f3c"),
        .input = fromHex(
            "3ad77bb40d7a3660a89ecaf32466ef97"
            "f5d3d58503b9699de785895a96fdbaaf"
            "43b1cd7f598ece23881b00e3ed030688"
            "7b0c785e27e8ad3f8223207104725dd4"),
        .expected = fromHex(
            "6bc1bee22e409f96e93d7e117393172a"
            "ae2d8a571e03ac9c9eb76fac45af8e51"
            "30c81c46a35ce411e5fbc1191a0a52ef"
            "f69f2445df4f9b17ad2b417be66c3710")
    };

    const AesDecryptVector Cbc = {
        .mode = hex::crypt::AESMode::CBC,
        .keyLength = hex::crypt::KeyLength::Key128Bits,
        .key = fromHex("2b7e151628aed2a6abf7158809cf4f3c"),
        .nonce = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 },
        .iv = { 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F },
        .input = fromHex(
            "7649abac8119b246cee98e9b12e9197d"
            "5086cb9b507219ee95db113a917678b2"
            "73bed6b8e3c1743b7116e69e22229516"
            "3ff1caa1681fac09120eca307586e1a7"),
        .expected = fromHex(
            "6bc1bee22e409f96e93d7e117393172a"
            "ae2d8a571e03ac9c9eb76fac45af8e51"
            "30c81c46a35ce411e5fbc1191a0a52ef"
            "f69f2445df4f9b17ad2b417be66c3710")
    };

    const AesDecryptVector CbcPkcs7 = {
        .mode = hex::crypt::AESMode::CBC,
        .keyLength = hex::crypt::KeyLength::Key128Bits,
        .key = fromHex("2b7e151628aed2a6abf7158809cf4f3c"),
        .nonce = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 },
        .iv = { 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F },
        .input = fromHex(
            "7649abac8119b246cee98e9b12e9197d"
            "8964e0b149c10b7b682e6e39aaeb731c"),
        .expected = fromHex(
            "6bc1bee22e409f96e93d7e117393172a"
            "10101010101010101010101010101010")
    };

    const AesDecryptVector Cfb128 = {
        .mode = hex::crypt::AESMode::CFB128,
        .keyLength = hex::crypt::KeyLength::Key128Bits,
        .key = fromHex("2b7e151628aed2a6abf7158809cf4f3c"),
        .nonce = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 },
        .iv = { 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F },
        .input = fromHex(
            "3b3fd92eb72dad20333449f8e83cfb4a"
            "c8a64537a0b3a93fcde3cdad9f1ce58b"
            "26751f67a3cbb140b1808cf187a4f4df"
            "c04b05357c5d1c0eeac4c66f9ff7f2e6"),
        .expected = fromHex(
            "6bc1bee22e409f96e93d7e117393172a"
            "ae2d8a571e03ac9c9eb76fac45af8e51"
            "30c81c46a35ce411e5fbc1191a0a52ef"
            "f69f2445df4f9b17ad2b417be66c3710")
    };

    const AesDecryptVector Ctr = {
        .mode = hex::crypt::AESMode::CTR,
        .keyLength = hex::crypt::KeyLength::Key128Bits,
        .key = fromHex("2b7e151628aed2a6abf7158809cf4f3c"),
        .nonce = { 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7 },
        .iv = { 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF },
        .input = fromHex(
            "874d6191b620e3261bef6864990db6ce"
            "9806f66b7970fdff8617187bb9fffdff"
            "5ae4df3edbd5d35e5b4f09020db03eab"
            "1e031dda2fbe03d1792170a0f3009cee"),
        .expected = fromHex(
            "6bc1bee22e409f96e93d7e117393172a"
            "ae2d8a571e03ac9c9eb76fac45af8e51"
            "30c81c46a35ce411e5fbc1191a0a52ef"
            "f69f2445df4f9b17ad2b417be66c3710")
    };

    const AesDecryptVector Ecb192 = makeSp80038aVector(
        hex::crypt::AESMode::ECB,
        hex::crypt::KeyLength::Key192Bits,
        "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
        "bd334f1d6e45f25ff712a214571fa5cc"
        "974104846d0ad3ad7734ecb3ecee4eef"
        "ef7afd2270e2e60adce0ba2face6444e"
        "9a4b41ba738d6c72fb16691603c18e0e");

    const AesDecryptVector Ecb256 = makeSp80038aVector(
        hex::crypt::AESMode::ECB,
        hex::crypt::KeyLength::Key256Bits,
        "603deb1015ca71be2b73aef0857d7781"
        "1f352c073b6108d72d9810a30914dff4",
        "f3eed1bdb5d2a03c064b5a7e3db181f8"
        "591ccb10d410ed26dc5ba74a31362870"
        "b6ed21b99ca6f4f9f153e7b1beafed1d"
        "23304b7a39f9f3ff067d8d8f9e24ecc7");

    const AesDecryptVector Cbc192 = makeSp80038aVector(
        hex::crypt::AESMode::CBC,
        hex::crypt::KeyLength::Key192Bits,
        "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
        "4f021db243bc633d7178183a9fa071e8"
        "b4d9ada9ad7dedf4e5e738763f69145a"
        "571b242012fb7ae07fa9baac3df102e0"
        "08b0e27988598881d920a9e64f5615cd");

    const AesDecryptVector Cbc256 = makeSp80038aVector(
        hex::crypt::AESMode::CBC,
        hex::crypt::KeyLength::Key256Bits,
        "603deb1015ca71be2b73aef0857d7781"
        "1f352c073b6108d72d9810a30914dff4",
        "f58c4c04d6e5f1ba779eabfb5f7bfbd6"
        "9cfc4e967edb808d679f777bc6702c7d"
        "39f23369a9d9bacfa530e26304231461"
        "b2eb05e2c39be9fcda6c19078c6a9d1b");

    const AesDecryptVector Cfb128192 = makeSp80038aVector(
        hex::crypt::AESMode::CFB128,
        hex::crypt::KeyLength::Key192Bits,
        "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
        "cdc80d6fddf18cab34c25909c99a4174"
        "67ce7f7f81173621961a2b70171d3d7a"
        "2e1e8a1dd59b88b1c8e60fed1efac4c9"
        "c05f9f9ca9834fa042ae8fba584b09ff");

    const AesDecryptVector Cfb128256 = makeSp80038aVector(
        hex::crypt::AESMode::CFB128,
        hex::crypt::KeyLength::Key256Bits,
        "603deb1015ca71be2b73aef0857d7781"
        "1f352c073b6108d72d9810a30914dff4",
        "dc7e84bfda79164b7ecd8486985d3860"
        "39ffed143b28b1c832113c6331e5407b"
        "df10132415e54b92a13ed0a8267ae2f9"
        "75a385741ab9cef82031623d55b1e471");

    const AesDecryptVector Ctr192 = makeSp80038aVector(
        hex::crypt::AESMode::CTR,
        hex::crypt::KeyLength::Key192Bits,
        "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
        "1abc932417521ca24f2b0459fe7e6e0b"
        "090339ec0aa6faefd5ccc2c6f4ce8e94"
        "1e36b26bd1ebc670d1bd1d665620abf7"
        "4f78a7f6d29809585a97daec58c6b050");

    const AesDecryptVector Ctr256 = makeSp80038aVector(
        hex::crypt::AESMode::CTR,
        hex::crypt::KeyLength::Key256Bits,
        "603deb1015ca71be2b73aef0857d7781"
        "1f352c073b6108d72d9810a30914dff4",
        "601ec313775789a5b7a7f504bbf3d228"
        "f443e3ca4d62b59aca84e990cacaf5c5"
        "2b0930daa23de94ce87017ba2d84988d"
        "dfc9c58db67aada613c2dd08457941a6");


}

TEST_SEQUENCE("AESDecrypt") {
    const std::array vectors = {
        &Ecb, &Ecb192, &Ecb256,
        &Cbc, &Cbc192, &Cbc256, &CbcPkcs7,
        &Cfb128, &Cfb128192, &Cfb128256,
        &Ctr, &Ctr192, &Ctr256
    };

    for (const auto *vector : vectors) {
        const auto actual = hex::crypt::aesDecrypt(
            vector->mode,
            vector->keyLength,
            vector->key,
            vector->nonce,
            vector->iv,
            vector->input);

        TEST_ASSERT(actual.has_value());
        TEST_ASSERT(actual.value() == vector->expected);
    }

    TEST_SUCCESS();
};
