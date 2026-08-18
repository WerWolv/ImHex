#pragma once

#include <hex/helpers/binary_pattern.hpp>

namespace hex::prv {

    class PatternMatcherMagic : public PatternMatcher<"magic"> {
    public:
        using PatternMatcher::PatternMatcher;

        bool match(const std::string& parameter) override {
            auto provider = this->getProvider();

            const auto pattern = [value = parameter]() mutable -> std::optional<BinaryPattern> {
                value = wolv::util::trim(value);

                if (value.empty())
                    return std::nullopt;

                if (!value.starts_with('['))
                    return std::nullopt;

                value = value.substr(1);

                const auto end = value.find(']');
                if (end == std::string::npos)
                    return std::nullopt;
                value.resize(end);

                value = wolv::util::trim(value);

                return BinaryPattern(value);
            }();

            const auto address = [provider, value = parameter]() mutable -> std::optional<u64> {
                value = wolv::util::trim(value);

                if (value.empty())
                    return std::nullopt;

                const auto start = value.find('@');
                if (start == std::string::npos)
                    return std::nullopt;

                value = value.substr(start + 1);
                value = wolv::util::trim(value);

                size_t end = 0;
                auto result = std::stoll(value, &end, 0);
                if (end != value.length())
                    return std::nullopt;

                if (result < 0) {
                    const auto size = provider->getActualSize();
                    if (u64(-result) > size) {
                        return std::nullopt;
                    }

                    return size + result;
                } else {
                    return result;
                }
            }();

            if (address && pattern) {
                std::vector<u8> bytes(pattern->getSize());
                if (!bytes.empty()) {
                    provider->read(*address, bytes.data(), bytes.size());

                    if (pattern->matches(bytes)) {
                        return true;
                    }
                }
            }

            return false;
        }
    };

}