#include "helpers/encoding_file.hpp"

#include <hex/helpers/utils.hpp>

#include <fstream>

namespace hex {

    EncodingFile::EncodingFile(Type type, std::string_view path) {
        std::ifstream encodingFile(path.data());

        switch (type) {
            case Type::Thingy: parseThingyFile(encodingFile); break;
            default: throw std::runtime_error("Invalid encoding file type");
        }
    }

    std::pair<std::string_view, size_t> EncodingFile::getEncodingFor(const std::vector<u8> &buffer) const {
        for (auto iter = this->m_mapping.rbegin(); iter != this->m_mapping.rend(); iter++) {
            auto &[size, mapping] = *iter;

            if (size > buffer.size()) continue;

            auto key = std::vector<u8>(buffer.begin(), buffer.begin() + size);
            if (mapping.contains(key))
                return { mapping.at(key), size };
        }

        return { ".", 1 };
    }

    void EncodingFile::parseThingyFile(std::ifstream &content) {
        for (std::string line; std::getline(content, line);) {

            std::string from, to;
            {
                auto delimiterPos = line.find('=', 0);

                if (delimiterPos == std::string::npos)
                    continue;

                from = line.substr(0, delimiterPos);
                to = line.substr(delimiterPos + 1);

                hex::trim(from);
                hex::trim(to);

                if (from.empty()) continue;
                if (to.empty()) to = " ";
            }

            auto fromBytes = hex::parseByteString(from);
            if (fromBytes.empty()) continue;

            if (!this->m_mapping.contains(fromBytes.size()))
                this->m_mapping.insert({ fromBytes.size(), { } });
            this->m_mapping[fromBytes.size()].insert({ fromBytes, to });

            this->m_longestSequence = std::max(this->m_longestSequence, fromBytes.size());
        }
    }

}