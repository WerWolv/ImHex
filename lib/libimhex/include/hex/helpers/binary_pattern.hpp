#pragma once

#include <hex.hpp>

#include <hex/helpers/utils.hpp>

#include <vector>

namespace hex {

    class BinaryPattern {
    public:
        BinaryPattern() = default;
        explicit BinaryPattern(const std::string &pattern);

        [[nodiscard]] bool isValid() const;
        [[nodiscard]] u64 getSize() const;

        [[nodiscard]] bool matches(const std::vector<u8> &bytes) const;
        [[nodiscard]] bool matchesByte(u8 byte, u32 offset) const;

        struct Pattern {
            u8 mask, value;
        };

    private:
        std::vector<Pattern> m_patterns;
    };

}