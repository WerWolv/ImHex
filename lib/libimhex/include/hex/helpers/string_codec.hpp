#pragma once

#include <hex.hpp>

#include <pl/core/string_encode_decode.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace hex {

    /**
     * @brief Decodes and encodes a PatternString's bytes for the pattern language runtime
     *
     * This is the value a pattern script sees, through ==, std::print, and other operations.
     */
    class PatternLanguageStringCodec : public pl::core::StringEncodeDecode {
    public:
        [[nodiscard]] pl::core::DecodeResult decode(std::span<const u8> bytes, std::string_view encoding,
            std::optional<size_t> maxCodepoints = std::nullopt) const override;
        [[nodiscard]] std::optional<std::vector<u8>> encode(std::string_view text, std::string_view encoding) const override;
        [[nodiscard]] std::vector<u8> encodeLossy(std::string_view text, std::string_view encoding) const override;
    };

}
