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

#include <fmt/format.h>
#include <fmt/chrono.h>

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
#define ANONYMOUS_VARIABLE(prefix) TOKEN_CONCAT(prefix, __COUNTER__)

namespace hex {

    std::string to_string(u128 value);
    std::string to_string(s128 value);

    std::string toByteString(u64 bytes);
    std::string makePrintable(char c);

    void openWebpage(std::string_view url);

    template<typename ... Args>
    inline std::string format(std::string_view format, Args ... args) {
        return fmt::format(format, args...);
    }

    template<typename ... Args>
    inline void print(std::string_view format, Args ... args) {
        fmt::print(format, args...);
    }

    [[nodiscard]] constexpr inline u64 extract(u8 from, u8 to, const hex::unsigned_integral auto &value) {
        using ValueType = std::remove_cvref_t<decltype(value)>;
        ValueType mask = (std::numeric_limits<ValueType>::max() >> (((sizeof(value) * 8) - 1) - (from - to))) << to;

        return (value & mask) >> to;
    }

    template<typename T>
    struct always_false : std::false_type {};

    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

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
        else if constexpr (sizeof(T) == 16)
            return T(__builtin_bswap64(value & 0xFFFF'FFFF'FFFF'FFFF)) << 64 | __builtin_bswap64(value >> 64);
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
        else if (size == 16)
            return u128(__builtin_bswap64(u128(value) & 0xFFFF'FFFF'FFFF'FFFF)) << 64 | __builtin_bswap64(u128(value) >> 64);
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

    std::vector<std::string> splitString(std::string_view string, std::string_view delimiter);
    std::string combineStrings(const std::vector<std::string> &strings, std::string_view delimiter = "");

    std::string toEngineeringString(double value);

    std::vector<u8> readFile(std::string_view path);

    template<typename T>
    std::vector<u8> toBytes(T value) {
        std::vector<u8> bytes(sizeof(T));
        std::memcpy(bytes.data(), &value, sizeof(T));

        return bytes;
    }

    inline std::vector<u8> parseByteString(std::string_view string) {
        auto byteString = std::string(string);
        byteString.erase(std::remove(byteString.begin(), byteString.end(), ' '), byteString.end());

        if ((byteString.length() % 2) != 0) return { };

        std::vector<u8> result;
        for (u32 i = 0; i < byteString.length(); i += 2) {
            if (!std::isxdigit(byteString[i]) || !std::isxdigit(byteString[i + 1]))
                return { };

            result.push_back(std::strtoul(byteString.substr(i, 2).c_str(), nullptr, 16));
        }

        return result;
    }

    inline std::string toBinaryString(hex::unsigned_integral auto number) {
        if (number == 0) return "0";

        std::string result;
        for (s16 bit = hex::bit_width(number) - 1; bit >= 0; bit--)
            result += (number & (0b1 << bit)) == 0 ? '0' : '1';

        return result;
    }

    inline void trimLeft(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
    }

     inline void trimRight(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), s.end());
    }

    inline void trim(std::string &s) {
        trimLeft(s);
        trimRight(s);
    }

    enum class ImHexPath {
        Patterns,
        PatternsInclude,
        Magic,
        Python,
        Plugins,
        Yara,
        Config,
        Resources,
        Constants
    };

    std::vector<std::string> getPath(ImHexPath path);

    #define SCOPE_GUARD ::hex::ScopeGuardOnExit() + [&]()
    #define ON_SCOPE_EXIT auto ANONYMOUS_VARIABLE(SCOPE_EXIT_) = SCOPE_GUARD
    template<class F>
    class ScopeGuard {
    private:
        F m_func;
        bool m_active;
    public:
        constexpr ScopeGuard(F func) : m_func(std::move(func)), m_active(true) { }
        ~ScopeGuard() { if (this->m_active) { this->m_func(); } }
        void release() { this->m_active = false; }

        ScopeGuard(ScopeGuard &&other) noexcept : m_func(std::move(other.m_func)), m_active(other.m_active) {
            other.cancel();
        }

        ScopeGuard& operator=(ScopeGuard &&) = delete;
    };

    enum class ScopeGuardOnExit { };

    template <typename F>
    constexpr ScopeGuard<F> operator+(ScopeGuardOnExit, F&& f) {
        return ScopeGuard<F>(std::forward<F>(f));
    }

    struct Region {
        u64 address;
        size_t size;
    };

}