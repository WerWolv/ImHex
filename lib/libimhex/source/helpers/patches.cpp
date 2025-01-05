#include <hex/helpers/patches.hpp>

#include <hex/helpers/utils.hpp>

#include <hex/providers/provider.hpp>

#include <cstring>
#include <string_view>


namespace hex {

    namespace {

        class PatchesGenerator : public hex::prv::Provider {
        public:
            explicit PatchesGenerator() = default;
            ~PatchesGenerator() override = default;

            [[nodiscard]] bool isAvailable() const override { return true; }
            [[nodiscard]] bool isReadable()  const override { return true; }
            [[nodiscard]] bool isWritable()  const override { return true; }
            [[nodiscard]] bool isResizable() const override { return true; }
            [[nodiscard]] bool isSavable()   const override { return false; }
            [[nodiscard]] bool isSavableAsRecent() const override { return false; }

            [[nodiscard]] bool open() override { return true; }
            void close() override { }

            void readRaw(u64 offset, void *buffer, size_t size) override {
                std::ignore = offset;
                std::ignore = buffer;
                std::ignore = size;
            }

            void writeRaw(u64 offset, const void *buffer, size_t size) override {
                for (u64 i = 0; i < size; i += 1)
                    m_patches[offset] = static_cast<const u8*>(buffer)[i];
            }

            [[nodiscard]] u64 getActualSize() const override {
                if (m_patches.empty())
                    return 0;
                else
                    return m_patches.rbegin()->first;
            }

            void resizeRaw(u64 newSize) override {
                std::ignore = newSize;
            }

            void insertRaw(u64 offset, u64 size) override {
                std::vector<std::pair<u64, u8>> patchesToMove;

                for (auto &[address, value] : m_patches) {
                    if (address > offset)
                        patchesToMove.emplace_back(address, value);
                }

                for (const auto &[address, value] : patchesToMove)
                    m_patches.erase(address);
                for (const auto &[address, value] : patchesToMove)
                    m_patches.insert({ address + size, value });
            }

            void removeRaw(u64 offset, u64 size) override {
                std::vector<std::pair<u64, u8>> patchesToMove;

                for (auto &[address, value] : m_patches) {
                    if (address > offset)
                        patchesToMove.emplace_back(address, value);
                }

                for (const auto &[address, value] : patchesToMove)
                    m_patches.erase(address);
                for (const auto &[address, value] : patchesToMove)
                    m_patches.insert({ address - size, value });
            }

            [[nodiscard]] std::string getName() const override {
                return "";
            }

            [[nodiscard]] UnlocalizedString getTypeName() const override { return ""; }

            const std::map<u64, u8>& getPatches() const {
                return m_patches;
            }
        private:
            std::map<u64, u8> m_patches;
        };


        void pushStringBack(std::vector<u8> &buffer, const std::string &string) {
            std::copy(string.begin(), string.end(), std::back_inserter(buffer));
        }

        template<typename T>
        void pushBytesBack(std::vector<u8> &buffer, T bytes) {
            buffer.resize(buffer.size() + sizeof(T));
            std::memcpy((&buffer.back() - sizeof(T)) + 1, &bytes, sizeof(T));
        }

    }



    wolv::util::Expected<std::vector<u8>, IPSError> Patches::toIPSPatch() const {
        std::vector<u8> result;

        pushStringBack(result, "PATCH");

        std::vector<u64> addresses;
        std::vector<u8> values;

        for (const auto &[address, value] : m_patches) {
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
                    return wolv::util::Unexpected(IPSError::PatchTooLarge);
                if (startAddress > 0xFFFF'FFFF)
                    return wolv::util::Unexpected(IPSError::AddressOutOfRange);

                u32 address       = startAddress.value();
                auto addressBytes = reinterpret_cast<u8 *>(&address);

                result.push_back(addressBytes[2]);
                result.push_back(addressBytes[1]);
                result.push_back(addressBytes[0]);
                pushBytesBack<u16>(result, changeEndianness<u16>(bytes.size(), std::endian::big));

                for (auto byte : bytes)
                    result.push_back(byte);

                bytes.clear();
                startAddress = {};
            }
        }

        pushStringBack(result, "EOF");

        return result;
    }

    wolv::util::Expected<std::vector<u8>, IPSError> Patches::toIPS32Patch() const {
        std::vector<u8> result;

        pushStringBack(result, "IPS32");

        std::vector<u64> addresses;
        std::vector<u8> values;

        for (const auto &[address, value] : m_patches) {
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
                    return wolv::util::Unexpected(IPSError::PatchTooLarge);
                if (startAddress > 0xFFFF'FFFF)
                    return wolv::util::Unexpected(IPSError::AddressOutOfRange);

                u32 address       = startAddress.value();
                auto addressBytes = reinterpret_cast<u8 *>(&address);

                result.push_back(addressBytes[3]);
                result.push_back(addressBytes[2]);
                result.push_back(addressBytes[1]);
                result.push_back(addressBytes[0]);
                pushBytesBack<u16>(result, changeEndianness<u16>(bytes.size(), std::endian::big));

                for (auto byte : bytes)
                    result.push_back(byte);

                bytes.clear();
                startAddress = {};
            }
        }

        pushStringBack(result, "EEOF");

        return result;
    }

    wolv::util::Expected<Patches, IPSError> Patches::fromProvider(hex::prv::Provider* provider) {
        PatchesGenerator generator;

        generator.getUndoStack().apply(provider->getUndoStack());

        if (generator.getActualSize() > 0xFFFF'FFFF)
            return wolv::util::Unexpected(IPSError::PatchTooLarge);

        auto patches = generator.getPatches();

        return Patches(std::move(patches));
    }


    wolv::util::Expected<Patches, IPSError> Patches::fromIPSPatch(const std::vector<u8> &ipsPatch) {
        if (ipsPatch.size() < (5 + 3))
            return wolv::util::Unexpected(IPSError::InvalidPatchHeader);

        const char *header = "PATCH";
        if (std::memcmp(ipsPatch.data(), header, 5) != 0)
            return wolv::util::Unexpected(IPSError::InvalidPatchHeader);

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
                    return wolv::util::Unexpected(IPSError::InvalidPatchFormat);

                for (u16 i = 0; i < size; i++)
                    result.get()[offset + i] = ipsPatch[ipsOffset + i];
                ipsOffset += size;
            }
            // Handle RLE record
            else {
                if (ipsOffset + 3 > ipsPatch.size() - 3)
                    return wolv::util::Unexpected(IPSError::InvalidPatchFormat);

                u16 rleSize = ipsPatch[ipsOffset + 0] | (ipsPatch[ipsOffset + 1] << 8);

                ipsOffset += 2;

                for (u16 i = 0; i < rleSize; i++)
                    result.get()[offset + i] = ipsPatch[ipsOffset + 0];

                ipsOffset += 1;
            }

            const char *footer = "EOF";
            if (std::memcmp(ipsPatch.data() + ipsOffset, footer, 3) == 0)
                foundEOF = true;
        }

        if (foundEOF)
            return result;
        else
            return wolv::util::Unexpected(IPSError::MissingEOF);
    }

    wolv::util::Expected<Patches, IPSError> Patches::fromIPS32Patch(const std::vector<u8> &ipsPatch) {
        if (ipsPatch.size() < (5 + 4))
            return wolv::util::Unexpected(IPSError::InvalidPatchHeader);

        const char *header = "IPS32";
        if (std::memcmp(ipsPatch.data(), header, 5) != 0)
            return wolv::util::Unexpected(IPSError::InvalidPatchHeader);

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
                    return wolv::util::Unexpected(IPSError::InvalidPatchFormat);

                for (u16 i = 0; i < size; i++)
                    result.get()[offset + i] = ipsPatch[ipsOffset + i];
                ipsOffset += size;
            }
            // Handle RLE record
            else {
                if (ipsOffset + 3 > ipsPatch.size() - 3)
                    return wolv::util::Unexpected(IPSError::InvalidPatchFormat);

                u16 rleSize = ipsPatch[ipsOffset + 0] | (ipsPatch[ipsOffset + 1] << 8);

                ipsOffset += 2;

                for (u16 i = 0; i < rleSize; i++)
                    result.get()[offset + i] = ipsPatch[ipsOffset + 0];

                ipsOffset += 1;
            }

            const char *footer = "EEOF";
            if (std::memcmp(ipsPatch.data() + ipsOffset, footer, 4) == 0)
                foundEEOF = true;
        }

        if (foundEEOF)
            return result;
        else
            return wolv::util::Unexpected(IPSError::MissingEOF);
    }

}