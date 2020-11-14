#pragma once

#include <hex.hpp>

#include <array>
#include <optional>
#include <string>
#include <memory>
#include <vector>

#include "parser/token.hpp"

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


}