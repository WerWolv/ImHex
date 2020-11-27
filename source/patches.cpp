#include "patches.hpp"

#include <concepts>
#include <cstring>
#include <string_view>
#include <type_traits>

#include "utils.hpp"

namespace hex {

    static void pushBytesBack(std::vector<u8> &buffer, const char* bytes) {
        std::string_view string(bytes);
        buffer.resize(buffer.size() + string.length());
        std::memcpy((&buffer.back() - string.length()) + 1, string.begin(), string.length());
    }

    template<typename T>
    static void pushBytesBack(std::vector<u8> &buffer, T bytes) {
        buffer.resize(buffer.size() + sizeof(T));
        std::memcpy((&buffer.back() - sizeof(T)) + 1, &bytes, sizeof(T));
    }

    std::vector<u8> generateIPSPatch(const Patches &patches) {
        std::vector<u8> result;

        pushBytesBack(result, "PATCH");

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

                if (bytes.size() > 0xFFFF || startAddress > 0xFF'FFFF)
                    return { };

                u32 address = startAddress.value();
                auto addressBytes = reinterpret_cast<u8*>(&address);

                result.push_back(addressBytes[2]); result.push_back(addressBytes[1]); result.push_back(addressBytes[0]);
                pushBytesBack<u16>(result, changeEndianess<u16>(bytes.size(), std::endian::big));

                for (auto byte : bytes)
                    result.push_back(byte);

                bytes.clear();
                startAddress = { };
            }
        }

        pushBytesBack(result, "EOF");

        return result;
    }

    std::vector<u8> generateIPS32Patch(const Patches &patches) {
        std::vector<u8> result;

        pushBytesBack(result, "IPS32");

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

                if (bytes.size() > 0xFFFF || startAddress > 0xFFFF'FFFF)
                    return { };

                u32 address = startAddress.value();
                auto addressBytes = reinterpret_cast<u8*>(&address);

                result.push_back(addressBytes[3]); result.push_back(addressBytes[2]); result.push_back(addressBytes[1]); result.push_back(addressBytes[0]);
                pushBytesBack<u16>(result, changeEndianess<u16>(bytes.size(), std::endian::big));

                for (auto byte : bytes)
                    result.push_back(byte);

                bytes.clear();
                startAddress = { };
            }
        }

        pushBytesBack(result, "EEOF");

        return result;
    }

}