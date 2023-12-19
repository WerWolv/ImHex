#pragma once

#include <hex.hpp>

#include <map>
#include <string_view>
#include <vector>
#include <span>

#include <wolv/io/fs.hpp>

namespace hex {

    class EncodingFile {
    public:
        enum class Type
        {
            Thingy
        };

        EncodingFile();
        EncodingFile(const EncodingFile &other);
        EncodingFile(EncodingFile &&other) noexcept;
        EncodingFile(Type type, const std::fs::path &path);
        EncodingFile(Type type, const std::string &content);

        EncodingFile& operator=(const EncodingFile &other);
        EncodingFile& operator=(EncodingFile &&other) noexcept;

        [[nodiscard]] std::pair<std::string_view, size_t> getEncodingFor(std::span<u8> buffer) const;
        [[nodiscard]] size_t getEncodingLengthFor(std::span<u8> buffer) const;
        [[nodiscard]] size_t getLongestSequence() const { return m_longestSequence; }

        [[nodiscard]] bool valid() const { return m_valid; }

        [[nodiscard]] const std::string& getTableContent() const { return m_tableContent; }

        [[nodiscard]] const std::string& getName() const { return m_name; }

    private:
        void parse(const std::string &content);

        bool m_valid = false;

        std::string m_name;
        std::string m_tableContent;
        std::unique_ptr<std::map<size_t, std::map<std::vector<u8>, std::string>>> m_mapping;
        size_t m_longestSequence = 0;
    };

}
