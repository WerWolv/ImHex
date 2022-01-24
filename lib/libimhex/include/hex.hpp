#pragma once

#include <cstdint>
#include <cstddef>

constexpr static const auto ImHexApiURL = "https://api.werwolv.net/imhex";
constexpr static const auto GitHubApiURL = "https://api.github.com/repos/WerWolv/ImHex";

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using u128 = __uint128_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using i128 = __int128_t;

namespace hex {

    struct Region {
        u64 address;
        size_t size;
    };

}
