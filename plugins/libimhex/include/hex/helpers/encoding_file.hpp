#pragma once

#include <hex.hpp>

#include <map>
#include <string_view>
#include <vector>

namespace hex {

    template<typename T>
    struct SizeSorter {
        bool operator() (const T& lhs, const T& rhs) const {
            return lhs.size() < rhs.size();
        }
    };

    class EncodingFile {
    public:
        enum class Type {
            Thingy,
            CSV
        };

        EncodingFile() = default;
        EncodingFile(Type type, const std::string &path);

        [[nodiscard]] std::pair<std::string_view, size_t> getEncodingFor(const std::vector<u8> &buffer) const;
        [[nodiscard]] size_t getLongestSequence() const { return this->m_longestSequence; }

        bool valid() const {
            return this->m_valid;
        }

    private:
        void parseThingyFile(std::ifstream &content);

        bool m_valid = false;

        std::map<u32, std::map<std::vector<u8>, std::string>> m_mapping;
        size_t m_longestSequence = 0;
    };

}