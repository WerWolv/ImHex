#pragma once

#include <hex.hpp>

#include <map>
#include <string_view>
#include <vector>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/file.hpp>

namespace hex {

    class EncodingFile {
    public:
        enum class Type
        {
            Thingy
        };

        EncodingFile() = default;
        EncodingFile(Type type, const std::fs::path &path);

        [[nodiscard]] std::pair<std::string_view, size_t> getEncodingFor(const std::vector<u8> &buffer) const;
        [[nodiscard]] size_t getLongestSequence() const { return this->m_longestSequence; }

        [[nodiscard]] bool valid() const { return this->m_valid; }

    private:
        void parseThingyFile(fs::File &file);

        bool m_valid = false;

        std::map<size_t, std::map<std::vector<u8>, std::string>> m_mapping;
        size_t m_longestSequence = 0;
    };

}
