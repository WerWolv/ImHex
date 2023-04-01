#pragma once

#include <hex.hpp>

#include <map>
#include <string_view>
#include <vector>
#include <span>

#include <hex/helpers/fs.hpp>

#include <wolv/io/file.hpp>

namespace hex {

    class EncodingFile {
    public:
        enum class Type
        {
            Thingy
        };

        EncodingFile() = default;
        EncodingFile(Type type, const std::fs::path &path);
        EncodingFile(Type type, const std::string &path);

        [[nodiscard]] std::pair<std::string_view, size_t> getEncodingFor(std::span<u8> buffer) const;
        [[nodiscard]] size_t getEncodingLengthFor(std::span<u8> buffer) const;
        [[nodiscard]] size_t getLongestSequence() const { return this->m_longestSequence; }

        [[nodiscard]] bool valid() const { return this->m_valid; }

        [[nodiscard]] const std::string& getTableContent() const { return this->m_tableContent; }

    private:
        void parse(const std::string &content);

        bool m_valid = false;

        std::string m_tableContent;
        std::map<size_t, std::map<std::vector<u8>, std::string>> m_mapping;
        size_t m_longestSequence = 0;
    };

}
