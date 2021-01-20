#pragma once

#include <hex.hpp>

#include <array>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#ifdef __MINGW32__
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
    #define off64_t off_t
    #define fopen64 fopen
    #define fseeko64 fseek
    #define ftello64 ftell
#else
    template<>
    struct std::is_integral<u128> : public std::true_type { };
    template<>
    struct std::is_integral<s128> : public std::true_type { };
    template<>
    struct std::is_signed<s128>   : public std::true_type { };
#endif

#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION <= 12000
#if __has_include(<concepts>)
// Make sure we break when derived_from is implemented in libc++. Then we can fix a compatibility version above
#include <concepts>
#endif
// libcxx 12 still doesn't have many default concepts implemented, as a result we need to define it ourself using clang built-ins.
// [concept.derived] (patch from https://reviews.llvm.org/D74292)
namespace hex {
template<class _Dp, class _Bp>
concept derived_from =
  __is_base_of(_Bp, _Dp) && __is_convertible_to(const volatile _Dp*, const volatile _Bp*);
}

// [concepts.arithmetic]
namespace hex {
template<class _Tp>
concept integral = __is_integral(_Tp);

template<class _Tp>
concept signed_integral = integral<_Tp> && __is_signed(_Tp);

template<class _Tp>
concept unsigned_integral = integral<_Tp> && !signed_integral<_Tp>;

template<class _Tp>
concept floating_point = __is_floating_point(_Tp);
}
#else
// Assume supported
#include <concepts>
namespace hex {
    using std::derived_from;

    using std::integral;
    using std::signed_integral;
    using std::unsigned_integral;
    using std::floating_point;
}
#endif

#define TOKEN_CONCAT_IMPL(x, y) x ## y
#define TOKEN_CONCAT(x, y) TOKEN_CONCAT_IMPL(x, y)

namespace hex {

    template<typename ... Args>
    inline std::string format(const char *format, Args ... args) {
        ssize_t size = snprintf( nullptr, 0, format, args ... );

        if (size <= 0)
            return "";

        std::vector<char> buffer(size + 1, 0x00);
        snprintf(buffer.data(), size + 1, format, args ...);

        return std::string(buffer.data(), buffer.data() + size);
    }

    [[nodiscard]] constexpr inline u64 extract(u8 from, u8 to, const hex::unsigned_integral auto &value) {
        std::remove_cvref_t<decltype(value)> mask = (std::numeric_limits<std::remove_cvref_t<decltype(value)>>::max() >> (((sizeof(value) * 8) - 1) - (from - to))) << to;
        return (value & mask) >> to;
    }

    template<hex::integral T>
    [[nodiscard]] constexpr inline T signExtend(T value, u8 currWidth, u8 targetWidth) {
        T mask = 1LLU << (currWidth - 1);
        return (((value ^ mask) - mask) << ((sizeof(T) * 8) - targetWidth)) >> ((sizeof(T) * 8) - targetWidth);
    }

    std::string toByteString(u64 bytes);
    std::string makePrintable(char c);

    template<typename T>
    struct always_false : std::false_type {};

    template<typename T>
    constexpr T changeEndianess(T value, std::endian endian) {
        if (endian == std::endian::native)
            return value;

        if constexpr (sizeof(T) == 1)
            return value;
        else if constexpr (sizeof(T) == 2)
            return __builtin_bswap16(value);
        else if constexpr (sizeof(T) == 4)
            return __builtin_bswap32(value);
        else if constexpr (sizeof(T) == 8)
            return __builtin_bswap64(value);
        else
                static_assert(always_false<T>::value, "Invalid type provided!");
    }

    template<typename T>
    constexpr T changeEndianess(T value, size_t size, std::endian endian) {
        if (endian == std::endian::native)
            return value;

        if (size == 1)
            return value;
        else if (size == 2)
            return __builtin_bswap16(value);
        else if (size == 4)
            return __builtin_bswap32(value);
        else if (size == 8)
            return __builtin_bswap64(value);
        else if (size == 16) {
            u64 parts[2];
            std::memcpy(parts, &value, size);
            return u128(parts[1]) << 64 | u128(parts[0]);
        }
        else
            throw std::invalid_argument("Invalid value size!");
    }

    template< class T >
    constexpr T bit_width(T x) noexcept {
        return std::numeric_limits<T>::digits - std::countl_zero(x);
    }

    template<typename T>
    constexpr T bit_ceil(T x) noexcept {
        if (x <= 1u)
            return T(1);

        return T(1) << bit_width(T(x - 1));
    }

    inline std::vector<std::string> splitString(std::string_view string, std::string_view delimiter) {
        size_t start = 0, end;
        std::string token;
        std::vector<std::string> res;

        while ((end = string.find (delimiter, start)) != std::string::npos) {
            token = string.substr(start, end - start);
            start = end + delimiter.length();
            res.push_back(token);
        }

        res.push_back(std::string(string.substr(start)));
        return res;
    }

    inline std::string toEngineeringString(double value) {
        constexpr std::array prefixes = { "a", "f", "p", "n", "u", "m", "", "k", "M", "G", "T", "P", "E" };

        int8_t prefixIndex = 6;

        while (prefixIndex != 0 && prefixIndex != 12 && (value >= 1000 || value < 1) && value != 0) {
            if (value >= 1000) {
                value /= 1000;
                prefixIndex++;
            } else if (value < 1) {
                value *= 1000;
                prefixIndex--;
            }
        }

        return std::to_string(value).substr(0, 5) + prefixes[prefixIndex];
    }

    std::vector<u8> readFile(std::string_view path);

    #define SCOPE_EXIT(func) ScopeExit TOKEN_CONCAT(scopeGuard, __COUNTER__)([&] { func })
    class ScopeExit {
    public:
        ScopeExit(std::function<void()> func) : m_func(func) {}
        ~ScopeExit() { if (this->m_func != nullptr) this->m_func(); }

        void release() {
            this->m_func = nullptr;
        }

    private:
        std::function<void()> m_func;
    };

    struct Region {
        u64 address;
        size_t size;
    };

}