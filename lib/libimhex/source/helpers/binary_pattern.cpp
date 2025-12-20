#include <hex/helpers/binary_pattern.hpp>

#include <algorithm>

namespace hex {

    namespace {

        void skipWhitespace(std::string_view &string) {
            while (!string.empty()) {
                if (!std::isspace(string.front()))
                    break;
                string = string.substr(1);
            }
        }

        std::vector<BinaryPattern::Pattern> parseValueExpression(std::string_view &string) {
            string = string.substr(1);

            // Parse bit size number
            u64 bitSize = 0;
            std::endian endian = std::endian::little;
            while (!string.empty() && std::isdigit(string.front())) {
                bitSize *= 10;
                bitSize += string.front() - '0';

                string = string.substr(1);
                skipWhitespace(string);
            }

            if (string.starts_with("le")) {
                endian = std::endian::little;
                string = string.substr(2);
            } else if (string.starts_with("be")) {
                endian = std::endian::big;
                string = string.substr(2);
            }

            if (bitSize > 64 || bitSize % 8 != 0)
                return { };

            if (string.empty() || string.front() != '(')
                return { };

            string = string.substr(1);

            i128 value = 0x00;
            bool negative = false;
            for (u32 i = 0; !string.empty(); i++) {
                const char c = string.front();

                if (c == ')') break;
                if (i == 0 && c == '-')
                    negative = true;
                else if (i == 0 && c == '+')
                    continue;
                else if (std::isdigit(c))
                    value = value * 10 + (c - '0');
                else
                    return {};

                string = string.substr(1);
            }

            if (negative)
                value = -value;

            if (string.empty() || string.front() != ')')
                return { };

            string = string.substr(1);

            u128 resultValue = changeEndianness(value, bitSize / 8, endian);
            std::vector<BinaryPattern::Pattern> result;
            for (u32 bit = 0; bit < bitSize; bit += 8) {
                result.emplace_back(
                    0xFF,
                    u8((resultValue >> bit) & hex::bitmask(8))
                );
            }

            return result;
        }

        std::vector<BinaryPattern::Pattern> parseBinaryPatternString(std::string_view string) {
            std::vector<BinaryPattern::Pattern> result;

            if (string.length() < 2)
                return { };

            bool inString = false;
            while (!string.empty()) {
                BinaryPattern::Pattern pattern = { .mask=0, .value=0 };

                 if (string.starts_with("\"")) {
                    inString = !inString;
                    string = string.substr(1);
                    continue;
                } else if (inString) {
                    pattern = { .mask=0xFF, .value=u8(string.front()) };
                    string = string.substr(1);
                } else if (string.starts_with("u") || string.starts_with("s")) {
                    auto newPatterns = parseValueExpression(string);
                    if (newPatterns.empty())
                        return {};
                    std::ranges::move(newPatterns, std::back_inserter(result));
                    continue;
                } else if (string.starts_with("??")) {
                    pattern = { .mask=0x00, .value=0x00 };
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

    }

    BinaryPattern::BinaryPattern(const std::string &pattern) : m_patterns(parseBinaryPatternString(pattern)) { }

    bool BinaryPattern::isValid() const { return !m_patterns.empty(); }

    bool BinaryPattern::matches(const std::vector<u8> &bytes) const {
        if (bytes.size() < m_patterns.size())
            return false;

        for (u32 i = 0; i < m_patterns.size(); i++) {
            if (!this->matchesByte(bytes[i], i))
                return false;
        }

        return true;
    }

    bool BinaryPattern::matchesByte(u8 byte, u32 offset) const {
        const auto &pattern = m_patterns[offset];

        return (byte & pattern.mask) == pattern.value;
    }

    u64 BinaryPattern::getSize() const {
        return m_patterns.size();
    }

}