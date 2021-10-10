#include <hex/helpers/crypto.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>

#include <mbedtls/version.h>
#include <mbedtls/base64.h>
#include <mbedtls/bignum.h>
#include <mbedtls/md5.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/aes.h>
#include <mbedtls/cipher.h>

#include <boost/crc.hpp>

#include <array>
#include <span>

#if MBEDTLS_VERSION_MAJOR <= 2

#define mbedtls_md5_starts mbedtls_md5_starts_ret
#define mbedtls_md5_update mbedtls_md5_update_ret
#define mbedtls_md5_finish mbedtls_md5_finish_ret

#define mbedtls_sha1_starts mbedtls_sha1_starts_ret
#define mbedtls_sha1_update mbedtls_sha1_update_ret
#define mbedtls_sha1_finish mbedtls_sha1_finish_ret

#define mbedtls_sha256_starts mbedtls_sha256_starts_ret
#define mbedtls_sha256_update mbedtls_sha256_update_ret
#define mbedtls_sha256_finish mbedtls_sha256_finish_ret

#define mbedtls_sha512_starts mbedtls_sha512_starts_ret
#define mbedtls_sha512_update mbedtls_sha512_update_ret
#define mbedtls_sha512_finish mbedtls_sha512_finish_ret

#endif

namespace hex::crypt {

    u16 crc8(prv::Provider* &data, u64 offset, size_t size, u32 polynomial, u32 init,  u32 xorout, bool reflectIn, bool reflectOut) {
        boost::crc_basic<8> crc(polynomial, init, xorout, reflectIn, reflectOut);

        std::array<u8, 512> buffer = { 0 };

        for (u64 bufferOffset = 0; offset < size; offset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);

            crc.process_bytes(buffer.data(), readSize);
        }

        return crc.checksum();
    }

    u16 crc16(prv::Provider* &data, u64 offset, size_t size, u32 polynomial, u32 init,  u32 xorout, bool reflectIn, bool reflectOut) {
        boost::crc_basic<16> crc(polynomial, init, xorout, reflectIn, reflectOut);

        std::array<u8, 512> buffer = { 0 };

        for (u64 bufferOffset = 0; offset < size; offset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);

            crc.process_bytes(buffer.data(), readSize);
        }

        return crc.checksum();
    }

    u32 crc32(prv::Provider* &data, u64 offset, size_t size, u32 polynomial, u32 init,  u32 xorout, bool reflectIn, bool reflectOut) {
        boost::crc_basic<32> crc(polynomial, init, xorout, reflectIn, reflectOut);

        std::array<u8, 512> buffer = { 0 };

        for (u64 bufferOffset = 0; offset < size; offset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);

            crc.process_bytes(buffer.data(), readSize);
        }

        return crc.checksum();
    }


    std::array<u8, 16> md5(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u8, 16> result = { 0 };

        mbedtls_md5_context ctx;
        mbedtls_md5_init(&ctx);

        mbedtls_md5_starts(&ctx);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            mbedtls_md5_update(&ctx, buffer.data(), readSize);
        }

        mbedtls_md5_finish(&ctx, result.data());

        mbedtls_md5_free(&ctx);

        return result;
    }

    std::array<u8, 16> md5(const std::vector<u8> &data) {
        std::array<u8, 16> result = { 0 };

        mbedtls_md5_context ctx;
        mbedtls_md5_init(&ctx);

        mbedtls_md5_starts(&ctx);
        mbedtls_md5_update(&ctx, data.data(), data.size());
        mbedtls_md5_finish(&ctx, result.data());

        mbedtls_md5_free(&ctx);

        return result;
    }

    std::array<u8, 20> sha1(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u8, 20> result = { 0 };

        mbedtls_sha1_context ctx;
        mbedtls_sha1_init(&ctx);

        mbedtls_sha1_starts(&ctx);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            mbedtls_sha1_update(&ctx, buffer.data(), readSize);
        }

        mbedtls_sha1_finish(&ctx, result.data());

        mbedtls_sha1_free(&ctx);

        return result;
    }

    std::array<u8, 20> sha1(const std::vector<u8> &data) {
        std::array<u8, 20> result = { 0 };

        mbedtls_sha1_context ctx;
        mbedtls_sha1_init(&ctx);

        mbedtls_sha1_starts(&ctx);
        mbedtls_sha1_update(&ctx, data.data(), data.size());
        mbedtls_sha1_finish(&ctx, result.data());

        mbedtls_sha1_free(&ctx);

        return result;
    }

    std::array<u8, 28> sha224(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u8, 28> result = { 0 };

        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);

        mbedtls_sha256_starts(&ctx, true);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            mbedtls_sha256_update(&ctx, buffer.data(), readSize);
        }

        mbedtls_sha256_finish(&ctx, result.data());

        mbedtls_sha256_free(&ctx);

        return result;
    }

    std::array<u8, 28> sha224(const std::vector<u8> &data) {
        std::array<u8, 28> result = { 0 };

        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);

        mbedtls_sha256_starts(&ctx, true);
        mbedtls_sha256_update(&ctx, data.data(), data.size());
        mbedtls_sha256_finish(&ctx, result.data());

        mbedtls_sha256_free(&ctx);

        return result;
    }

    std::array<u8, 32> sha256(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u8, 32> result = { 0 };

        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);

        mbedtls_sha256_starts(&ctx, false);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            mbedtls_sha256_update(&ctx, buffer.data(), readSize);
        }

        mbedtls_sha256_finish(&ctx, result.data());

        mbedtls_sha256_free(&ctx);

        return result;
    }

    std::array<u8, 32> sha256(const std::vector<u8> &data) {
        std::array<u8, 32> result = { 0 };

        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);

        mbedtls_sha256_starts(&ctx, false);
        mbedtls_sha256_update(&ctx, data.data(), data.size());
        mbedtls_sha256_finish(&ctx, result.data());

        mbedtls_sha256_free(&ctx);

        return result;
    }

    std::array<u8, 48> sha384(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u8, 48> result = { 0 };

        mbedtls_sha512_context ctx;
        mbedtls_sha512_init(&ctx);

        mbedtls_sha512_starts(&ctx, true);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            mbedtls_sha512_update(&ctx, buffer.data(), readSize);
        }

        mbedtls_sha512_finish(&ctx, result.data());

        mbedtls_sha512_free(&ctx);

        return result;
    }

    std::array<u8, 48> sha384(const std::vector<u8> &data) {
        std::array<u8, 48> result = { 0 };

        mbedtls_sha512_context ctx;
        mbedtls_sha512_init(&ctx);

        mbedtls_sha512_starts(&ctx, true);
        mbedtls_sha512_update(&ctx, data.data(), data.size());
        mbedtls_sha512_finish(&ctx, result.data());

        mbedtls_sha512_free(&ctx);

        return result;
    }

    std::array<u8, 64> sha512(prv::Provider* &data, u64 offset, size_t size) {
        std::array<u8, 64> result = { 0 };

        mbedtls_sha512_context ctx;
        mbedtls_sha512_init(&ctx);

        mbedtls_sha512_starts(&ctx, false);

        std::array<u8, 512> buffer = { 0 };
        for (u64 bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const u64 readSize = std::min(u64(buffer.size()), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            mbedtls_sha512_update(&ctx, buffer.data(), readSize);
        }

        mbedtls_sha512_finish(&ctx, result.data());

        mbedtls_sha512_free(&ctx);

        return result;
    }

    std::array<u8, 64> sha512(const std::vector<u8> &data) {
        std::array<u8, 64> result = { 0 };

        mbedtls_sha512_context ctx;
        mbedtls_sha512_init(&ctx);

        mbedtls_sha512_starts(&ctx, false);
        mbedtls_sha512_update(&ctx, data.data(), data.size());
        mbedtls_sha512_finish(&ctx, result.data());

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

    std::vector<u8> decode16(const std::string &input) {
        std::vector<u8> output(input.length() / 2, 0x00);

        mbedtls_mpi ctx;
        mbedtls_mpi_init(&ctx);

        ON_SCOPE_EXIT { mbedtls_mpi_free(&ctx); };

        if (mbedtls_mpi_read_string(&ctx, 16, input.c_str()))
            return { };

        if (mbedtls_mpi_write_binary(&ctx, output.data(), output.size()))
            return { };

        return output;
    }

    std::string encode16(const std::vector<u8> &input) {
        std::string output(input.size() * 2 + 1, 0x00);

        mbedtls_mpi ctx;
        mbedtls_mpi_init(&ctx);

        ON_SCOPE_EXIT { mbedtls_mpi_free(&ctx); };

        if (mbedtls_mpi_read_binary(&ctx, input.data(), input.size()))
            return { };

        size_t written = 0;
        if (mbedtls_mpi_write_string(&ctx, 16, output.data(), output.size(), &written))
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

        size_t outputSize = input.size() + mbedtls_cipher_get_block_size(&ctx);
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
