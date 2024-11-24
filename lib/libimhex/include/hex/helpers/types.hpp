#pragma once

#include <cstddef>
#include <cstdint>

#include <concepts>
#include <type_traits>

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
        u64 size;

        [[nodiscard]] constexpr bool isWithin(const Region &other) const {
            if (*this == Invalid() || other == Invalid())
                return false;

            if (this->getStartAddress() >= other.getStartAddress() && this->getEndAddress() <= other.getEndAddress())
                return true;

            return false;
        }

        [[nodiscard]] constexpr bool overlaps(const Region &other) const {
            if (*this == Invalid() || other == Invalid())
                return false;

            if (this->getEndAddress() >= other.getStartAddress() && this->getStartAddress() <= other.getEndAddress())
                return true;

            return false;
        }

        [[nodiscard]] constexpr u64 getStartAddress() const { return this->address; }
        [[nodiscard]] constexpr u64 getEndAddress() const {
            if (this->size == 0)
                return this->address;
            else
                return this->address + this->size - 1;
        }
        [[nodiscard]] constexpr size_t getSize() const { return this->size; }

        [[nodiscard]] constexpr bool operator==(const Region &other) const {
            return this->address == other.address && this->size == other.size;
        }

        constexpr static Region Invalid() {
            return { 0, 0 };
        }

        constexpr bool operator<(const Region &other) const {
            return this->address < other.address;
        }
    };


    template<typename T>
    concept Pointer = std::is_pointer_v<T>;

    template<Pointer T>
    struct NonNull {
        NonNull(T ptr) : pointer(ptr) { }
        NonNull(std::nullptr_t) = delete;
        NonNull(std::integral auto) = delete;
        NonNull(bool) = delete;

        [[nodiscard]] T get()        const { return pointer; }
        [[nodiscard]] T operator->() const { return pointer; }
        [[nodiscard]] T operator*()  const { return *pointer; }
        [[nodiscard]] operator T()   const { return pointer; }

        T pointer;
    };

}