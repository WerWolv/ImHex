#pragma once

using u8   = std::uint8_t;
using u16  = std::uint16_t;
using u32  = std::uint32_t;
using u64  = std::uint64_t;
using u128 = __uint128_t;

using i8   = std::int8_t;
using i16  = std::int16_t;
using i32  = std::int32_t;
using i64  = std::int64_t;
using i128 = __int128_t;

using color_t = u32;

namespace hex {

    struct Region {
        u64 address;
        size_t size;

        [[nodiscard]] constexpr bool isWithin(const Region &other) const {
            return (this->getStartAddress() >= other.getStartAddress()) && (this->getEndAddress() <= other.getEndAddress());
        }

        [[nodiscard]] constexpr bool overlaps(const Region &other) const {
            return (this->getEndAddress() >= other.getStartAddress()) && (this->getStartAddress() < other.getEndAddress());
        }

        [[nodiscard]] constexpr u64 getStartAddress() const {
            return this->address;
        }

        [[nodiscard]] constexpr u64 getEndAddress() const {
            return this->address + this->size - 1;
        }

        [[nodiscard]] constexpr size_t getSize() const {
            return this->size;
        }
    };

}