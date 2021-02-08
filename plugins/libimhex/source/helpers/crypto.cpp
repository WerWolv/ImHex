#include <hex/helpers/crypto.hpp>

#include <hex/providers/provider.hpp>

#include <mbedtls/base64.h>
#include <mbedtls/md5.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/aes.h>
#include <mbedtls/cipher.h>

#include <array>
#include <span>

namespace hex::crypt {

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
        const auto table = [polynomial] {
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

    std::array<u8, 16> md5(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u8, 16> result = { 0 };

        mbedtls_md5_context ctx;
        mbedtls_md5_init(&ctx);

        mbedtls_md5_starts_ret(&ctx);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            mbedtls_md5_update_ret(&ctx, buffer.data(), readSize);
        }

        mbedtls_md5_finish_ret(&ctx, result.data());

        mbedtls_md5_free(&ctx);

        return result;
    }

    std::array<u8, 20> sha1(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u8, 20> result = { 0 };

        mbedtls_sha1_context ctx;
        mbedtls_sha1_init(&ctx);

        mbedtls_sha1_starts_ret(&ctx);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            mbedtls_sha1_update_ret(&ctx, buffer.data(), readSize);
        }

        mbedtls_sha1_finish_ret(&ctx, result.data());

        mbedtls_sha1_free(&ctx);

        return result;
    }

    std::array<u8, 28> sha224(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u8, 28> result = { 0 };

        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);

        mbedtls_sha256_starts_ret(&ctx, true);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            mbedtls_sha256_update_ret(&ctx, buffer.data(), readSize);
        }

        mbedtls_sha256_finish_ret(&ctx, result.data());

        mbedtls_sha256_free(&ctx);

        return result;
    }

    std::array<u8, 32> sha256(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u8, 32> result = { 0 };

        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);

        mbedtls_sha256_starts_ret(&ctx, false);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            mbedtls_sha256_update_ret(&ctx, buffer.data(), readSize);
        }

        mbedtls_sha256_finish_ret(&ctx, result.data());

        mbedtls_sha256_free(&ctx);

        return result;
    }

    std::array<u8, 48> sha384(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u8, 48> result = { 0 };

        mbedtls_sha512_context ctx;
        mbedtls_sha512_init(&ctx);

        mbedtls_sha512_starts_ret(&ctx, true);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            mbedtls_sha512_update_ret(&ctx, buffer.data(), readSize);
        }

        mbedtls_sha512_finish_ret(&ctx, result.data());

        mbedtls_sha512_free(&ctx);

        return result;
    }

    std::array<u8, 64> sha512(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u8, 64> result = { 0 };

        mbedtls_sha512_context ctx;
        mbedtls_sha512_init(&ctx);

        mbedtls_sha512_starts_ret(&ctx, false);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            mbedtls_sha512_update_ret(&ctx, buffer.data(), readSize);
        }

        mbedtls_sha512_finish_ret(&ctx, result.data());

        mbedtls_sha512_free(&ctx);

        return result;
    }

    std::vector<u8> decode64(const std::vector<u8> &input) {
        size_t outputSize = (3 * input.size()) / 4;
        std::vector<u8> output(outputSize + 1, 0x00);

        size_t written = 0;
        if (mbedtls_base64_decode(output.data(), output.size(), &written, reinterpret_cast<const unsigned char *>(input.data()), input.size()))
            return { };

        return output;
    }

    std::vector<u8> encode64(const std::vector<u8> &input) {
        size_t outputSize = 4 * ((input.size() + 2) / 3);
        std::vector<u8> output(outputSize + 1, 0x00);

        size_t written = 0;
        if (mbedtls_base64_encode(output.data(), output.size(), &written, reinterpret_cast<const unsigned char *>(input.data()), input.size()))
            return { };

        return output;
    }

    static std::vector<u8> aes(mbedtls_cipher_type_t type, mbedtls_operation_t operation, const std::vector<u8> &key, std::array<u8, 8> nonce, std::array<u8, 8> iv, const std::vector<u8> &input) {
        std::vector<u8> output;

        if (input.empty())
            return { };

        mbedtls_cipher_context_t ctx;
        auto cipherInfo = mbedtls_cipher_info_from_type(type);


        mbedtls_cipher_setup(&ctx, cipherInfo);
        mbedtls_cipher_setkey(&ctx, key.data(), key.size() * 8, operation);

        std::array<u8, 16> nonceCounter = { 0 };
        std::copy(nonce.begin(), nonce.end(), nonceCounter.begin());
        std::copy(iv.begin(), iv.end(), nonceCounter.begin() + 8);

        size_t outputSize = input.size() + cipherInfo->block_size;
        output.resize(outputSize, 0x00);
        mbedtls_cipher_crypt(&ctx, nonceCounter.data(), nonceCounter.size(), input.data(), input.size(), output.data(), &outputSize);

        mbedtls_cipher_free(&ctx);

        output.resize(input.size());

        return output;
    }

    std::vector<u8> aesDecrypt(AESMode mode, KeyLength keyLength, const std::vector<u8> &key, std::array<u8, 8> nonce, std::array<u8, 8> iv, const std::vector<u8> &input) {
        switch (keyLength) {
            case KeyLength::Key128Bits: if (key.size() != 128 / 8) return { }; break;
            case KeyLength::Key192Bits: if (key.size() != 192 / 8) return { }; break;
            case KeyLength::Key256Bits: if (key.size() != 256 / 8) return { }; break;
            default: return { };
        }

        mbedtls_cipher_type_t type;
        switch (mode) {
            case AESMode::ECB:      type = MBEDTLS_CIPHER_AES_128_ECB;      break;
            case AESMode::CBC:      type = MBEDTLS_CIPHER_AES_128_CBC;      break;
            case AESMode::CFB128:   type = MBEDTLS_CIPHER_AES_128_CFB128;   break;
            case AESMode::CTR:      type = MBEDTLS_CIPHER_AES_128_CTR;      break;
            case AESMode::GCM:      type = MBEDTLS_CIPHER_AES_128_GCM;      break;
            case AESMode::CCM:      type = MBEDTLS_CIPHER_AES_128_CCM;      break;
            case AESMode::OFB:      type = MBEDTLS_CIPHER_AES_128_OFB;      break;
            case AESMode::XTS:      type = MBEDTLS_CIPHER_AES_128_XTS;      break;
            default: return { };
        }

        type = mbedtls_cipher_type_t(type + u8(keyLength));

        return aes(type, MBEDTLS_DECRYPT, key, nonce, iv, input);
    }

}