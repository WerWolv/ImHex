#include <algorithm>
#include <hex/helpers/crypto.hpp>

#include <hex/providers/provider.hpp>

#include <mbedtls/version.h>
#include <mbedtls/base64.h>
#include <mbedtls/error.h>

#if MBEDTLS_VERSION_MAJOR >= 4
    // TODO: We'll need to migrate to the new <psa/crypto.h> eventually. For now, just include the old stuff again
    #define MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
    #include <mbedtls/private/md5.h>
    #include <mbedtls/private/sha1.h>
    #include <mbedtls/private/sha256.h>
    #include <mbedtls/private/sha512.h>
    #include <mbedtls/private/cipher.h>
#else
    #include <mbedtls/md5.h>
    #include <mbedtls/sha1.h>
    #include <mbedtls/sha256.h>
    #include <mbedtls/sha512.h>
    #include <mbedtls/cipher.h>
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <bit>
#include <span>

namespace hex::crypt {
    using namespace std::placeholders;

    template<std::invocable<unsigned char *, size_t> Func>
    void processDataByChunks(prv::Provider *data, u64 offset, size_t size, Func func) {
        std::array<u8, 512> buffer = { 0 };
        for (size_t bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const auto readSize = std::min(buffer.size(), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            func(buffer.data(), readSize);
        }
    }

    std::array<u8, 16> md5(prv::Provider *&data, u64 offset, size_t size) {
        std::array<u8, 16> result = { 0 };

        mbedtls_md5_context ctx;
        mbedtls_md5_init(&ctx);

        mbedtls_md5_starts(&ctx);

        processDataByChunks(data, offset, size, [&ctx](auto && data, auto && size) { return mbedtls_md5_update(&ctx, data, size); });

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

    std::array<u8, 20> sha1(prv::Provider *&data, u64 offset, size_t size) {
        std::array<u8, 20> result = { 0 };

        mbedtls_sha1_context ctx;
        mbedtls_sha1_init(&ctx);

        mbedtls_sha1_starts(&ctx);

        processDataByChunks(data, offset, size, [&ctx](auto && data, auto && size) { return mbedtls_sha1_update(&ctx, data, size); });

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

    std::array<u8, 28> sha224(prv::Provider *&data, u64 offset, size_t size) {
        std::array<u8, 28> result = { 0 };

        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);

        mbedtls_sha256_starts(&ctx, true);

        processDataByChunks(data, offset, size, [&ctx](auto && data, auto && size) { return mbedtls_sha256_update(&ctx, data, size); });

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

    std::array<u8, 32> sha256(prv::Provider *&data, u64 offset, size_t size) {
        std::array<u8, 32> result = { 0 };

        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);

        mbedtls_sha256_starts(&ctx, false);

        processDataByChunks(data, offset, size, [&ctx](auto && data, auto && size) { return mbedtls_sha256_update(&ctx, data, size); });

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

    std::array<u8, 48> sha384(prv::Provider *&data, u64 offset, size_t size) {
        std::array<u8, 48> result = { 0 };

        mbedtls_sha512_context ctx;
        mbedtls_sha512_init(&ctx);

        mbedtls_sha512_starts(&ctx, true);

        processDataByChunks(data, offset, size, [&ctx](auto && data, auto && size) { return mbedtls_sha512_update(&ctx, data, size); });

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

    std::array<u8, 64> sha512(prv::Provider *&data, u64 offset, size_t size) {
        std::array<u8, 64> result = { 0 };

        mbedtls_sha512_context ctx;
        mbedtls_sha512_init(&ctx);

        mbedtls_sha512_starts(&ctx, false);

        processDataByChunks(data, offset, size, [&ctx](auto && data, auto && size) { return mbedtls_sha512_update(&ctx, data, size); });

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

        size_t written = 0;
        mbedtls_base64_decode(nullptr, 0, &written, input.data(), input.size());
        std::vector<u8> output(written, 0x00);
        if (mbedtls_base64_decode(output.data(), output.size(), &written, input.data(), input.size()))
            return {};

        output.resize(written);

        return output;
    }

    std::vector<u8> encode64(const std::vector<u8> &input) {

        size_t written = 0;
        mbedtls_base64_encode(nullptr, 0, &written, input.data(), input.size());

        std::vector<u8> output(written, 0x00);
        if (mbedtls_base64_encode(output.data(), output.size(), &written, input.data(), input.size()))
            return {};

        output.resize(written);

        return output;
    }

    std::string getErrorString(int error) {
        std::array<char, 128> errorBuffer = { 0 };
        mbedtls_strerror(error, errorBuffer.data(), errorBuffer.size());
        return errorBuffer.data();
    }

    static wolv::util::Expected<std::vector<u8>, int> aes(mbedtls_cipher_type_t type, mbedtls_operation_t operation, const std::vector<u8> &key,
                   std::array<u8, 8> nonce, std::array<u8, 8> iv, const std::span<const u8> &input) {

        if (input.empty())
            return {};
        if (key.size() > 256)
            return {};

        mbedtls_cipher_context_t ctx;
        auto cipherInfo = mbedtls_cipher_info_from_type(type);

        if (cipherInfo == nullptr)
            return {};

        int setupResult = mbedtls_cipher_setup(&ctx, cipherInfo);
        if (setupResult != 0)
            return wolv::util::Unexpected(setupResult);

        int setKeyResult = mbedtls_cipher_setkey(&ctx, key.data(), key.size() * 8, operation);
        if (setKeyResult != 0)
            return wolv::util::Unexpected(setKeyResult);

        std::array<u8, 16> nonceCounter = { 0 };

        auto mode = mbedtls_cipher_get_cipher_mode(&ctx);

        // if we are in ECB mode, we don't need to set the nonce
        if (mode != MBEDTLS_MODE_ECB) {
            std::ranges::copy(nonce, nonceCounter.begin());
            std::ranges::copy(iv, nonceCounter.begin() + 8);
        }

        size_t outputSize = input.size() + mbedtls_cipher_get_block_size(&ctx);
        std::vector<u8> output(outputSize, 0x00);

        int cryptResult = 0;
        if (mode == MBEDTLS_MODE_ECB) {
            cryptResult = mbedtls_cipher_crypt(&ctx, nullptr, 0, input.data(), input.size(), output.data(), &outputSize);
        } else {
            cryptResult = mbedtls_cipher_crypt(&ctx, nonceCounter.data(), nonceCounter.size(), input.data(), input.size(), output.data(), &outputSize);
        }

        // free regardless of the result
        mbedtls_cipher_free(&ctx);

        if (cryptResult != 0) {
            return wolv::util::Unexpected(cryptResult);
        }

        output.resize(input.size());

        return output;
    }

    wolv::util::Expected<std::vector<u8>, int> aesDecrypt(AESMode mode, KeyLength keyLength, const std::vector<u8> &key, std::array<u8, 8> nonce, std::array<u8, 8> iv, const std::vector<u8> &input) {
        switch (keyLength) {
            case KeyLength::Key128Bits:
                if (key.size() != 128 / 8)
                    return wolv::util::Unexpected(CRYPTO_ERROR_INVALID_KEY_LENGTH);
                break;
            case KeyLength::Key192Bits:
                if (key.size() != 192 / 8)
                    return wolv::util::Unexpected(CRYPTO_ERROR_INVALID_KEY_LENGTH);
                break;
            case KeyLength::Key256Bits:
                if (key.size() != 256 / 8)
                    return wolv::util::Unexpected(CRYPTO_ERROR_INVALID_KEY_LENGTH);
                break;
            default:
                return wolv::util::Unexpected(CRYPTO_ERROR_INVALID_KEY_LENGTH);
        }

        mbedtls_cipher_type_t type;
        switch (mode) {
            case AESMode::ECB:
                type = MBEDTLS_CIPHER_AES_128_ECB;
                break;
            case AESMode::CBC:
                type = MBEDTLS_CIPHER_AES_128_CBC;
                break;
            case AESMode::CFB128:
                type = MBEDTLS_CIPHER_AES_128_CFB128;
                break;
            case AESMode::CTR:
                type = MBEDTLS_CIPHER_AES_128_CTR;
                break;
            case AESMode::GCM:
                type = MBEDTLS_CIPHER_AES_128_GCM;
                break;
            case AESMode::CCM:
                type = MBEDTLS_CIPHER_AES_128_CCM;
                break;
            case AESMode::OFB:
                type = MBEDTLS_CIPHER_AES_128_OFB;
                break;
            case AESMode::XTS:
                type = MBEDTLS_CIPHER_AES_128_XTS;
                break;
            default:
                return wolv::util::Unexpected(CRYPTO_ERROR_INVALID_MODE);
        }

        type = mbedtls_cipher_type_t(type + u8(keyLength));

        return aes(type, MBEDTLS_DECRYPT, key, nonce, iv, input);
    }

}
