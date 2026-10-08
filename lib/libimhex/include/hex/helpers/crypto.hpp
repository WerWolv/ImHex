#pragma once

#include <hex.hpp>

#include <wolv/utils/expected.hpp>

#include <array>
#include <string>
#include <vector>

#define CRYPTO_ERROR_INVALID_KEY_LENGTH (-1)
#define CRYPTO_ERROR_INVALID_MODE (-2)
#define CRYPTO_ERROR_INVALID_INPUT (-3)
#define CRYPTO_ERROR_UNSUPPORTED_MODE (-4)
#define CRYPTO_ERROR_AUTHENTICATION_FAILED (-5)

namespace hex::prv {
    class Provider;
}

namespace hex::crypt {

    void initialize();
    void exit();

    u8 crc8(prv::Provider *&data, u64 offset, size_t size, u32 polynomial, u32 init, u32 xorOut, bool reflectIn, bool reflectOut);
    u16 crc16(prv::Provider *&data, u64 offset, size_t size, u32 polynomial, u32 init, u32 xorOut, bool reflectIn, bool reflectOut);
    u32 crc32(prv::Provider *&data, u64 offset, size_t size, u32 polynomial, u32 init, u32 xorOut, bool reflectIn, bool reflectOut);

    std::array<u8, 16> md5(prv::Provider *&data, u64 offset, size_t size);
    std::array<u8, 20> sha1(prv::Provider *&data, u64 offset, size_t size);
    std::array<u8, 28> sha224(prv::Provider *&data, u64 offset, size_t size);
    std::array<u8, 32> sha256(prv::Provider *&data, u64 offset, size_t size);
    std::array<u8, 48> sha384(prv::Provider *&data, u64 offset, size_t size);
    std::array<u8, 64> sha512(prv::Provider *&data, u64 offset, size_t size);

    std::array<u8, 16> md5(const std::vector<u8> &data);
    std::array<u8, 20> sha1(const std::vector<u8> &data);
    std::array<u8, 28> sha224(const std::vector<u8> &data);
    std::array<u8, 32> sha256(const std::vector<u8> &data);
    std::array<u8, 48> sha384(const std::vector<u8> &data);
    std::array<u8, 64> sha512(const std::vector<u8> &data);

    std::vector<u8> decode64(const std::vector<u8> &input);
    std::vector<u8> encode64(const std::vector<u8> &input);
    std::vector<u8> decode16(const std::string &input);
    std::string encode16(const std::vector<u8> &input);
    std::string getErrorString(int error);

    i128 decodeSleb128(const std::vector<u8> &bytes);
    u128 decodeUleb128(const std::vector<u8> &bytes);
    std::vector<u8> encodeSleb128(i128 value);
    std::vector<u8> encodeUleb128(u128 value);

    enum class AESMode : u8 {
        ECB    = 0,
        CBC    = 1,
        CFB128 = 2,
        CTR    = 3,
        GCM    = 4,
        CCM    = 5,
        OFB    = 6,
        XTS    = 7
    };

    enum class KeyLength : u8 {
        Key128Bits = 0,
        Key192Bits = 1,
        Key256Bits = 2
    };

    wolv::util::Expected<std::vector<u8>, int> aesDecrypt(AESMode mode, KeyLength keyLength, const std::vector<u8> &key, const std::vector<u8> &nonce, const std::vector<u8> &iv, const std::vector<u8> &input, const std::vector<u8> &tag, const std::vector<u8> &aad);
}
