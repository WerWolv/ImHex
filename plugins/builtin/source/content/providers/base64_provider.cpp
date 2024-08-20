#include <content/providers/base64_provider.hpp>

#include <hex/helpers/crypto.hpp>
#include <hex/helpers/utils.hpp>

namespace hex::plugin::builtin {

    void Base64Provider::readRaw(u64 offset, void *buffer, size_t size) {
        const u64 base64Offset = 4 * (offset / 3);
        const u64 base64Size = std::min<u64>(hex::alignTo<u64>(4 * (size / 3), 4) + 4, m_fileSize);

        std::vector<u8> bytes(base64Size);
        FileProvider::readRaw(base64Offset, bytes.data(), bytes.size());

        auto decoded = crypt::decode64(bytes);
        if (decoded.empty())
            return;

        u64 startOffset = offset % 3;
        std::memcpy(buffer, decoded.data() + startOffset, std::min<size_t>(decoded.size() - startOffset, size));
    }

    void Base64Provider::writeRaw(u64 offset, const void *buffer, size_t size) {
        const u64 base64Offset = 4 * (offset / 3);
        const u64 base64Size = hex::alignTo<u64>(4 * (size / 3), 4) + 4;

        std::vector<u8> bytes(base64Size);
        FileProvider::readRaw(base64Offset, bytes.data(), bytes.size());

        auto decoded = crypt::decode64(bytes);
        if (decoded.empty())
            return;

        u64 startOffset = offset % 3;
        std::memcpy(decoded.data() + startOffset, buffer, std::min<size_t>(decoded.size() - startOffset, size));

        auto encoded = crypt::encode64(decoded);
        if (encoded.empty())
            return;

        FileProvider::writeRaw(base64Offset, encoded.data(), encoded.size());
    }

    void Base64Provider::resizeRaw(u64 newSize) {
        u64 newFileLength = 4 * (newSize / 3);
        FileProvider::resizeRaw(newFileLength);
    }

    void Base64Provider::insertRaw(u64 offset, u64 size) {
        u64 newFileLength = 4 * ((getActualSize() + size) / 3);
        FileProvider::insertRaw(4 * (offset / 3), newFileLength);

        constexpr static auto NullByte = 0x00;
        for (u64 i = 0; i < size; i++)
            writeRaw(offset + i, &NullByte, 1);
    }

    void Base64Provider::removeRaw(u64 offset, u64 size) {
        u64 newFileLength = 4 * ((getActualSize() - size) / 3);
        FileProvider::removeRaw(4 * (offset / 3), newFileLength);
    }



}
