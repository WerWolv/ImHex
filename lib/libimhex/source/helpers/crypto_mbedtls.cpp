#include <algorithm>
#include <hex/helpers/crypto.hpp>

#include <hex/providers/provider.hpp>

#include <wolv/utils/guards.hpp>
#include <wolv/utils/expected.hpp>

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

namespace hex::crypt {

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
        switch (error) {
            case CRYPTO_ERROR_INVALID_KEY_LENGTH:
                return "Invalid key length";
            case CRYPTO_ERROR_INVALID_MODE:
                return "Invalid mode";
            default: {
                std::array<char, 128> buffer = { 0 };
                mbedtls_strerror(error, buffer.data(), buffer.size());
                return buffer.data();
            }
        }
    }

    static wolv::util::Expected<std::vector<u8>, int> aes(mbedtls_cipher_type_t type, mbedtls_operation_t operation, const std::vector<u8> &key,
                   const std::vector<u8> &nonce, const std::vector<u8> &iv, const std::vector<u8> &input, const std::vector<u8> &tag, const std::vector<u8> &aad) {
        mbedtls_cipher_context_t ctx;
        mbedtls_cipher_init(&ctx);
        ON_SCOPE_EXIT { mbedtls_cipher_free(&ctx); };

        auto cipherInfo = mbedtls_cipher_info_from_type(type);

        if (cipherInfo == nullptr)
            return wolv::util::Unexpected(MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE);

        int setupResult = mbedtls_cipher_setup(&ctx, cipherInfo);
        if (setupResult != 0)
            return wolv::util::Unexpected(setupResult);

        int setKeyResult = mbedtls_cipher_setkey(&ctx, key.data(), key.size() * 8, operation);
        if (setKeyResult != 0)
            return wolv::util::Unexpected(setKeyResult);

        auto mode = mbedtls_cipher_get_cipher_mode(&ctx);

        if (mode == MBEDTLS_MODE_CBC) {
            int paddingResult = mbedtls_cipher_set_padding_mode(&ctx, MBEDTLS_PADDING_NONE);
            if (paddingResult != 0) {
                return wolv::util::Unexpected(paddingResult);
            }
        }

        if (mode == MBEDTLS_MODE_GCM || mode == MBEDTLS_MODE_CCM) {
            const auto &aeadIv = mode == MBEDTLS_MODE_GCM ? iv : nonce;
            std::vector<u8> authenticatedInput;
            authenticatedInput.reserve(input.size() + tag.size());
            authenticatedInput.insert(authenticatedInput.end(), input.begin(), input.end());
            authenticatedInput.insert(authenticatedInput.end(), tag.begin(), tag.end());

            size_t outputSize = input.size();
            std::vector<u8> output(outputSize, 0x00);
            const auto cryptResult = mbedtls_cipher_auth_decrypt_ext(
                &ctx,
                aeadIv.data(), aeadIv.size(),
                aad.data(), aad.size(),
                authenticatedInput.data(), authenticatedInput.size(),
                output.data(), output.size(),
                &outputSize, tag.size());

            if (cryptResult != 0)
                return wolv::util::Unexpected(cryptResult);

            output.resize(outputSize);
            return output;
        }

        if (input.empty())
            return {};

        size_t outputSize = input.size() + mbedtls_cipher_get_block_size(&ctx);
        std::vector<u8> output(outputSize, 0x00);

        int cryptResult = 0;
        if (mode == MBEDTLS_MODE_ECB) {
            const auto blockSize = static_cast<size_t>(mbedtls_cipher_get_block_size(&ctx));
            outputSize = 0;

            for (size_t inputOffset = 0; inputOffset < input.size(); inputOffset += blockSize) {
                const auto inputSize = std::min(blockSize, input.size() - inputOffset);
                size_t blockOutputSize = 0;

                cryptResult = mbedtls_cipher_crypt(&ctx, nullptr, 0, input.data() + inputOffset, inputSize, output.data() + outputSize, &blockOutputSize);
                if (cryptResult != 0)
                    break;

                outputSize += blockOutputSize;
            }
        } else {
            cryptResult = mbedtls_cipher_crypt(&ctx, iv.data(), iv.size(), input.data(), input.size(), output.data(), &outputSize);
        }

        if (cryptResult != 0) {
            return wolv::util::Unexpected(cryptResult);
        }

        output.resize(outputSize);

        return output;
    }

    wolv::util::Expected<std::vector<u8>, int> aesDecrypt(AESMode mode, KeyLength keyLength, const std::vector<u8> &key, const std::vector<u8> &nonce, const std::vector<u8> &iv, const std::vector<u8> &input, const std::vector<u8> &tag, const std::vector<u8> &aad) {
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

        return aes(type, MBEDTLS_DECRYPT, key, nonce, iv, input, tag, aad);
    }

}
