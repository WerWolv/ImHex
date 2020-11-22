#pragma once

#include <hex.hpp>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifdef __MINGW32__
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif

#include "lang/token.hpp"

namespace hex {

    template<typename ... Args>
    inline std::string format(const std::string &format, Args ... args) {
        size_t size = snprintf( nullptr, 0, format.c_str(), args ... );

        if( size <= 0 )
            return "";

        std::vector<char> buffer(size + 1, 0x00);
        snprintf(buffer.data(), size + 1, format.c_str(), args ...);

        return std::string(buffer.data(), buffer.data() + size);
    }

    [[nodiscard]] constexpr inline u64 signExtend(u64 value, u8 currWidth, u8 targetWidth) {
        u64 mask = 1LLU << (currWidth - 1);
        return (((value ^ mask) - mask) << (64 - targetWidth)) >> (64 - targetWidth);
    }

    constexpr inline bool isUnsigned(const lang::Token::TypeToken::Type type) {
        return (static_cast<u32>(type) & 0x0F) == 0x00;
    }

    constexpr inline bool isSigned(const lang::Token::TypeToken::Type type) {
        return (static_cast<u32>(type) & 0x0F) == 0x01;
    }

    constexpr inline bool isFloatingPoint(const lang::Token::TypeToken::Type type) {
        return (static_cast<u32>(type) & 0x0F) == 0x02;
    }

    constexpr inline u32 getTypeSize(const lang::Token::TypeToken::Type type) {
        return static_cast<u32>(type) >> 4;
    }

    inline std::string toByteString(u64 bytes) {
        double value = bytes;
        u8 unitIndex = 0;

        while (value > 1024) {
            value /= 1024;
            unitIndex++;

            if (unitIndex == 6)
                break;
        }

        std::string result = hex::format("%.2f", value);

        switch (unitIndex) {
            case 0: result += " Bytes"; break;
            case 1: result += " kB"; break;
            case 2: result += " MB"; break;
            case 3: result += " GB"; break;
            case 4: result += " TB"; break;
            case 5: result += " PB"; break;
            case 6: result += " EB"; break;
            default: result = "A lot!";
        }

        return result;
    }

    [[nodiscard]] constexpr inline u64 extract(u8 from, u8 to, const u64 &value) {
        u64 mask = (std::numeric_limits<u64>::max() >> (63 - (from - to))) << to;
        return (value & mask) >> to;
    }

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
        else
            throw std::invalid_argument("Invalid value size!");
    }


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
}