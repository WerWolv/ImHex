#pragma once

#include <hex.hpp>

#include <hex/helpers/concepts.hpp>

#include <array>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <nfd.hpp>

#if defined(OS_MACOS)
    #define off64_t off_t
    #define fopen64 fopen
    #define fseeko64 fseek
    #define ftello64 ftell
#endif

#define TOKEN_CONCAT_IMPL(x, y) x ## y
#define TOKEN_CONCAT(x, y) TOKEN_CONCAT_IMPL(x, y)
#define ANONYMOUS_VARIABLE(prefix) TOKEN_CONCAT(prefix, __COUNTER__)

namespace hex {

    std::string to_string(u128 value);
    std::string to_string(s128 value);

    std::string toByteString(u64 bytes);
    std::string makePrintable(u8 c);

    void runCommand(const std::string &command);
    void openWebpage(std::string url);

    [[nodiscard]] constexpr inline u64 extract(u8 from, u8 to, const hex::unsigned_integral auto &value) {
        using ValueType = std::remove_cvref_t<decltype(value)>;
        ValueType mask = (std::numeric_limits<ValueType>::max() >> (((sizeof(value) * 8) - 1) - (from - to))) << to;

        return (value & mask) >> to;
    }

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

    enum class DialogMode {
        Open,
        Save,
        Folder
    };

    void openFileBrowser(std::string_view title, DialogMode mode, const std::vector<nfdfilteritem_t> &validExtensions, const std::function<void(std::string)> &callback);

    float float16ToFloat32(u16 float16);

    namespace scope_guard {

        #define SCOPE_GUARD ::hex::scope_guard::ScopeGuardOnExit() + [&]()
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

    }

    namespace first_time_exec {

        #define FIRST_TIME static auto ANONYMOUS_VARIABLE(FIRST_TIME_) = ::hex::first_time_exec::FirstTimeExecutor() + [&]()

        template<class F>
        class FirstTimeExecute {
        public:
            constexpr FirstTimeExecute(F func) { func(); }

            FirstTimeExecute& operator=(FirstTimeExecute &&) = delete;
        };

        enum class FirstTimeExecutor { };

        template <typename F>
        constexpr FirstTimeExecute<F> operator+(FirstTimeExecutor, F&& f) {
            return FirstTimeExecute<F>(std::forward<F>(f));
        }

    }

    namespace final_cleanup {

        #define FINAL_CLEANUP static auto ANONYMOUS_VARIABLE(ON_EXIT_) = ::hex::final_cleanup::FinalCleanupExecutor() + [&]()

        template<class F>
        class FinalCleanupExecute {
            F m_func;
        public:
            constexpr FinalCleanupExecute(F func) : m_func(func) { }
            constexpr ~FinalCleanupExecute() { this->m_func(); }

            FinalCleanupExecute& operator=(FinalCleanupExecute &&) = delete;
        };

        enum class FinalCleanupExecutor { };

        template <typename F>
        constexpr FinalCleanupExecute<F> operator+(FinalCleanupExecutor, F&& f) {
            return FinalCleanupExecute<F>(std::forward<F>(f));
        }

    }

}
