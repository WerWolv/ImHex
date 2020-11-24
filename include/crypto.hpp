#pragma once

#include <hex.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace hex {

    namespace prv { class Provider; }

    u16 crc16(prv::Provider* &data, u64 offset, size_t size, u16 polynomial, u16 init);
    u32 crc32(prv::Provider* &data, u64 offset, size_t size, u32 polynomial, u32 init);

    std::array<u32, 4> md4(prv::Provider* &data, u64 offset, size_t size);
    std::array<u32, 4> md5(prv::Provider* &data, u64 offset, size_t size);
    std::array<u32, 5> sha1(prv::Provider* &data, u64 offset, size_t size);
    std::array<u32, 7> sha224(prv::Provider* &data, u64 offset, size_t size);
    std::array<u32, 8> sha256(prv::Provider* &data, u64 offset, size_t size);
    std::array<u32, 12> sha384(prv::Provider* &data, u64 offset, size_t size);
    std::array<u32, 16> sha512(prv::Provider* &data, u64 offset, size_t size);

    std::vector<u8> decode64(const std::vector<u8> &input);
    std::vector<u8> encode64(const std::vector<u8> &input);
}