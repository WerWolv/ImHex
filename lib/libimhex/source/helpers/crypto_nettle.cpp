#include <hex/helpers/crypto.hpp>

#include <hex/providers/provider.hpp>

#include <nettle/aes.h>
#include <nettle/base64.h>
#include <nettle/cbc.h>
#include <nettle/ccm.h>
#include <nettle/cfb.h>
#include <nettle/ctr.h>
#include <nettle/gcm.h>
#include <nettle/md5.h>
#include <nettle/memops.h>
#include <nettle/sha1.h>
#include <nettle/sha2.h>
#include <nettle/version.h>

#include <algorithm>
#include <array>
#include <concepts>

namespace hex::crypt {

    template<std::invocable<unsigned char *, size_t> Func>
    void processHashDataByChunks(prv::Provider *data, u64 offset, size_t size, Func func) {
        std::array<u8, 512> buffer = { 0 };
        for (size_t bufferOffset = 0; bufferOffset < size; bufferOffset += buffer.size()) {
            const auto readSize = std::min(buffer.size(), size - bufferOffset);
            data->read(offset + bufferOffset, buffer.data(), readSize);
            func(buffer.data(), readSize);
        }
    }

    template<size_t ResultSize, typename Context, auto Init, auto Update, auto Digest>
    std::array<u8, ResultSize> hashProvider(prv::Provider *data, u64 offset, size_t size) {
        std::array<u8, ResultSize> result = { 0 };
        Context context;
        Init(&context);
        processHashDataByChunks(data, offset, size, [&](const auto *buffer, auto bufferSize) {
            Update(&context, bufferSize, buffer);
        });
#if NETTLE_VERSION_MAJOR >= 4
        Digest(&context, result.data());
#else
        Digest(&context, result.size(), result.data());
#endif
        return result;
    }

    template<size_t ResultSize, typename Context, auto Init, auto Update, auto Digest>
    std::array<u8, ResultSize> hashVector(const std::vector<u8> &data) {
        std::array<u8, ResultSize> result = { 0 };
        Context context;
        Init(&context);
        if (!data.empty())
            Update(&context, data.size(), data.data());
#if NETTLE_VERSION_MAJOR >= 4
        Digest(&context, result.data());
#else
        Digest(&context, result.size(), result.data());
#endif
        return result;
    }

    std::array<u8, 16> md5(prv::Provider *&data, u64 offset, size_t size) {
        return hashProvider<16, md5_ctx, md5_init, md5_update, md5_digest>(data, offset, size);
    }

    std::array<u8, 16> md5(const std::vector<u8> &data) {
        return hashVector<16, md5_ctx, md5_init, md5_update, md5_digest>(data);
    }

    std::array<u8, 20> sha1(prv::Provider *&data, u64 offset, size_t size) {
        return hashProvider<20, sha1_ctx, sha1_init, sha1_update, sha1_digest>(data, offset, size);
    }

    std::array<u8, 20> sha1(const std::vector<u8> &data) {
        return hashVector<20, sha1_ctx, sha1_init, sha1_update, sha1_digest>(data);
    }

    std::array<u8, 28> sha224(prv::Provider *&data, u64 offset, size_t size) {
        return hashProvider<28, sha224_ctx, sha224_init, sha224_update, sha224_digest>(data, offset, size);
    }

    std::array<u8, 28> sha224(const std::vector<u8> &data) {
        return hashVector<28, sha224_ctx, sha224_init, sha224_update, sha224_digest>(data);
    }

    std::array<u8, 32> sha256(prv::Provider *&data, u64 offset, size_t size) {
        return hashProvider<32, sha256_ctx, sha256_init, sha256_update, sha256_digest>(data, offset, size);
    }

    std::array<u8, 32> sha256(const std::vector<u8> &data) {
        return hashVector<32, sha256_ctx, sha256_init, sha256_update, sha256_digest>(data);
    }

    std::array<u8, 48> sha384(prv::Provider *&data, u64 offset, size_t size) {
        return hashProvider<48, sha384_ctx, sha384_init, sha384_update, sha384_digest>(data, offset, size);
    }

    std::array<u8, 48> sha384(const std::vector<u8> &data) {
        return hashVector<48, sha384_ctx, sha384_init, sha384_update, sha384_digest>(data);
    }

    std::array<u8, 64> sha512(prv::Provider *&data, u64 offset, size_t size) {
        return hashProvider<64, sha512_ctx, sha512_init, sha512_update, sha512_digest>(data, offset, size);
    }

    std::array<u8, 64> sha512(const std::vector<u8> &data) {
        return hashVector<64, sha512_ctx, sha512_init, sha512_update, sha512_digest>(data);
    }

    std::vector<u8> decode64(const std::vector<u8> &input) {
        if (input.empty())
            return {};

        base64_decode_ctx context;
        base64_decode_init(&context);

        std::vector<u8> output(BASE64_DECODE_LENGTH(input.size()));
        size_t outputSize = output.size();
        if (base64_decode_update(&context, &outputSize, output.data(), input.size(), reinterpret_cast<const char *>(input.data())) == 0 ||
            base64_decode_final(&context) == 0)
            return {};

        output.resize(outputSize);
        return output;
    }

    std::vector<u8> encode64(const std::vector<u8> &input) {
        std::vector<u8> output(BASE64_ENCODE_RAW_LENGTH(input.size()));
        if (!input.empty())
            base64_encode_raw(reinterpret_cast<char *>(output.data()), input.size(), input.data());
        return output;
    }

    std::string getErrorString(int error) {
        switch (error) {
            case CRYPTO_ERROR_INVALID_KEY_LENGTH:       return "Invalid key length";
            case CRYPTO_ERROR_INVALID_MODE:             return "Invalid mode";
            case CRYPTO_ERROR_INVALID_INPUT:            return "Invalid input";
            case CRYPTO_ERROR_UNSUPPORTED_MODE:         return "Unsupported mode";
            case CRYPTO_ERROR_AUTHENTICATION_FAILED:    return "Authentication failed";
            default:                                    return "Unknown crypto error";
        }
    }

    static bool isValidCcmMessageLength(size_t nonceSize, size_t inputSize) {
        const auto lengthSize = 15 - nonceSize;
        if (lengthSize >= sizeof(size_t))
            return true;

        return inputSize < (size_t(1) << (lengthSize * 8));
    }

    static int validateAesParameters(AESMode mode, KeyLength keyLength, const std::vector<u8> &key,
                                     const std::vector<u8> &nonce, const std::vector<u8> &iv,
                                     const std::vector<u8> &input, const std::vector<u8> &tag) {
        size_t expectedKeySize = 0;
        switch (keyLength) {
            case KeyLength::Key128Bits: expectedKeySize = 16; break;
            case KeyLength::Key192Bits: expectedKeySize = 24; break;
            case KeyLength::Key256Bits: expectedKeySize = 32; break;
            default: return CRYPTO_ERROR_INVALID_KEY_LENGTH;
        }

        if (key.size() != expectedKeySize)
            return CRYPTO_ERROR_INVALID_KEY_LENGTH;

        switch (mode) {
            case AESMode::ECB:
                if ((input.size() % AES_BLOCK_SIZE) != 0)
                    return CRYPTO_ERROR_INVALID_INPUT;
                break;
            case AESMode::CBC:
                if (iv.size() != AES_BLOCK_SIZE || (input.size() % AES_BLOCK_SIZE) != 0)
                    return CRYPTO_ERROR_INVALID_INPUT;
                break;
            case AESMode::CFB128:
            case AESMode::CTR:
            case AESMode::OFB:
                if (iv.size() != AES_BLOCK_SIZE)
                    return CRYPTO_ERROR_INVALID_INPUT;
                break;
            case AESMode::GCM:
                if (iv.empty() || tag.size() < 4 || tag.size() > GCM_DIGEST_SIZE)
                    return CRYPTO_ERROR_INVALID_INPUT;
                break;
            case AESMode::CCM:
                if (nonce.size() < 7 || nonce.size() > 13 || tag.size() < 4 || tag.size() > CCM_DIGEST_SIZE ||
                    (tag.size() % 2) != 0 || !isValidCcmMessageLength(nonce.size(), input.size()))
                    return CRYPTO_ERROR_INVALID_INPUT;
                break;
            case AESMode::XTS:
                return CRYPTO_ERROR_UNSUPPORTED_MODE;
            default:
                return CRYPTO_ERROR_INVALID_MODE;
        }

        return 0;
    }

    template<typename Context, auto SetKey, auto SetIv, auto Update, auto Decrypt, auto Digest>
    static wolv::util::Expected<std::vector<u8>, int> decryptGcm(const std::vector<u8> &key, const std::vector<u8> &iv,
                                                                 const std::vector<u8> &input, const std::vector<u8> &tag,
                                                                 const std::vector<u8> &aad) {
        Context context;
        SetKey(&context, key.data());
        SetIv(&context, iv.size(), iv.data());
        if (!aad.empty())
            Update(&context, aad.size(), aad.data());

        std::vector<u8> output(input.size());
        if (!input.empty())
            Decrypt(&context, input.size(), output.data(), input.data());

        std::array<u8, GCM_DIGEST_SIZE> actualTag = { 0 };
#if NETTLE_VERSION_MAJOR >= 4
        Digest(&context, actualTag.data());
#else
        Digest(&context, tag.size(), actualTag.data());
#endif
        if (memeql_sec(actualTag.data(), tag.data(), tag.size()) == 0)
            return wolv::util::Unexpected(CRYPTO_ERROR_AUTHENTICATION_FAILED);

        return output;
    }

    template<typename Context, auto SetEncryptKey, auto Encrypt>
    static wolv::util::Expected<std::vector<u8>, int> decryptCcm(const std::vector<u8> &key, const std::vector<u8> &nonce,
                                                                 const std::vector<u8> &input, const std::vector<u8> &tag,
                                                                 const std::vector<u8> &aad) {
        Context context;
        SetEncryptKey(&context, key.data());

        std::vector<u8> authenticatedInput;
        authenticatedInput.reserve(input.size() + tag.size());
        authenticatedInput.insert(authenticatedInput.end(), input.begin(), input.end());
        authenticatedInput.insert(authenticatedInput.end(), tag.begin(), tag.end());

        std::vector<u8> output(input.size());
        const auto result = ccm_decrypt_message(
            &context, reinterpret_cast<nettle_cipher_func *>(Encrypt),
            nonce.size(), nonce.data(),
            aad.size(), aad.data(),
            tag.size(), input.size(), output.data(), authenticatedInput.data());
        if (result == 0)
            return wolv::util::Unexpected(CRYPTO_ERROR_AUTHENTICATION_FAILED);

        return output;
    }

    template<typename Context, auto SetEncryptKey, auto SetDecryptKey, auto Encrypt, auto Decrypt>
    static wolv::util::Expected<std::vector<u8>, int> decryptAes(AESMode mode, const std::vector<u8> &key,
                                                                 const std::vector<u8> &nonce, const std::vector<u8> &iv,
                                                                 const std::vector<u8> &input, const std::vector<u8> &tag,
                                                                 const std::vector<u8> &aad) {
        if (mode == AESMode::GCM) {
            if constexpr (std::same_as<Context, aes128_ctx>)
                return decryptGcm<gcm_aes128_ctx, gcm_aes128_set_key, gcm_aes128_set_iv, gcm_aes128_update, gcm_aes128_decrypt, gcm_aes128_digest>(key, iv, input, tag, aad);
            else if constexpr (std::same_as<Context, aes192_ctx>)
                return decryptGcm<gcm_aes192_ctx, gcm_aes192_set_key, gcm_aes192_set_iv, gcm_aes192_update, gcm_aes192_decrypt, gcm_aes192_digest>(key, iv, input, tag, aad);
            else
                return decryptGcm<gcm_aes256_ctx, gcm_aes256_set_key, gcm_aes256_set_iv, gcm_aes256_update, gcm_aes256_decrypt, gcm_aes256_digest>(key, iv, input, tag, aad);
        }

        if (mode == AESMode::CCM)
            return decryptCcm<Context, SetEncryptKey, Encrypt>(key, nonce, input, tag, aad);

        Context context;
        std::vector<u8> output(input.size());

        if (mode == AESMode::ECB) {
            SetDecryptKey(&context, key.data());
            if (!input.empty())
                Decrypt(&context, input.size(), output.data(), input.data());
            return output;
        }

        SetEncryptKey(&context, key.data());
        auto mutableIv = std::array<u8, AES_BLOCK_SIZE>();
        std::copy(iv.begin(), iv.end(), mutableIv.begin());

        auto *encryptFunction = reinterpret_cast<nettle_cipher_func *>(Encrypt);
        switch (mode) {
            case AESMode::CBC:
                SetDecryptKey(&context, key.data());
                if (!input.empty())
                    cbc_decrypt(&context, reinterpret_cast<nettle_cipher_func *>(Decrypt), AES_BLOCK_SIZE, mutableIv.data(), input.size(), output.data(), input.data());
                break;
            case AESMode::CFB128:
                if (!input.empty())
                    cfb_decrypt(&context, encryptFunction, AES_BLOCK_SIZE, mutableIv.data(), input.size(), output.data(), input.data());
                break;
            case AESMode::CTR:
                if (!input.empty())
                    ctr_crypt(&context, encryptFunction, AES_BLOCK_SIZE, mutableIv.data(), input.size(), output.data(), input.data());
                break;
            case AESMode::OFB: {
                std::array<u8, AES_BLOCK_SIZE> feedback = mutableIv;
                std::array<u8, AES_BLOCK_SIZE> stream = { 0 };
                for (size_t offset = 0; offset < input.size(); offset += AES_BLOCK_SIZE) {
                    Encrypt(&context, AES_BLOCK_SIZE, stream.data(), feedback.data());
                    feedback = stream;
                    const auto blockSize = std::min<size_t>(AES_BLOCK_SIZE, input.size() - offset);
                    for (size_t index = 0; index < blockSize; ++index)
                        output[offset + index] = input[offset + index] ^ stream[index];
                }
                break;
            }
            default:
                return wolv::util::Unexpected(CRYPTO_ERROR_INVALID_MODE);
        }

        return output;
    }

    wolv::util::Expected<std::vector<u8>, int> aesDecrypt(AESMode mode, KeyLength keyLength, const std::vector<u8> &key, const std::vector<u8> &nonce, const std::vector<u8> &iv, const std::vector<u8> &input, const std::vector<u8> &tag, const std::vector<u8> &aad) {
        if (const auto validationResult = validateAesParameters(mode, keyLength, key, nonce, iv, input, tag); validationResult != 0)
            return wolv::util::Unexpected(validationResult);

        switch (keyLength) {
            case KeyLength::Key128Bits:
                return decryptAes<aes128_ctx, aes128_set_encrypt_key, aes128_set_decrypt_key, aes128_encrypt, aes128_decrypt>(mode, key, nonce, iv, input, tag, aad);
            case KeyLength::Key192Bits:
                return decryptAes<aes192_ctx, aes192_set_encrypt_key, aes192_set_decrypt_key, aes192_encrypt, aes192_decrypt>(mode, key, nonce, iv, input, tag, aad);
            case KeyLength::Key256Bits:
                return decryptAes<aes256_ctx, aes256_set_encrypt_key, aes256_set_decrypt_key, aes256_encrypt, aes256_decrypt>(mode, key, nonce, iv, input, tag, aad);
            default:
                return wolv::util::Unexpected(CRYPTO_ERROR_INVALID_KEY_LENGTH);
        }
    }

}
