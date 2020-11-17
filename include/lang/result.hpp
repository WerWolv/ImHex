#pragma once

#include <cstdint>

namespace hex::lang {

    class Result {
    public:
        constexpr Result(const std::uint8_t module, const std::uint32_t desc) noexcept : m_result((module << 24) | (desc & 0x00FFFFFF)) { }

        constexpr std::uint32_t getResult() const noexcept { return this->m_result; }
        constexpr std::uint8_t getModule() const noexcept { return this->m_result >> 24; }
        constexpr std::uint32_t getDescription() const noexcept { return this->m_result & 0x00FFFFFF; }

        constexpr bool succeeded() const noexcept { return this->m_result == 0; }
        constexpr bool failed() const noexcept { return !succeeded(); }
    private:
        const std::uint32_t m_result;
    };

}