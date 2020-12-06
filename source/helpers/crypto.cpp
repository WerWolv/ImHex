#include "helpers/crypto.hpp"

#include "providers/provider.hpp"

#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

#include <openssl/evp.h>

#include <array>
#include <span>

namespace hex {

    u16 crc16(prv::Provider* &data, u64 offset, size_t size, u16 polynomial, u16 init) {
        const auto table = [polynomial] {
            std::array<u16, 256> table;

            for (u16 i = 0; i < 256; i++) {
                u16 crc = 0;
                u16 c = i;

                for (u16 j = 0; j < 8; j++) {
                    if (((crc ^ c) & 0x0001U) != 0)
                        crc = (crc >> 1U) ^ polynomial;
                    else
                        crc >>= 1U;

                    c >>= 1U;
                }

                table[i] = crc;
            }

            return table;
        }();

        u16 crc = init;

        std::array<u8, 512> buffer = { 0 };

        for (u64 bufferOffset = 0; offset < size; offset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);

            for (size_t i = 0; i < readSize; i++) {
                crc = (crc >> 8) ^ table[(crc ^ u16(buffer[i])) & 0x00FF];
            }
        }

        return crc;
    }

    u32 crc32(prv::Provider* &data, u64 offset, size_t size, u32 polynomial, u32 init) {
        auto table = [polynomial] {
            std::array<uint32_t, 256> table = {0};

            for (uint32_t i = 0; i < 256; i++) {
                uint32_t c = i;
                for (size_t j = 0; j < 8; j++) {
                    if (c & 1)
                        c = polynomial ^ (c >> 1);
                    else
                        c >>= 1;
                }
                table[i] = c;
            }

            return table;
        }();

        uint32_t c = init;
        std::array<u8, 512> buffer = { 0 };

        for (u64 bufferOffset = 0; offset < size; offset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);

            for (size_t i = 0; i < readSize; i++) {
                c = table[(c ^ buffer[i]) & 0xFF] ^ (c >> 8);
            }
        }

        return ~c;
    }

    std::array<u32, 4> md4(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u32, 4> result = { 0 };

        MD4_CTX ctx;

        MD4_Init(&ctx);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            MD4_Update(&ctx, buffer.data(), readSize);
        }

        MD4_Final(reinterpret_cast<u8*>(result.data()), &ctx);

        return result;
    }

    std::array<u32, 4> md5(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u32, 4> result = { 0 };

        MD5_CTX ctx;

        MD5_Init(&ctx);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            MD5_Update(&ctx, buffer.data(), readSize);
        }

        MD5_Final(reinterpret_cast<u8*>(result.data()), &ctx);

        return result;
    }

    std::array<u32, 5> sha1(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u32, 5> result = { 0 };

        SHA_CTX ctx;

        SHA1_Init(&ctx);
        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            SHA1_Update(&ctx, buffer.data(), readSize);
        }

        SHA1_Final(reinterpret_cast<u8*>(result.data()), &ctx);

        return result;
    }

    std::array<u32, 7> sha224(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u32, 7> result = { 0 };

        SHA256_CTX ctx;

        SHA224_Init(&ctx);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            SHA224_Update(&ctx, buffer.data(), readSize);
        }

        SHA224_Final(reinterpret_cast<u8*>(result.data()), &ctx);

        return result;
    }

    std::array<u32, 8> sha256(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u32, 8> result = { 0 };

        SHA256_CTX ctx;

        SHA256_Init(&ctx);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            SHA256_Update(&ctx, buffer.data(), readSize);
        }

        SHA256_Final(reinterpret_cast<u8*>(result.data()), &ctx);

        return result;
    }

    std::array<u32, 12> sha384(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u32, 12> result = { 0 };

        SHA512_CTX ctx;

        SHA384_Init(&ctx);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            SHA384_Update(&ctx, buffer.data(), readSize);
        }

        SHA384_Final(reinterpret_cast<u8*>(result.data()), &ctx);

        return result;
    }

    std::array<u32, 16> sha512(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u32, 16> result = { 0 };

        SHA512_CTX ctx;

        SHA512_Init(&ctx);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            SHA512_Update(&ctx, buffer.data(), readSize);
        }

        SHA512_Final(reinterpret_cast<u8*>(result.data()), &ctx);

        return result;
    }

    std::vector<u8> decode64(const std::vector<u8> &input) {
        size_t outputSize = (3 * input.size()) / 4;
        std::vector<u8> output(outputSize + 1, 0x00);

        if (EVP_DecodeBlock(output.data(), reinterpret_cast<const unsigned char *>(input.data()), input.size()) != outputSize)
            return { };

        return output;
    }

    std::vector<u8> encode64(const std::vector<u8> &input) {
        size_t outputSize = 4 * ((input.size() + 2) / 3);
        std::vector<u8> output(outputSize + 1, 0x00);

        if (EVP_EncodeBlock(output.data(), reinterpret_cast<const unsigned char *>(input.data()), input.size()) != outputSize)
            return { };

        return output;
    }

}