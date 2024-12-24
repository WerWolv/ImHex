#include "content/providers/motorola_srec_provider.hpp"


#include <hex/api/localization_manager.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>

#include <wolv/io/file.hpp>
#include <wolv/io/fs.hpp>
#include <wolv/utils/expected.hpp>
#include <wolv/utils/string.hpp>

namespace hex::plugin::builtin {

    namespace motorola_srec {

        u8 parseHexDigit(char c) {
            if (c >= '0' && c <= '9')
                return c - '0';
            else if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            else if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            else
                throw std::runtime_error("Failed to parse hex digit");
        }

        wolv::util::Expected<std::map<u64, std::vector<u8>>, std::string> parseMotorolaSREC(const std::string &string) {
            std::map<u64, std::vector<u8>> result;

            u64 offset = 0x00;
            u8 checksum = 0x00;
            u8 byteCount = 0x00;
            u32 address = 0x0000'0000;
            std::vector<u8> data;

            auto c = [&] {
                while (offset < string.length() && std::isspace(string[offset]))
                    offset++;

                if (offset >= string.length())
                    throw std::runtime_error("Unexpected end of file");

                return string[offset++];
            };

            auto parseValue = [&](u8 count) {
                u64 value = 0x00;
                for (u8 i = 0; i < count; i++) {
                    u8 byte = (parseHexDigit(c()) << 4) | parseHexDigit(c());
                    value <<= 8;
                    value |= byte;

                    checksum += byte;
                }

                return value;
            };

            enum class RecordType {
                Header          = 0x00,
                Data16          = 0x01,
                Data24          = 0x02,
                Data32          = 0x03,
                Reserved        = 0x04,
                Count16         = 0x05,
                Count24         = 0x06,
                StartAddress32  = 0x07,
                StartAddress24  = 0x08,
                StartAddress16  = 0x09,
            } recordType;

            bool endOfFile = false;
            try {
                while (offset < string.length()) {
                    // Parse record start
                    if (c() != 'S')
                        return { };

                    if (endOfFile)
                        throw std::runtime_error("Unexpected end of file");

                    // Parse record type
                    {
                        char typeCharacter = c();
                        if (typeCharacter < '0' || typeCharacter > '9')
                            throw std::runtime_error("Invalid record type");
                        recordType = static_cast<RecordType>(typeCharacter - '0');
                    }

                    checksum = 0x00;

                    // Parse byte count
                    byteCount = parseValue(1);

                    // Parse address
                    switch (recordType) {
                        case RecordType::Reserved:
                            break;
                        case RecordType::Header:
                        case RecordType::Data16:
                        case RecordType::Count16:
                        case RecordType::StartAddress16:
                            byteCount -= 2;
                            address = parseValue(2);
                            break;
                        case RecordType::Data24:
                        case RecordType::Count24:
                        case RecordType::StartAddress24:
                            byteCount -= 3;
                            address = parseValue(3);
                            break;
                        case RecordType::Data32:
                        case RecordType::StartAddress32:
                            byteCount -= 4;
                            address = parseValue(4);
                            break;
                    }

                    byteCount -= 1;

                    auto readData = [&byteCount, &parseValue] {
                        std::vector<u8> bytes;
                        bytes.resize(byteCount);
                        for (u8 i = 0; i < byteCount; i++) {
                            bytes[i] = parseValue(1);
                        }

                        return bytes;
                    };

                    // Parse data
                    data = readData();

                    // Parse checksum
                    {
                        auto value = parseValue(1);
                        if (((checksum - value) ^ 0xFF) != value)
                            throw std::runtime_error("Invalid checksum");
                    }

                    // Construct region
                    switch (recordType) {
                        case RecordType::Data16:
                        case RecordType::Data24:
                        case RecordType::Data32:
                            result[address] = data;
                            break;
                        case RecordType::Header:
                        case RecordType::Reserved:
                            break;
                        case RecordType::Count16:
                        case RecordType::Count24:
                            break;
                        case RecordType::StartAddress32:
                        case RecordType::StartAddress24:
                        case RecordType::StartAddress16:
                            endOfFile = true;
                            break;
                    }

                    while (std::isspace(string[offset]) && offset < string.length())
                        offset++;
                }
            } catch (const std::runtime_error &e) {
                return wolv::util::Unexpected<std::string>(e.what());
            }

            return result;
        }

    }

    bool MotorolaSRECProvider::open() {
        auto file = wolv::io::File(m_sourceFilePath, wolv::io::File::Mode::Read);
        if (!file.isValid()) {
            this->setErrorMessage(hex::format("hex.builtin.provider.file.error.open"_lang, m_sourceFilePath.string(), std::system_category().message(errno)));
            return false;
        }

        auto data = motorola_srec::parseMotorolaSREC(file.readString());
        if (!data.has_value()) {
            this->setErrorMessage(data.error());
            return false;
        }

        u64 maxAddress = 0x00;
        for (auto &[address, bytes] : data.value()) {
            auto endAddress = (address + bytes.size()) - 1;
            m_data.emplace({ address, endAddress }, std::move(bytes));

            if (endAddress > maxAddress)
                maxAddress = endAddress;
        }

        m_dataSize = maxAddress + 1;
        m_dataValid = true;

        return true;
    }

    void MotorolaSRECProvider::close() {

    }

    [[nodiscard]] std::string MotorolaSRECProvider::getName() const {
        return hex::format("hex.builtin.provider.motorola_srec.name"_lang, wolv::util::toUTF8String(m_sourceFilePath.filename()));
    }


    [[nodiscard]] std::vector<MotorolaSRECProvider::Description> MotorolaSRECProvider::getDataDescription() const {
        std::vector<Description> result;

        result.emplace_back("hex.builtin.provider.file.path"_lang, wolv::util::toUTF8String(m_sourceFilePath));
        result.emplace_back("hex.builtin.provider.file.size"_lang, hex::toByteString(this->getActualSize()));

        return result;
    }

    bool MotorolaSRECProvider::handleFilePicker() {
        auto picked = fs::openFileBrowser(fs::DialogMode::Open, {
                { "Motorola SREC File", "s19" },
                { "Motorola SREC File", "s28" },
                { "Motorola SREC File", "s37" },
                { "Motorola SREC File", "s" },
                { "Motorola SREC File", "s1" },
                { "Motorola SREC File", "s2" },
                { "Motorola SREC File", "s3" },
                { "Motorola SREC File", "sx" },
                { "Motorola SREC File", "srec" },
                { "Motorola SREC File", "exo" },
                { "Motorola SREC File", "mot" },
                { "Motorola SREC File", "mxt" }
            },
            [this](const std::fs::path &path) {
                m_sourceFilePath = path;
            }
        );

        if (!picked)
            return false;
        if (!wolv::io::fs::isRegularFile(m_sourceFilePath))
            return false;

        return true;
    }

}
