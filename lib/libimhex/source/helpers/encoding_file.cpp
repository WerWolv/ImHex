#include <hex/helpers/encoding_file.hpp>

#include <hex/helpers/utils.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

namespace hex {

    EncodingFile::EncodingFile(Type type, const std::fs::path &path) {
        auto file = wolv::io::File(path, wolv::io::File::Mode::Read);
        switch (type) {
            case Type::Thingy:
                parse(file.readString());
                break;
            default:
                return;
        }

        this->m_valid = true;
    }

    EncodingFile::EncodingFile(Type type, const std::string &content) {
        switch (type) {
            case Type::Thingy:
                parse(content);
                break;
            default:
                return;
        }

        this->m_valid = true;
    }

    std::pair<std::string_view, size_t> EncodingFile::getEncodingFor(std::span<u8> buffer) const {
        for (auto riter = this->m_mapping.crbegin(); riter != this->m_mapping.crend(); ++riter) {
            const auto &[size, mapping] = *riter;

            if (size > buffer.size()) continue;

            std::vector<u8> key(buffer.begin(), buffer.begin() + size);
            if (mapping.contains(key))
                return { mapping.at(key), size };
        }

        return { ".", 1 };
    }

    size_t EncodingFile::getEncodingLengthFor(std::span<u8> buffer) const {
        for (auto riter = this->m_mapping.crbegin(); riter != this->m_mapping.crend(); ++riter) {
            const auto &[size, mapping] = *riter;

            if (size > buffer.size()) continue;

            std::vector<u8> key(buffer.begin(), buffer.begin() + size);
            if (mapping.contains(key))
                return size;
        }

        return 1;
    }

    void EncodingFile::parse(const std::string &content) {
        this->m_tableContent = content;
        for (const auto &line : splitString(this->m_tableContent, "\n")) {

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
            if (to.empty())
                to = " ";

            if (!this->m_mapping.contains(fromBytes.size()))
                this->m_mapping.insert({ fromBytes.size(), {} });

            auto keySize = fromBytes.size();
            this->m_mapping[keySize].insert({ std::move(fromBytes), to });

            this->m_longestSequence = std::max(this->m_longestSequence, keySize);
        }
    }

}