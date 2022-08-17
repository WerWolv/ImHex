#include "content/providers/intel_hex_provider.hpp"

#include <cstring>

#include <hex/api/imhex_api.hpp>
#include <hex/api/localization.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/file.hpp>
#include <hex/helpers/fmt.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin::prv {

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

        std::map<u64, std::vector<u8>> parseIntelHex(const std::string &string) {
            std::map<u64, std::vector<u8>> result;

            u8 checksum = 0x00;
            u64 offset = 0x00;

            u8 byteCount = 0x00;
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

            auto c = [&]() {
                while (std::isspace(string[offset]) && offset < string.length())
                    offset++;

                if (offset >= string.length())
                    throw std::runtime_error("Unexpected end of file");

                return string[offset++];
            };

            auto parseValue = [&](u8 byteCount) {
                u64 value = 0x00;
                for (u8 i = 0; i < byteCount; i++) {
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
                }

            } catch (const std::runtime_error &e) {
                return { };
            }

            return result;
        }

    }

    void IntelHexProvider::setBaseAddress(u64 address) {
        auto oldBase = this->getBaseAddress();

        auto intervals = this->m_data.findOverlapping(oldBase, oldBase + this->getActualSize());

        for (auto &interval : intervals) {
            interval.start = (interval.start - oldBase) + address;
            interval.stop  = (interval.stop  - oldBase) + address;
        }

        this->m_data = std::move(intervals);

        Provider::setBaseAddress(address);
    }

    void IntelHexProvider::readRaw(u64 offset, void *buffer, size_t size) {
        auto intervals = this->m_data.findOverlapping(offset, (offset + size) - 1);

        std::memset(buffer, 0x00, size);
        auto bytes = reinterpret_cast<u8*>(buffer);
        for (const auto &interval : intervals) {
            for (u32 i = std::max(interval.start, offset); i <= interval.stop && (i - offset) < size; i++) {
                bytes[i - offset] =  interval.value[i - interval.start];
            }
        }
    }

    void IntelHexProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        hex::unused(offset, buffer, size);
    }

    size_t IntelHexProvider::getActualSize() const {
        return this->m_dataSize;
    }

    bool IntelHexProvider::open() {
        auto file = fs::File(this->m_sourceFilePath, fs::File::Mode::Read);
        if (!file.isValid())
            return false;

        auto data = intel_hex::parseIntelHex(file.readString());
        if (data.empty())
            return false;

        u64 maxAddress = 0x00;
        decltype(this->m_data)::interval_vector intervals;
        for (auto &[address, bytes] : data) {
            auto endAddress = (address + bytes.size()) - 1;
            intervals.emplace_back(address, endAddress, std::move(bytes));

            if (endAddress > maxAddress)
                maxAddress = endAddress;
        }
        this->m_data = std::move(intervals);
        this->m_dataSize = maxAddress + 1;
        this->m_dataValid = true;

        return Provider::open();
    }

    void IntelHexProvider::close() {

        Provider::close();
    }

    [[nodiscard]] std::string IntelHexProvider::getName() const {
        return hex::format("hex.builtin.provider.intel_hex.name"_lang, this->m_sourceFilePath.filename().string());
    }

    bool IntelHexProvider::handleFilePicker() {
        auto picked = fs::openFileBrowser(fs::DialogMode::Open, { { "Intel Hex File", "*" } }, [this](const std::fs::path &path) {
            this->m_sourceFilePath = path;
        });
        if (!picked)
            return false;
        if (!fs::isRegularFile(this->m_sourceFilePath))
            return false;

        return true;
    }

    std::pair<Region, bool> IntelHexProvider::getRegionValidity(u64 address) const {
        auto intervals = this->m_data.findOverlapping(address, address);
        if (intervals.empty()) {
            return Provider::getRegionValidity(address);
        }

        auto closestInterval = intervals.front();
        for (const auto &interval : intervals) {
            if (interval.start < closestInterval.start)
                closestInterval = interval;
        }
        return { Region { closestInterval.start, (closestInterval.stop - closestInterval.start) + 1}, true };
    }

    void IntelHexProvider::loadSettings(const nlohmann::json &settings) {
        Provider::loadSettings(settings);

        this->m_sourceFilePath = settings["path"].get<std::string>();
    }

    nlohmann::json IntelHexProvider::storeSettings(nlohmann::json settings) const {
        settings["path"] = this->m_sourceFilePath.string();

        return Provider::storeSettings(settings);
    }

}
