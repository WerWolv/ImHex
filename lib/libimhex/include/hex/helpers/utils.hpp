#pragma once

#include <hex.hpp>

#include <hex/helpers/concepts.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/intrinsics.hpp>

#include <array>
#include <bit>
#include <cstring>
#include <cctype>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#define TOKEN_CONCAT_IMPL(x, y)    x##y
#define TOKEN_CONCAT(x, y)         TOKEN_CONCAT_IMPL(x, y)
#define ANONYMOUS_VARIABLE(prefix) TOKEN_CONCAT(prefix, __COUNTER__)

struct ImVec2;

namespace hex {

    long double operator""_scaled(long double value);
    long double operator""_scaled(unsigned long long value);
    ImVec2 scaled(const ImVec2 &vector);

    std::string to_string(u128 value);
    std::string to_string(i128 value);

    std::string toByteString(u64 bytes);
    std::string makePrintable(u8 c);

    void runCommand(const std::string &command);
    void openWebpage(std::string url);

    std::string encodeByteString(const std::vector<u8> &bytes);
    std::vector<u8> decodeByteString(const std::string &string);

    [[nodiscard]] constexpr inline u64 extract(u8 from, u8 to, const std::unsigned_integral auto &value) {
        if (from < to) std::swap(from, to);

        using ValueType = std::remove_cvref_t<decltype(value)>;
        ValueType mask  = (std::numeric_limits<ValueType>::max() >> (((sizeof(value) * 8) - 1) - (from - to))) << to;

        return (value & mask) >> to;
    }

    [[nodiscard]] inline u64 extract(u32 from, u32 to, const std::vector<u8> &bytes) {
        u8 index = 0;
        while (from > 32 && to > 32) {
            from -= 8;
            to -= 8;
            index++;
        }

        u64 value = 0;
        std::memcpy(&value, &bytes[index], std::min(sizeof(value), bytes.size() - index));
        u64 mask = (std::numeric_limits<u64>::max() >> (64 - (from + 1)));

        return (value & mask) >> to;
    }

    constexpr inline i128 signExtend(size_t numBits, i128 value) {
        i128 mask = 1U << (numBits - 1);
        return (value ^ mask) - mask;
    }

    template<std::integral T>
    constexpr inline T swapBitOrder(size_t numBits, T value) {
        T result = 0x00;

        for (size_t bit = 0; bit < numBits; bit++) {
            result <<= 1;
            result |= (value & (1 << bit)) != 0;
        }

        return result;
    }

    template<class... Ts>
    struct overloaded : Ts... { using Ts::operator()...; };
    template<class... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;

    template<size_t Size>
    struct SizeTypeImpl { };

    template<>
    struct SizeTypeImpl<1> { using Type = u8; };
    template<>
    struct SizeTypeImpl<2> { using Type = u16; };
    template<>
    struct SizeTypeImpl<4> { using Type = u32; };
    template<>
    struct SizeTypeImpl<8> { using Type = u64; };
    template<>
    struct SizeTypeImpl<16> { using Type = u128; };

    template<size_t Size>
    using SizeType = typename SizeTypeImpl<Size>::Type;

    template<typename T>
    constexpr T changeEndianess(const T &value, size_t size, std::endian endian) {
        if (endian == std::endian::native)
            return value;

        size = std::min(size, sizeof(T));

        std::array<uint8_t, sizeof(T)> data = { 0 };
        std::memcpy(&data[0], &value, size);

        for (uint32_t i = 0; i < size / 2; i++) {
            std::swap(data[i], data[size - 1 - i]);
        }

        T result = { };
        std::memcpy(&result, &data[0], size);

        return result;
    }

    template<typename T>
    constexpr T changeEndianess(const T &value, std::endian endian) {
        return changeEndianess(value, sizeof(value), endian);
    }

    [[nodiscard]] constexpr u128 bitmask(u8 bits) {
        return u128(-1) >> (128 - bits);
    }

    template<class T>
    constexpr T bit_width(T x) noexcept {
        return std::numeric_limits<T>::digits - std::countl_zero(x);
    }

    template<typename T>
    constexpr T bit_ceil(T x) noexcept {
        if (x <= 1u)
            return T(1);

        return T(1) << bit_width(T(x - 1));
    }

    template<std::integral T, std::integral U>
    auto powi(T base, U exp) {
        using ResultType = decltype(T{} * U{});

        if (exp < 0)
            return ResultType(0);

        ResultType result = 1;

        while (exp != 0) {
            if ((exp & 0b1) == 0b1)
                result *= base;
            exp >>= 1;
            base *= base;
        }
        return result;
    }

    template<typename T, typename... Args>
    void moveToVector(std::vector<T> &buffer, T &&first, Args &&...rest) {
        buffer.push_back(std::move(first));

        if constexpr (sizeof...(rest) > 0)
            moveToVector(buffer, std::move(rest)...);
    }

    template<typename T, typename... Args>
    std::vector<T> moveToVector(T &&first, Args &&...rest) {
        std::vector<T> result;
        moveToVector(result, T(std::move(first)), std::move(rest)...);

        return result;
    }

    std::vector<std::string> splitString(const std::string &string, const std::string &delimiter);
    std::string combineStrings(const std::vector<std::string> &strings, const std::string &delimiter = "");

    std::string toEngineeringString(double value);

    template<typename T>
    std::vector<u8> toBytes(T value) {
        std::vector<u8> bytes(sizeof(T));
        std::memcpy(bytes.data(), &value, sizeof(T));

        return bytes;
    }

    inline std::vector<u8> parseByteString(const std::string &string) {
        auto byteString = std::string(string);
        byteString.erase(std::remove(byteString.begin(), byteString.end(), ' '), byteString.end());

        if ((byteString.length() % 2) != 0) return {};

        std::vector<u8> result;
        for (u32 i = 0; i < byteString.length(); i += 2) {
            if (!std::isxdigit(byteString[i]) || !std::isxdigit(byteString[i + 1]))
                return {};

            result.push_back(std::strtoul(byteString.substr(i, 2).c_str(), nullptr, 16));
        }

        return result;
    }

    inline std::string toBinaryString(std::unsigned_integral auto number) {
        if (number == 0) return "0";

        std::string result;
        for (i16 bit = hex::bit_width(number) - 1; bit >= 0; bit--)
            result += (number & (0b1 << bit)) == 0 ? '0' : '1';

        return result;
    }

    template<typename T>
    inline void trimLeft(std::basic_string<T> &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch) && ch >= 0x20;
        }));
    }

    template<typename T>
    inline void trimRight(std::basic_string<T> &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
            return !std::isspace(ch) && ch >= 0x20;
        }).base(), s.end());
    }

    template<typename T>
    inline void trim(std::basic_string<T> &s) {
        trimLeft(s);
        trimRight(s);
    }

    float float16ToFloat32(u16 float16);

    inline bool equalsIgnoreCase(const std::string &left, const std::string &right) {
        return std::equal(left.begin(), left.end(), right.begin(), right.end(), [](char a, char b) {
            return tolower(a) == tolower(b);
        });
    }

    inline bool containsIgnoreCase(const std::string &a, const std::string &b) {
        auto iter = std::search(a.begin(), a.end(), b.begin(), b.end(), [](char ch1, char ch2) {
            return std::toupper(ch1) == std::toupper(ch2);
        });

        return iter != a.end();
    }

    template<typename T, typename... VariantTypes>
    T get_or(const std::variant<VariantTypes...> &variant, T alt) {
        const T *value = std::get_if<T>(&variant);
        if (value == nullptr)
            return alt;
        else
            return *value;
    }

    template<std::integral T>
    T alignTo(T value, T alignment) {
        T remainder = value % alignment;

        return remainder != 0 ? value + (alignment - remainder) : value;
    }

    std::optional<u8> hexCharToValue(char c);

    bool isProcessElevated();

    std::optional<std::string> getEnvironmentVariable(const std::string &env);

    namespace scope_guard {

#define SCOPE_GUARD   ::hex::scope_guard::ScopeGuardOnExit() + [&]()
#define ON_SCOPE_EXIT [[maybe_unused]] auto ANONYMOUS_VARIABLE(SCOPE_EXIT_) = SCOPE_GUARD

        template<class F>
        class ScopeGuard {
        private:
            F m_func;
            bool m_active;

        public:
            explicit constexpr ScopeGuard(F func) : m_func(std::move(func)), m_active(true) { }
            ~ScopeGuard() {
                if (this->m_active) { this->m_func(); }
            }
            void release() { this->m_active = false; }

            ScopeGuard(ScopeGuard &&other) noexcept : m_func(std::move(other.m_func)), m_active(other.m_active) {
                other.cancel();
            }

            ScopeGuard &operator=(ScopeGuard &&) = delete;
        };

        enum class ScopeGuardOnExit
        {
        };

        template<typename F>
        constexpr ScopeGuard<F> operator+(ScopeGuardOnExit, F &&f) {
            return ScopeGuard<F>(std::forward<F>(f));
        }

    }

    namespace first_time_exec {

#define FIRST_TIME [[maybe_unused]] static auto ANONYMOUS_VARIABLE(FIRST_TIME_) = ::hex::first_time_exec::FirstTimeExecutor() + [&]()

        template<class F>
        class FirstTimeExecute {
        public:
            explicit constexpr FirstTimeExecute(F func) { func(); }

            FirstTimeExecute &operator=(FirstTimeExecute &&) = delete;
        };

        enum class FirstTimeExecutor
        {
        };

        template<typename F>
        constexpr FirstTimeExecute<F> operator+(FirstTimeExecutor, F &&f) {
            return FirstTimeExecute<F>(std::forward<F>(f));
        }

    }

    namespace final_cleanup {

#define FINAL_CLEANUP [[maybe_unused]] static auto ANONYMOUS_VARIABLE(ON_EXIT_) = ::hex::final_cleanup::FinalCleanupExecutor() + [&]()

        template<class F>
        class FinalCleanupExecute {
            F m_func;

        public:
            explicit constexpr FinalCleanupExecute(F func) : m_func(func) { }
            constexpr ~FinalCleanupExecute() { this->m_func(); }

            FinalCleanupExecute &operator=(FinalCleanupExecute &&) = delete;
        };

        enum class FinalCleanupExecutor
        {
        };

        template<typename F>
        constexpr FinalCleanupExecute<F> operator+(FinalCleanupExecutor, F &&f) {
            return FinalCleanupExecute<F>(std::forward<F>(f));
        }

    }

}
