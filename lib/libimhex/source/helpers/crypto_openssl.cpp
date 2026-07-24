#include <algorithm>
#include <hex/helpers/crypto.hpp>

#include <hex/providers/provider.hpp>

#include <openssl/err.h>
#include <openssl/evp.h>

#include <array>
#include <climits>
#include <memory>
#include <span>

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

    static int getLastError() {
        return static_cast<int>(static_cast<unsigned int>(ERR_get_error()));
    }

    template<size_t Size>
    static std::array<u8, Size> hash(prv::Provider *data, u64 offset, size_t size, const EVP_MD *algorithm) {
        std::array<u8, Size> result = { 0 };
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
        if (context == nullptr || EVP_DigestInit_ex(context.get(), algorithm, nullptr) != 1)
            return result;

        bool success = true;
        processDataByChunks(data, offset, size, [&context, &success](auto &&buffer, auto &&bufferSize) {
            if (success)
                success = EVP_DigestUpdate(context.get(), buffer, bufferSize) == 1;
        });

        unsigned int written = 0;
        if (!success || EVP_DigestFinal_ex(context.get(), result.data(), &written) != 1 || written != Size)
            result.fill(0);

        return result;
    }

    template<size_t Size>
    static std::array<u8, Size> hash(const std::vector<u8> &data, const EVP_MD *algorithm) {
        std::array<u8, Size> result = { 0 };
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
        if (context == nullptr || EVP_DigestInit_ex(context.get(), algorithm, nullptr) != 1)
            return result;
        if (EVP_DigestUpdate(context.get(), data.data(), data.size()) != 1)
            return result;

        unsigned int written = 0;
        if (EVP_DigestFinal_ex(context.get(), result.data(), &written) != 1 || written != Size)
            result.fill(0);

        return result;
    }

    std::array<u8, 16> md5(prv::Provider *&data, u64 offset, size_t size) {
        return hash<16>(data, offset, size, EVP_md5());
    }

    std::array<u8, 16> md5(const std::vector<u8> &data) {
        return hash<16>(data, EVP_md5());
    }

    std::array<u8, 20> sha1(prv::Provider *&data, u64 offset, size_t size) {
        return hash<20>(data, offset, size, EVP_sha1());
    }

    std::array<u8, 20> sha1(const std::vector<u8> &data) {
        return hash<20>(data, EVP_sha1());
    }

    std::array<u8, 28> sha224(prv::Provider *&data, u64 offset, size_t size) {
        return hash<28>(data, offset, size, EVP_sha224());
    }

    std::array<u8, 28> sha224(const std::vector<u8> &data) {
        return hash<28>(data, EVP_sha224());
    }

    std::array<u8, 32> sha256(prv::Provider *&data, u64 offset, size_t size) {
        return hash<32>(data, offset, size, EVP_sha256());
    }

    std::array<u8, 32> sha256(const std::vector<u8> &data) {
        return hash<32>(data, EVP_sha256());
    }

    std::array<u8, 48> sha384(prv::Provider *&data, u64 offset, size_t size) {
        return hash<48>(data, offset, size, EVP_sha384());
    }

    std::array<u8, 48> sha384(const std::vector<u8> &data) {
        return hash<48>(data, EVP_sha384());
    }

    std::array<u8, 64> sha512(prv::Provider *&data, u64 offset, size_t size) {
        return hash<64>(data, offset, size, EVP_sha512());
    }

    std::array<u8, 64> sha512(const std::vector<u8> &data) {
        return hash<64>(data, EVP_sha512());
    }

    std::vector<u8> decode64(const std::vector<u8> &input) {
        if (input.empty() || input.size() > INT_MAX)
            return {};

        std::vector<u8> normalizedInput;
        normalizedInput.reserve(input.size());
        for (size_t index = 0; index < input.size(); ++index) {
            const auto value = input[index];
            if (value == '\r' && (index + 1 >= input.size() || input[index + 1] != '\n'))
                return {};

            if (value == '\r' || value == '\n') {
                while (!normalizedInput.empty() && normalizedInput.back() == ' ')
                    normalizedInput.pop_back();
            } else {
                normalizedInput.push_back(value);
            }
        }
        while (!normalizedInput.empty() && normalizedInput.back() == ' ')
            normalizedInput.pop_back();

        if (normalizedInput.empty() || normalizedInput.size() % 4 != 0)
            return {};

        size_t padding = 0;
        if (normalizedInput.back() == '=')
            padding++;
        if (normalizedInput.size() > 1 && normalizedInput[normalizedInput.size() - 2] == '=')
            padding++;

        const auto paddingStart = normalizedInput.end() - padding;
        if (std::find(normalizedInput.begin(), paddingStart, '=') != paddingStart)
            return {};

        const auto outputSize = 3 * (normalizedInput.size() / 4);
        std::vector<u8> output(outputSize, 0x00);
        const auto written = EVP_DecodeBlock(output.data(), normalizedInput.data(), static_cast<int>(normalizedInput.size()));
        if (written < 0)
            return {};

        output.resize(static_cast<size_t>(written) - padding);
        return output;
    }

    std::vector<u8> encode64(const std::vector<u8> &input) {
        if (input.empty())
            return {};
        if (input.size() > INT_MAX)
            return {};

        const auto outputSize = 4 * ((input.size() + 2) / 3);
        std::vector<u8> output(outputSize + 1, 0x00);
        const auto written = EVP_EncodeBlock(output.data(), input.data(), static_cast<int>(input.size()));
        if (written < 0 || static_cast<size_t>(written) != outputSize)
            return {};

        output.resize(outputSize);
        return output;
    }

    std::string getErrorString(int error) {
        if (error == CRYPTO_ERROR_UNSUPPORTED_MODE)
            return "AES mode is unsupported by the current API";

        std::array<char, 256> errorBuffer = { 0 };
        ERR_error_string_n(static_cast<unsigned long>(static_cast<unsigned int>(error)), errorBuffer.data(), errorBuffer.size());
        return errorBuffer.data();
    }

    static const EVP_CIPHER *getCipher(AESMode mode, KeyLength keyLength) {
        switch (mode) {
            case AESMode::ECB:
                switch (keyLength) {
                    case KeyLength::Key128Bits: return EVP_aes_128_ecb();
                    case KeyLength::Key192Bits: return EVP_aes_192_ecb();
                    case KeyLength::Key256Bits: return EVP_aes_256_ecb();
                }
                break;
            case AESMode::CFB128:
                switch (keyLength) {
                    case KeyLength::Key128Bits: return EVP_aes_128_cfb128();
                    case KeyLength::Key192Bits: return EVP_aes_192_cfb128();
                    case KeyLength::Key256Bits: return EVP_aes_256_cfb128();
                }
                break;
            case AESMode::CTR:
                switch (keyLength) {
                    case KeyLength::Key128Bits: return EVP_aes_128_ctr();
                    case KeyLength::Key192Bits: return EVP_aes_192_ctr();
                    case KeyLength::Key256Bits: return EVP_aes_256_ctr();
                }
                break;
            case AESMode::OFB:
                switch (keyLength) {
                    case KeyLength::Key128Bits: return EVP_aes_128_ofb();
                    case KeyLength::Key192Bits: return EVP_aes_192_ofb();
                    case KeyLength::Key256Bits: return EVP_aes_256_ofb();
                }
                break;
            case AESMode::CBC:
            case AESMode::GCM:
            case AESMode::CCM:
            case AESMode::XTS:
                return nullptr;
        }

        return nullptr;
    }

    static wolv::util::Expected<std::vector<u8>, int> aes(const EVP_CIPHER *cipher, const std::vector<u8> &key,
            std::array<u8, 8> nonce, std::array<u8, 8> iv, const std::span<const u8> &input, bool isEcb) {
        if (input.empty())
            return {};

        std::array<u8, 16> nonceCounter = { 0 };
        if (!isEcb) {
            std::ranges::copy(nonce, nonceCounter.begin());
            std::ranges::copy(iv, nonceCounter.begin() + 8);
        }

        ERR_clear_error();
        std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> context(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
        if (context == nullptr)
            return wolv::util::Unexpected(getLastError());
        if (EVP_DecryptInit_ex(context.get(), cipher, nullptr, key.data(), isEcb ? nullptr : nonceCounter.data()) != 1)
            return wolv::util::Unexpected(getLastError());
        if (isEcb && EVP_CIPHER_CTX_set_padding(context.get(), 0) != 1)
            return wolv::util::Unexpected(getLastError());

        std::vector<u8> output(input.size() + EVP_CIPHER_block_size(cipher), 0x00);
        size_t inputOffset = 0;
        size_t outputOffset = 0;
        while (inputOffset < input.size()) {
            const auto chunkSize = std::min(input.size() - inputOffset, static_cast<size_t>(INT_MAX));
            int written = 0;
            if (EVP_DecryptUpdate(context.get(), output.data() + outputOffset, &written,
                    input.data() + inputOffset, static_cast<int>(chunkSize)) != 1)
                return wolv::util::Unexpected(getLastError());

            inputOffset += chunkSize;
            outputOffset += static_cast<size_t>(written);
        }

        int written = 0;
        if (EVP_DecryptFinal_ex(context.get(), output.data() + outputOffset, &written) != 1)
            return wolv::util::Unexpected(getLastError());

        output.resize(input.size());
        return output;
    }

    wolv::util::Expected<std::vector<u8>, int> aesDecrypt(AESMode mode, KeyLength keyLength, const std::vector<u8> &key,
            std::array<u8, 8> nonce, std::array<u8, 8> iv, const std::vector<u8> &input) {
        size_t expectedKeySize = 0;
        switch (keyLength) {
            case KeyLength::Key128Bits: expectedKeySize = 128 / 8; break;
            case KeyLength::Key192Bits: expectedKeySize = 192 / 8; break;
            case KeyLength::Key256Bits: expectedKeySize = 256 / 8; break;
            default: return wolv::util::Unexpected(CRYPTO_ERROR_INVALID_KEY_LENGTH);
        }

        if (key.size() != expectedKeySize)
            return wolv::util::Unexpected(CRYPTO_ERROR_INVALID_KEY_LENGTH);
        if (mode != AESMode::ECB && mode != AESMode::CBC && mode != AESMode::CFB128 && mode != AESMode::CTR &&
                mode != AESMode::GCM && mode != AESMode::CCM && mode != AESMode::OFB && mode != AESMode::XTS)
            return wolv::util::Unexpected(CRYPTO_ERROR_INVALID_MODE);

        const auto cipher = getCipher(mode, keyLength);
        if (cipher == nullptr)
            return wolv::util::Unexpected(CRYPTO_ERROR_UNSUPPORTED_MODE);

        return aes(cipher, key, nonce, iv, input, mode == AESMode::ECB);
    }

}
