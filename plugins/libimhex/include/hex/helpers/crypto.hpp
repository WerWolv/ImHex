#pragma once

#include <hex.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace hex::prv { class Provider; }

namespace hex::crypt {

    void initialize();
    void exit();

    u16 crc16(prv::Provider* &data, u64 offset, size_t size, u16 polynomial, u16 init);
    u32 crc32(prv::Provider* &data, u64 offset, size_t size, u32 polynomial, u32 init);

    std::array<u8, 16> md5(prv::Provider* &data, u64 offset, size_t size);
    std::array<u8, 20> sha1(prv::Provider* &data, u64 offset, size_t size);
    std::array<u8, 28> sha224(prv::Provider* &data, u64 offset, size_t size);
    std::array<u8, 32> sha256(prv::Provider* &data, u64 offset, size_t size);
    std::array<u8, 48> sha384(prv::Provider* &data, u64 offset, size_t size);
    std::array<u8, 64> sha512(prv::Provider* &data, u64 offset, size_t size);

    std::vector<u8> decode64(const std::vector<u8> &input);
    std::vector<u8> encode64(const std::vector<u8> &input);

    enum class AESMode : u8 {
        ECB     = 0,
        CBC     = 1,
        CFB128  = 2,
        CTR     = 3,
        GCM     = 4,
        CCM     = 5,
        OFB     = 6,
        XTS     = 7
    };

    enum class KeyLength : u8 {
        Key128Bits = 0,
        Key192Bits = 1,
        Key256Bits = 2
    };

    std::vector<u8> aesDecrypt(AESMode mode, KeyLength keyLength, const std::vector<u8> &key, std::array<u8, 8> nonce, std::array<u8, 8> iv, const std::vector<u8> &input);
}