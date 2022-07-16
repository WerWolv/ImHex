#include <hex/helpers/encoding_file.hpp>

#include <hex/helpers/utils.hpp>

namespace hex {

    EncodingFile::EncodingFile(Type type, const std::fs::path &path) {
        auto file = fs::File(path, fs::File::Mode::Read);
        switch (type) {
            case Type::Thingy:
                parseThingyFile(file);
                break;
            default:
                return;
        }

        this->m_valid = true;
    }

    std::pair<std::string_view, size_t> EncodingFile::getEncodingFor(const std::vector<u8> &buffer) const {
        for (auto riter = this->m_mapping.crbegin(); riter != this->m_mapping.crend(); ++riter) {
            const auto &[size, mapping] = *riter;

            if (size > buffer.size()) continue;

            auto key = std::vector<u8>(buffer.begin(), buffer.begin() + size);
            if (mapping.contains(key))
                return { mapping.at(key), size };
        }

        return { ".", 1 };
    }

    void EncodingFile::parseThingyFile(fs::File &file) {
        for (const auto &line : splitString(file.readString(), "\n")) {

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
                hex::trim(to);
            if (to.empty())
                to = " ";

            if (!this->m_mapping.contains(fromBytes.size()))
                this->m_mapping.insert({ fromBytes.size(), {} });
            this->m_mapping[fromBytes.size()].insert({ fromBytes, to });

            this->m_longestSequence = std::max(this->m_longestSequence, fromBytes.size());
        }
    }

}