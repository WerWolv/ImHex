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
        std::vector<u8> nonce;
        std::vector<u8> iv;
        std::vector<u8> aad;
        std::vector<u8> input;
        std::vector<u8> tag;
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

    const AesDecryptVector Gcm = {
        .mode = hex::crypt::AESMode::GCM,
        .keyLength = hex::crypt::KeyLength::Key128Bits,
        .key = fromHex("feffe9928665731c6d6a8f9467308308"),
        .iv = fromHex("cafebabefacedbaddecaf888"),
        .aad = fromHex(
            "3ad77bb40d7a3660a89ecaf32466ef97"
            "f5d3d58503b9699de785895a96fdbaaf"
            "43b1cd7f598ece23881b00e3ed030688"
            "7b0c785e27e8ad3f8223207104725dd4"),
        .input = fromHex(
            "42831ec2217774244b7221b784d0d49c"
            "e3aa212f2c02a4e035c17e2329aca12e"
            "21d514b25466931c7d8f6a5aac84aa05"
            "1ba30b396a0aac973d58e091473f5985"),
        .tag = fromHex("64c0232904af398a5b67c10b53a5024d"),
        .expected = fromHex(
            "d9313225f88406e5a55909c5aff5269a"
            "86a7a9531534f7da2e4c303d8a318a72"
            "1c3c0c95956809532fcf0e2449a6b525"
            "b16aedf5aa0de657ba637b391aafd255")
    };

    const AesDecryptVector Ccm = {
        .mode = hex::crypt::AESMode::CCM,
        .keyLength = hex::crypt::KeyLength::Key128Bits,
        .key = fromHex("404142434445464748494a4b4c4d4e4f"),
        .nonce = fromHex("10111213141516"),
        .aad = fromHex("0001020304050607"),
        .input = fromHex("7162015b"),
        .tag = fromHex("4dac255d"),
        .expected = fromHex("20212223")
    };

    const AesDecryptVector Ofb = {
        .mode = hex::crypt::AESMode::OFB,
        .keyLength = hex::crypt::KeyLength::Key128Bits,
        .key = fromHex("2b7e151628aed2a6abf7158809cf4f3c"),
        .nonce = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 },
        .iv = { 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F },
        .input = fromHex(
            "3b3fd92eb72dad20333449f8e83cfb4a"
            "7789508d16918f03f53c52dac54ed825"
            "9740051e9c5fecf64344f7a82260edcc"
            "304c6528f659c77866a510d9c1d6ae5e"),
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

    const AesDecryptVector Ofb192 = makeSp80038aVector(
        hex::crypt::AESMode::OFB,
        hex::crypt::KeyLength::Key192Bits,
        "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
        "cdc80d6fddf18cab34c25909c99a4174"
        "fcc28b8d4c63837c09e81700c1100401"
        "8d9a9aeac0f6596f559c6d4daf59a5f2"
        "6d9f200857ca6c3e9cac524bd9acc92a");

    const AesDecryptVector Ofb256 = makeSp80038aVector(
        hex::crypt::AESMode::OFB,
        hex::crypt::KeyLength::Key256Bits,
        "603deb1015ca71be2b73aef0857d7781"
        "1f352c073b6108d72d9810a30914dff4",
        "dc7e84bfda79164b7ecd8486985d3860"
        "4febdc6740d20b3ac88f6ad82a4fb08d"
        "71ab47a086e86eedf39d1c5bba97c408"
        "0126141d67f37be8538f5a8be740e484");

    const AesDecryptVector Gcm192 = {
        .mode = hex::crypt::AESMode::GCM,
        .keyLength = hex::crypt::KeyLength::Key192Bits,
        .key = fromHex(
            "feffe9928665731c6d6a8f9467308308"
            "feffe9928665731c"),
        .iv = fromHex("cafebabefacedbaddecaf888"),
        .aad = fromHex(
            "3ad77bb40d7a3660a89ecaf32466ef97"
            "f5d3d58503b9699de785895a96fdbaaf"
            "43b1cd7f598ece23881b00e3ed030688"
            "7b0c785e27e8ad3f8223207104725dd4"),
        .input = fromHex(
            "3980ca0b3c00e841eb06fac4872a2757"
            "859e1ceaa6efd984628593b40ca1e19c"
            "7d773d00c144c525ac619d18c84a3f47"
            "18e2448b2fe324d9ccda2710acade256"),
        .tag = fromHex("3b9153b4e7318a5f3bbeac108f8a8edb"),
        .expected = fromHex(
            "d9313225f88406e5a55909c5aff5269a"
            "86a7a9531534f7da2e4c303d8a318a72"
            "1c3c0c95956809532fcf0e2449a6b525"
            "b16aedf5aa0de657ba637b391aafd255")
    };

    const AesDecryptVector Gcm256 = {
        .mode = hex::crypt::AESMode::GCM,
        .keyLength = hex::crypt::KeyLength::Key256Bits,
        .key = fromHex(
            "feffe9928665731c6d6a8f9467308308"
            "feffe9928665731c6d6a8f9467308308"),
        .iv = fromHex("cafebabefacedbaddecaf888"),
        .aad = fromHex(
            "3ad77bb40d7a3660a89ecaf32466ef97"
            "f5d3d58503b9699de785895a96fdbaaf"
            "43b1cd7f598ece23881b00e3ed030688"
            "7b0c785e27e8ad3f8223207104725dd4"),
        .input = fromHex(
            "522dc1f099567d07f47f37a32a84427d"
            "643a8cdcbfe5c0c97598a2bd2555d1aa"
            "8cb08e48590dbb3da7b08b1056828838"
            "c5f61e6393ba7a0abcc9f662898015ad"),
        .tag = fromHex("c06d76f31930fef37acae23ed465ae62"),
        .expected = fromHex(
            "d9313225f88406e5a55909c5aff5269a"
            "86a7a9531534f7da2e4c303d8a318a72"
            "1c3c0c95956809532fcf0e2449a6b525"
            "b16aedf5aa0de657ba637b391aafd255")
    };

    const AesDecryptVector Ccm192 = {
        .mode = hex::crypt::AESMode::CCM,
        .keyLength = hex::crypt::KeyLength::Key192Bits,
        .key = fromHex("19ebfde2d5468ba0a3031bde629b11fd4094afcb205393fa"),
        .nonce = fromHex("5a8aa485c316e9"),
        .input = fromHex("411986d04d6463100bff03f7d0bde7ea2c3488784378138c"),
        .tag = fromHex("ddc93a54"),
        .expected = fromHex("3796cf51b8726652a4204733b8fbb047cf00fb91a9837e22")
    };

    const AesDecryptVector Ccm256 = {
        .mode = hex::crypt::AESMode::CCM,
        .keyLength = hex::crypt::KeyLength::Key256Bits,
        .key = fromHex("af063639e66c284083c5cf72b70d8bc277f5978e80d9322d99f2fdc718cda569"),
        .nonce = fromHex("a544218dadd3c1"),
        .input = fromHex("64a1341679972dc5869fcf69b19d5c5ea50aa0b5e985f5b7"),
        .tag = fromHex("22aa8d59"),
        .expected = fromHex("d3d5424e20fbec43ae495353ed830271515ab104f8860c98")
    };

}

TEST_SEQUENCE("AESDecrypt") {
    const std::array vectors = {
        &Ecb, &Ecb192, &Ecb256,
        &Cbc, &Cbc192, &Cbc256, &CbcPkcs7,
        &Cfb128, &Cfb128192, &Cfb128256,
        &Ctr, &Ctr192, &Ctr256,
        &Gcm, &Gcm192, &Gcm256,
        &Ccm, &Ccm192, &Ccm256,
        &Ofb, &Ofb192, &Ofb256
    };

    for (const auto *vector : vectors) {
        const auto actual = hex::crypt::aesDecrypt(
            vector->mode,
            vector->keyLength,
            vector->key,
            vector->nonce,
            vector->iv,
            vector->input,
            vector->tag,
            vector->aad);

        TEST_ASSERT(actual.has_value());
        TEST_ASSERT(actual.value() == vector->expected);

        if (vector->mode == hex::crypt::AESMode::GCM || vector->mode == hex::crypt::AESMode::CCM) {
            auto alteredTag = vector->tag;
            alteredTag.front() ^= 0x01;
            TEST_ASSERT(!hex::crypt::aesDecrypt(
                vector->mode,
                vector->keyLength,
                vector->key,
                vector->nonce,
                vector->iv,
                vector->input,
                alteredTag,
                vector->aad).has_value());

            TEST_ASSERT(!hex::crypt::aesDecrypt(
                vector->mode,
                vector->keyLength,
                vector->key,
                vector->nonce,
                vector->iv,
                vector->input,
                { },
                vector->aad).has_value());

            auto alteredAad = vector->aad;
            if (alteredAad.empty())
                alteredAad.push_back(0x00);
            else
                alteredAad.front() ^= 0x01;

            TEST_ASSERT(!hex::crypt::aesDecrypt(
                vector->mode,
                vector->keyLength,
                vector->key,
                vector->nonce,
                vector->iv,
                vector->input,
                vector->tag,
                alteredAad).has_value());
        }
    }

    TEST_SUCCESS();
};
