#include "content/providers/intel_hex_provider.hpp"

#include <cstring>

#include <hex/api/localization_manager.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>

#include <nlohmann/json.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/expected.hpp>
#include <wolv/utils/string.hpp>

namespace hex::plugin::builtin {

    namespace intel_hex {

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

        wolv::util::Expected<std::map<u64, std::vector<u8>>, std::string> parseIntelHex(const std::string &string) {
            std::map<u64, std::vector<u8>> result;

            u8 checksum = 0x00;
            u64 offset = 0x00;

            u8  byteCount = 0x00;
            u32 segmentAddress = 0x0000'0000;
            u32 extendedLinearAddress = 0x0000'0000;
            u16 address = 0x0000;
            std::vector<u8> data;

            enum class RecordType {
                Data                    = 0x00,
                EndOfFile               = 0x01,
                ExtendedSegmentAddress  = 0x02,
                StartSegmentAddress     = 0x03,
                ExtendedLinearAddress   = 0x04,
                StartLinearAddress      = 0x05
            } recordType;

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

            bool endOfFile = false;
            try {
                while (offset < string.length()) {
                    // Parse start code
                    if (c() != ':')
                        return { };

                    checksum = 0x00;

                    if (endOfFile)
                        throw std::runtime_error("Unexpected end of file");

                    // Parse byte count
                    byteCount = parseValue(1);

                    // Parse address
                    address = parseValue(2);

                    // Parse record type
                    recordType = static_cast<RecordType>(parseValue(1));

                    data.clear();
                    for (u32 i = 0; i < byteCount; i++) {
                        data.push_back(parseValue(1));
                    }

                    parseValue(1);
                    if (!data.empty() && checksum != 0x00)
                        throw std::runtime_error("Checksum mismatch");

                    while (std::isspace(string[offset]) && offset < string.length())
                        offset++;

                    // Construct region
                    switch (recordType) {
                        case RecordType::Data: {
                            result[extendedLinearAddress | (segmentAddress + address)] = data;
                            break;
                        }
                        case RecordType::EndOfFile: {
                            endOfFile = true;
                            break;
                        }
                        case RecordType::ExtendedSegmentAddress: {
                            if (byteCount != 2)
                                throw std::runtime_error("Unexpected byte count");

                            segmentAddress = (data[0] << 8 | data[1]) * 16;
                            break;
                        }
                        case RecordType::StartSegmentAddress: {
                            if (byteCount != 4)
                                throw std::runtime_error("Unexpected byte count");

                            // Can be safely ignored
                            break;
                        }
                        case RecordType::ExtendedLinearAddress: {
                            if (byteCount != 2)
                                throw std::runtime_error("Unexpected byte count");

                            extendedLinearAddress = (data[0] << 8 | data[1]) << 16;
                            break;
                        }
                        case RecordType::StartLinearAddress: {
                            if (byteCount != 4)
                                throw std::runtime_error("Unexpected byte count");

                            // Can be safely ignored
                            break;
                        }
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

    void IntelHexProvider::setBaseAddress(u64 address) {
        auto oldBase = this->getBaseAddress();

        auto regions = m_data.overlapping({ oldBase, oldBase + this->getActualSize() });

        decltype(m_data) newIntervals;
        for (auto &[interval, data] : regions) {
            newIntervals.insert({ interval.start - oldBase + address, interval.end - oldBase + address }, *data);
        }
        m_data = newIntervals;

        Provider::setBaseAddress(address);
    }

    void IntelHexProvider::readRaw(u64 offset, void *buffer, size_t size) {
        auto intervals = m_data.overlapping({ offset, (offset + size) - 1 });

        std::memset(buffer, 0x00, size);
        auto bytes = static_cast<u8*>(buffer);
        for (const auto &[interval, data] : intervals) {
            for (u32 i = std::max(interval.start, offset); i <= interval.end && (i - offset) < size; i++) {
                bytes[i - offset] = (*data)[i - interval.start];
            }
        }
    }

    void IntelHexProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        std::ignore = offset;
        std::ignore = buffer;
        std::ignore = size;
    }

    u64 IntelHexProvider::getActualSize() const {
        return m_dataSize;
    }

    bool IntelHexProvider::open() {
        auto file = wolv::io::File(m_sourceFilePath, wolv::io::File::Mode::Read);
        if (!file.isValid()) {
            this->setErrorMessage(hex::format("hex.builtin.provider.file.error.open"_lang, m_sourceFilePath.string(), std::system_category().message(errno)));
            return false;
        }

        auto data = intel_hex::parseIntelHex(file.readString());
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

    void IntelHexProvider::close() {

    }

    [[nodiscard]] std::string IntelHexProvider::getName() const {
        return hex::format("hex.builtin.provider.intel_hex.name"_lang, wolv::util::toUTF8String(m_sourceFilePath.filename()));
    }

    [[nodiscard]] std::vector<IntelHexProvider::Description> IntelHexProvider::getDataDescription() const {
        std::vector<Description> result;

        result.emplace_back("hex.builtin.provider.file.path"_lang, wolv::util::toUTF8String(m_sourceFilePath));
        result.emplace_back("hex.builtin.provider.file.size"_lang, hex::toByteString(this->getActualSize()));

        return result;
    }

    bool IntelHexProvider::handleFilePicker() {
        auto picked = fs::openFileBrowser(fs::DialogMode::Open, {
                { "Intel Hex File", "hex" },
                { "Intel Hex File", "h86" },
                { "Intel Hex File", "hxl" },
                { "Intel Hex File", "hxh" },
                { "Intel Hex File", "obl" },
                { "Intel Hex File", "obh" },
                { "Intel Hex File", "mcs" },
                { "Intel Hex File", "ihex" },
                { "Intel Hex File", "ihe" },
                { "Intel Hex File", "ihx" },
                { "Intel Hex File", "a43" },
                { "Intel Hex File", "a90" }
            }, [this](const std::fs::path &path) {
                m_sourceFilePath = path;
            }
        );

        if (!picked)
            return false;
        if (!wolv::io::fs::isRegularFile(m_sourceFilePath))
            return false;

        return true;
    }

    std::pair<Region, bool> IntelHexProvider::getRegionValidity(u64 address) const {
        auto intervals = m_data.overlapping({ address, address });
        if (intervals.empty()) {
            return { Region(address, 1), false };
        }

        decltype(m_data)::Interval closestInterval = { 0, 0 };
        for (const auto &[interval, data] : intervals) {
            if (interval.start <= closestInterval.end)
                closestInterval = interval;
        }

        return { Region { closestInterval.start, (closestInterval.end - closestInterval.start) + 1}, Provider::getRegionValidity(address).second };
    }

    void IntelHexProvider::loadSettings(const nlohmann::json &settings) {
        Provider::loadSettings(settings);

        auto path = settings.at("path").get<std::string>();
        m_sourceFilePath = std::u8string(path.begin(), path.end());
    }

    nlohmann::json IntelHexProvider::storeSettings(nlohmann::json settings) const {
        settings["path"] = wolv::io::fs::toNormalizedPathString(m_sourceFilePath);

        return Provider::storeSettings(settings);
    }

}
