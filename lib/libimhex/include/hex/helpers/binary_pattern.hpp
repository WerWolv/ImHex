#pragma once

#include <hex.hpp>

#include <hex/helpers/utils.hpp>

#include <vector>

namespace hex {

    class BinaryPattern {
    public:
        struct Pattern {
            u8 mask, value;
        };

        BinaryPattern() = default;
        explicit BinaryPattern(const std::string &pattern) : m_patterns(parseBinaryPatternString(pattern)) { }

        [[nodiscard]] bool isValid() const { return !m_patterns.empty(); }

        [[nodiscard]] bool matches(const std::vector<u8> &bytes) const {
            if (bytes.size() < m_patterns.size())
                return false;

            for (u32 i = 0; i < m_patterns.size(); i++) {
                if (!this->matchesByte(bytes[i], i))
                    return false;
            }

            return true;
        }

        [[nodiscard]] bool matchesByte(u8 byte, u32 offset) const {
            const auto &pattern = m_patterns[offset];

            return (byte & pattern.mask) == pattern.value;
        }

        [[nodiscard]] u64 getSize() const {
            return m_patterns.size();
        }

    private:
        static std::vector<Pattern> parseBinaryPatternString(std::string string) {
            std::vector<Pattern> result;

            if (string.length() < 2)
                return { };

            bool inString = false;
            while (string.length() > 0) {
                Pattern pattern = { 0, 0 };
                if (string.starts_with("\"")) {
                    inString = !inString;
                    string = string.substr(1);
                    continue;
                } else if (inString) {
                    pattern = { 0xFF, u8(string.front()) };
                    string = string.substr(1);
                } else if (string.starts_with("??")) {
                    pattern = { 0x00, 0x00 };
                    string = string.substr(2);
                } else if ((std::isxdigit(string.front()) || string.front() == '?') && string.length() >= 2) {
                    const auto hex = string.substr(0, 2);

                    for (const auto &c : hex) {
                        pattern.mask  <<= 4;
                        pattern.value <<= 4;

                        if (std::isxdigit(c)) {
                            pattern.mask |= 0x0F;

                            if (auto hexValue = hex::hexCharToValue(c); hexValue.has_value())
                                pattern.value |= hexValue.value();
                            else
                                return { };
                        } else if (c != '?') {
                            return { };
                        }
                    }

                    string = string.substr(2);
                } else if (std::isspace(string.front())) {
                    string = string.substr(1);
                    continue;
                } else {
                    return { };
                }

                result.push_back(pattern);
            }

            if (inString)
                return { };

            return result;
        }
    private:
        std::vector<Pattern> m_patterns;
    };

}