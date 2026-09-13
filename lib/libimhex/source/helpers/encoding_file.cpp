#include <hex/helpers/encoding_file.hpp>

#include <hex/helpers/unicode.hpp>

#include <hex/helpers/default_paths.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <ranges>
#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

namespace hex {

    namespace {

        /**
         * @brief Reads the right hand side of a table line
         *
         * A `\uXXXX` escape names a code point, so a table can map a byte to a character with no
         * glyph of its own, such as a control code. `\\` is one literal backslash. Any other
         * backslash stands for itself, so a table that already holds one keeps working.
         */
        std::string decodeTableValue(std::string_view value) {
            std::string result;

            for (size_t i = 0; i < value.size(); ) {
                const bool isEscape = value[i] == '\\' && (i + 1) < value.size();

                if (isEscape && value[i + 1] == '\\') {
                    result += '\\';
                    i += 2;
                    continue;
                }

                std::optional<std::string> encoded;
                if (isEscape && value[i + 1] == 'u' && (i + 6) <= value.size()) {
                    u32 codepoint = 0;
                    bool valid = true;
                    for (size_t digit = 0; digit < 4; digit += 1) {
                        const auto hexValue = hexCharToValue(value[i + 2 + digit]);
                        if (!hexValue.has_value()) {
                            valid = false;
                            break;
                        }

                        codepoint = (codepoint << 4) | *hexValue;
                    }

                    // A surrogate half is not a scalar value, so it has no encoding.
                    if (valid && (codepoint < 0xD800 || codepoint > 0xDFFF))
                        encoded = wolv::util::utf32ToUtf8(std::u32string(1, char32_t(codepoint)));
                }

                // Anything unreadable stands for itself, so a stray backslash survives.
                if (!encoded.has_value()) {
                    result += value[i];
                    i += 1;
                    continue;
                }

                result += *encoded;
                i += 6;
            }

            return result;
        }

        /**
         * @brief Finds the encodings/<stem>.tbl file, if there is one
         */
        std::optional<std::fs::path> findEncodingFile(std::string_view stem) {
            // Discards any directory part. A script reaches this with no sandbox prompt.
            const auto fileName = std::fs::path(stem).filename().string() + ".tbl";

            for (const auto &basePath : paths::Encodings.read()) {
                auto path = basePath / fileName;
                if (std::fs::is_regular_file(path))
                    return path;
            }

            return std::nullopt;
        }

    }

    namespace impl {

        void appendEncodingLineStartAddress(std::vector<u64> &lineStartAddresses, size_t line, u64 nextLineStartAddress) {
            if (line + 1 == lineStartAddresses.size())
                lineStartAddresses.push_back(nextLineStartAddress);
        }

    }

    EncodingFile::EncodingFile() :
        m_mapping(std::make_unique<std::map<size_t, std::map<std::vector<u8>, std::string>>>()),
        m_reverseMapping(std::make_unique<std::map<size_t, std::map<std::string, std::vector<u8>, std::less<>>>>()) {

    }

    EncodingFile::EncodingFile(const hex::EncodingFile &other) {
        m_mapping = std::make_unique<std::map<size_t, std::map<std::vector<u8>, std::string>>>(*other.m_mapping);
        m_reverseMapping = std::make_unique<std::map<size_t, std::map<std::string, std::vector<u8>, std::less<>>>>(*other.m_reverseMapping);
        m_tableContent = other.m_tableContent;
        m_longestSequence = other.m_longestSequence;
        m_shortestSequence = other.m_shortestSequence;
        m_ambiguousEncoding = other.m_ambiguousEncoding;
        m_valid = other.m_valid;
        m_name = other.m_name;
    }

    EncodingFile::EncodingFile(EncodingFile &&other) noexcept {
        m_mapping = std::move(other.m_mapping);
        m_reverseMapping = std::move(other.m_reverseMapping);
        m_tableContent = std::move(other.m_tableContent);
        m_longestSequence = other.m_longestSequence;
        m_shortestSequence = other.m_shortestSequence;
        m_ambiguousEncoding = other.m_ambiguousEncoding;
        m_valid = other.m_valid;
        m_name = std::move(other.m_name);
    }

    EncodingFile::EncodingFile(Type type, const std::fs::path &path) : EncodingFile() {
        auto file = wolv::io::File(path, wolv::io::File::Mode::Read);
        switch (type) {
            case Type::Thingy:
                parse(file.readString());
                break;
            default:
                return;
        }

        {
            m_name = path.stem().string();
            m_name = wolv::util::replaceStrings(m_name, "_", " ");

            if (!m_name.empty())
                m_name[0] = std::toupper(m_name[0]);
        }

        m_valid = true;
    }

    EncodingFile::EncodingFile(Type type, const std::string &content) : EncodingFile() {
        switch (type) {
            case Type::Thingy:
                parse(content);
                break;
            default:
                return;
        }

        m_name = "Unknown";
        m_valid = true;
    }


    EncodingFile &EncodingFile::operator=(const hex::EncodingFile &other) {
        if(this == &other) {
            return *this;
        }
        m_mapping = std::make_unique<std::map<size_t, std::map<std::vector<u8>, std::string>>>(*other.m_mapping);
        m_reverseMapping = std::make_unique<std::map<size_t, std::map<std::string, std::vector<u8>, std::less<>>>>(*other.m_reverseMapping);
        m_tableContent = other.m_tableContent;
        m_longestSequence = other.m_longestSequence;
        m_shortestSequence = other.m_shortestSequence;
        m_ambiguousEncoding = other.m_ambiguousEncoding;
        m_valid = other.m_valid;
        m_name = other.m_name;

        return *this;
    }

    EncodingFile &EncodingFile::operator=(EncodingFile &&other) noexcept {
        m_mapping = std::move(other.m_mapping);
        m_reverseMapping = std::move(other.m_reverseMapping);
        m_tableContent = std::move(other.m_tableContent);
        m_longestSequence = other.m_longestSequence;
        m_shortestSequence = other.m_shortestSequence;
        m_ambiguousEncoding = other.m_ambiguousEncoding;
        m_valid = other.m_valid;
        m_name = std::move(other.m_name);

        return *this;
    }



    std::optional<std::pair<std::string_view, size_t>> EncodingFile::lookup(std::span<const u8> buffer) const {
        for (const auto &[size, mapping] : std::ranges::reverse_view(*m_mapping)) {
            if (size > buffer.size()) continue;

            std::vector key(buffer.begin(), buffer.begin() + size);
            if (const auto entry = mapping.find(key); entry != mapping.end())
                return std::pair<std::string_view, size_t>{ entry->second, size };
        }

        return std::nullopt;
    }

    std::pair<std::string_view, size_t> EncodingFile::getEncodingFor(std::span<const u8> buffer) const {
        return this->lookup(buffer).value_or(std::pair<std::string_view, size_t>{ ".", 1 });
    }

    u64 EncodingFile::getEncodingLengthFor(std::span<u8> buffer) const {
        for (const auto& [size, mapping] : std::ranges::reverse_view(*m_mapping)) {
            if (size > buffer.size()) continue;

            std::vector key(buffer.begin(), buffer.begin() + size);
            if (mapping.contains(key))
                return size;
        }

        return 1;
    }

    std::string EncodingFile::decodeAll(std::span<const u8> buffer) const {
        std::string result;

        while (!buffer.empty()) {
            const auto [character, size] = getEncodingFor(buffer);
            result += character;
            buffer = buffer.subspan(size);
        }

        return result;
    }

    pl::core::DecodeResult EncodingFile::decodeBounded(std::span<const u8> buffer, std::optional<size_t> maxCodepoints) const {
        pl::core::DecodeResult result;

        while (!buffer.empty()) {
            if (maxCodepoints.has_value() && result.codepointCount >= *maxCodepoints) {
                result.stopReason = pl::core::DecodeStop::CodepointLimit;
                return result;
            }

            // Too short for even the shortest mapped sequence.
            if (buffer.size() < m_shortestSequence) {
                result.stopReason = pl::core::DecodeStop::EndOfInput;
                return result;
            }

            const auto entry = lookup(buffer);
            if (!entry.has_value()) {
                result.stopReason = pl::core::DecodeStop::MalformedBytes;
                return result;
            }

            result.text += entry->first;
            result.bytesConsumed += entry->second;
            result.codepointCount += 1;
            buffer = buffer.subspan(entry->second);
        }

        result.stopReason = pl::core::DecodeStop::EndOfInput;
        return result;
    }

    bool EncodingFile::isFullyMapped(std::span<const u8> buffer) const {
        while (!buffer.empty()) {
            const auto entry = lookup(buffer);
            if (!entry.has_value())
                return false;

            buffer = buffer.subspan(entry->second);
        }

        return true;
    }

    std::optional<std::pair<std::vector<u8>, size_t>> EncodingFile::getBytesFor(std::string_view sequence) const {
        for (const auto &[size, mapping] : std::ranges::reverse_view(*m_reverseMapping)) {
            if (size > sequence.size()) continue;

            auto key = sequence.substr(0, size);
            auto iter = mapping.find(key);
            if (iter != mapping.end())
                return std::pair { iter->second, size };
        }

        return std::nullopt;
    }

    std::optional<std::vector<u8>> EncodingFile::encodeAll(std::string_view sequence) const {
        if (!canEncode())
            return std::nullopt;

        std::vector<u8> result;

        while (!sequence.empty()) {
            auto match = getBytesFor(sequence);
            if (!match.has_value())
                return std::nullopt;

            const auto &[bytes, size] = match.value();
            result.insert(result.end(), bytes.begin(), bytes.end());
            sequence = sequence.substr(size);
        }

        return result;
    }


    void EncodingFile::parse(const std::string &content) {
        m_tableContent = content;

        // Every decoded value so far. A repeat makes the encoding ambiguous.
        std::vector<std::string_view> encodedValues;

        for (const auto &line : wolv::util::splitString(m_tableContent, "\n")) {

            std::string from, to;
            {
                auto delimiterPos = line.find('=');

                if (delimiterPos >= line.length())
                    continue;

                from = line.substr(0, delimiterPos);
                to   = line.substr(delimiterPos + 1);

                if (from.empty()) continue;
            }

            auto fromBytes = hex::parseByteString(from);
            if (fromBytes.empty()) continue;

            if (to.length() > 1)
                to = wolv::util::trim(to);
            to = decodeTableValue(to);
            if (to.empty())
                to = " ";

            if (!m_mapping->contains(fromBytes.size()))
                m_mapping->insert({ fromBytes.size(), {} });

            u64 keySize = fromBytes.size();
            u64 valueSize = to.size();

            if (!m_reverseMapping->contains(valueSize))
                m_reverseMapping->insert({ valueSize, {} });

            auto &reverseBucket = (*m_reverseMapping)[valueSize];
            auto existingEntry = reverseBucket.find(to);
            if (existingEntry == reverseBucket.end()) {
                auto iter = reverseBucket.emplace(to, fromBytes).first;
                encodedValues.emplace_back(iter->first);
            } else if (existingEntry->second != fromBytes) {
                // Two byte sequences that give one value conflict. An identical repeated line does not.
                m_ambiguousEncoding = true;
            }

            (*m_mapping)[keySize].insert({ std::move(fromBytes), to });

            m_longestSequence = std::max(m_longestSequence, keySize);
            m_shortestSequence = std::min(m_shortestSequence, keySize);
        }

        // Prefix-free is sufficient, not necessary; the full test is Sardinas-Patterson.
        if (!m_ambiguousEncoding) {
            std::ranges::sort(encodedValues);
            for (size_t i = 1; i < encodedValues.size(); i++) {
                if (encodedValues[i].starts_with(encodedValues[i - 1])) {
                    m_ambiguousEncoding = true;
                    break;
                }
            }
        }
    }


    const EncodingFile* getEncodingByName(const std::string &name) {
        static std::mutex mutex;
        static std::map<std::string, EncodingFile> encodings;

        std::scoped_lock lock(mutex);

        if (const auto entry = encodings.find(name); entry != encodings.end())
            return entry->second.valid() ? &entry->second : nullptr;

        // An encoding name is conventionally case-insensitive.
        const auto lowerCaseName = toLower(name);

        auto path = findEncodingFile(name);

        if (!path.has_value()) {
            path = findEncodingFile(lowerCaseName);
        }

        EncodingFile encoding;
        if (path.has_value())
            encoding = EncodingFile(EncodingFile::Type::Thingy, *path);

        // A failed lookup is cached too, so a bad name hits the file system once.
        const auto &result = encodings.emplace(name, std::move(encoding)).first->second;
        return result.valid() ? &result : nullptr;
    }

}
