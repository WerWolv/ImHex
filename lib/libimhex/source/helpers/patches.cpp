#include <hex/helpers/patches.hpp>

#include <hex/helpers/utils.hpp>

#include <cstring>
#include <string_view>
#include <type_traits>

namespace hex {

    static void pushStringBack(std::vector<u8> &buffer, const std::string &string) {
        std::copy(string.begin(), string.end(), std::back_inserter(buffer));
    }

    template<typename T>
    static void pushBytesBack(std::vector<u8> &buffer, T bytes) {
        buffer.resize(buffer.size() + sizeof(T));
        std::memcpy((&buffer.back() - sizeof(T)) + 1, &bytes, sizeof(T));
    }

    std::expected<std::vector<u8>, IPSError> generateIPSPatch(const Patches &patches) {
        std::vector<u8> result;

        pushStringBack(result, "PATCH");

        std::vector<u64> addresses;
        std::vector<u8> values;

        for (const auto &[address, value] : patches) {
            addresses.push_back(address);
            values.push_back(value);
        }

        std::optional<u64> startAddress;
        std::vector<u8> bytes;
        for (u32 i = 0; i < addresses.size(); i++) {
            if (!startAddress.has_value())
                startAddress = addresses[i];

            if (i != addresses.size() - 1 && addresses[i] == (addresses[i + 1] - 1)) {
                bytes.push_back(values[i]);
            } else {
                bytes.push_back(values[i]);

                if (bytes.size() > 0xFFFF)
                    return std::unexpected(IPSError::PatchTooLarge);
                if (startAddress > 0xFFFF'FFFF)
                    return std::unexpected(IPSError::AddressOutOfRange);

                u32 address       = startAddress.value();
                auto addressBytes = reinterpret_cast<u8 *>(&address);

                result.push_back(addressBytes[2]);
                result.push_back(addressBytes[1]);
                result.push_back(addressBytes[0]);
                pushBytesBack<u16>(result, changeEndianess<u16>(bytes.size(), std::endian::big));

                for (auto byte : bytes)
                    result.push_back(byte);

                bytes.clear();
                startAddress = {};
            }
        }

        pushStringBack(result, "EOF");

        return result;
    }

    std::expected<std::vector<u8>, IPSError> generateIPS32Patch(const Patches &patches) {
        std::vector<u8> result;

        pushStringBack(result, "IPS32");

        std::vector<u64> addresses;
        std::vector<u8> values;

        for (const auto &[address, value] : patches) {
            addresses.push_back(address);
            values.push_back(value);
        }

        std::optional<u64> startAddress;
        std::vector<u8> bytes;
        for (u32 i = 0; i < addresses.size(); i++) {
            if (!startAddress.has_value())
                startAddress = addresses[i];

            if (i != addresses.size() - 1 && addresses[i] == (addresses[i + 1] - 1)) {
                bytes.push_back(values[i]);
            } else {
                bytes.push_back(values[i]);

                if (bytes.size() > 0xFFFF)
                    return std::unexpected(IPSError::PatchTooLarge);
                if (startAddress > 0xFFFF'FFFF)
                    return std::unexpected(IPSError::AddressOutOfRange);

                u32 address       = startAddress.value();
                auto addressBytes = reinterpret_cast<u8 *>(&address);

                result.push_back(addressBytes[3]);
                result.push_back(addressBytes[2]);
                result.push_back(addressBytes[1]);
                result.push_back(addressBytes[0]);
                pushBytesBack<u16>(result, changeEndianess<u16>(bytes.size(), std::endian::big));

                for (auto byte : bytes)
                    result.push_back(byte);

                bytes.clear();
                startAddress = {};
            }
        }

        pushStringBack(result, "EEOF");

        return result;
    }

    std::expected<Patches, IPSError> loadIPSPatch(const std::vector<u8> &ipsPatch) {
        if (ipsPatch.size() < (5 + 3))
            return std::unexpected(IPSError::InvalidPatchHeader);

        if (std::memcmp(ipsPatch.data(), "PATCH", 5) != 0)
            return std::unexpected(IPSError::InvalidPatchHeader);

        Patches result;
        bool foundEOF = false;

        u32 ipsOffset = 5;
        while (ipsOffset < ipsPatch.size() - (5 + 3)) {
            u32 offset = ipsPatch[ipsOffset + 2] | (ipsPatch[ipsOffset + 1] << 8) | (ipsPatch[ipsOffset + 0] << 16);
            u16 size   = ipsPatch[ipsOffset + 4] | (ipsPatch[ipsOffset + 3] << 8);

            ipsOffset += 5;

            // Handle normal record
            if (size > 0x0000) {
                if (ipsOffset + size > ipsPatch.size() - 3)
                    return std::unexpected(IPSError::InvalidPatchFormat);

                for (u16 i = 0; i < size; i++)
                    result[offset + i] = ipsPatch[ipsOffset + i];
                ipsOffset += size;
            }
            // Handle RLE record
            else {
                if (ipsOffset + 3 > ipsPatch.size() - 3)
                    return std::unexpected(IPSError::InvalidPatchFormat);

                u16 rleSize = ipsPatch[ipsOffset + 0] | (ipsPatch[ipsOffset + 1] << 8);

                ipsOffset += 2;

                for (u16 i = 0; i < rleSize; i++)
                    result[offset + i] = ipsPatch[ipsOffset + 0];

                ipsOffset += 1;
            }

            if (std::memcmp(ipsPatch.data() + ipsOffset, "EOF", 3) == 0)
                foundEOF = true;
        }

        if (foundEOF)
            return result;
        else
            return std::unexpected(IPSError::MissingEOF);
    }

    std::expected<Patches, IPSError> loadIPS32Patch(const std::vector<u8> &ipsPatch) {
        if (ipsPatch.size() < (5 + 4))
            return std::unexpected(IPSError::InvalidPatchHeader);

        if (std::memcmp(ipsPatch.data(), "IPS32", 5) != 0)
            return std::unexpected(IPSError::InvalidPatchHeader);

        Patches result;
        bool foundEEOF = false;

        u32 ipsOffset = 5;
        while (ipsOffset < ipsPatch.size() - (5 + 4)) {
            u32 offset = ipsPatch[ipsOffset + 3] | (ipsPatch[ipsOffset + 2] << 8) | (ipsPatch[ipsOffset + 1] << 16) | (ipsPatch[ipsOffset + 0] << 24);
            u16 size   = ipsPatch[ipsOffset + 5] | (ipsPatch[ipsOffset + 4] << 8);

            ipsOffset += 6;

            // Handle normal record
            if (size > 0x0000) {
                if (ipsOffset + size > ipsPatch.size() - 3)
                    return std::unexpected(IPSError::InvalidPatchFormat);

                for (u16 i = 0; i < size; i++)
                    result[offset + i] = ipsPatch[ipsOffset + i];
                ipsOffset += size;
            }
            // Handle RLE record
            else {
                if (ipsOffset + 3 > ipsPatch.size() - 3)
                    return std::unexpected(IPSError::InvalidPatchFormat);

                u16 rleSize = ipsPatch[ipsOffset + 0] | (ipsPatch[ipsOffset + 1] << 8);

                ipsOffset += 2;

                for (u16 i = 0; i < rleSize; i++)
                    result[offset + i] = ipsPatch[ipsOffset + 0];

                ipsOffset += 1;
            }

            if (std::memcmp(ipsPatch.data() + ipsOffset, "EEOF", 4) == 0)
                foundEEOF = true;
        }

        if (foundEEOF)
            return result;
        else
            return std::unexpected(IPSError::MissingEOF);
    }

}