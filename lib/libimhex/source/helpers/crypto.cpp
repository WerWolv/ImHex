#include <hex/helpers/crypto.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/concepts.hpp>

#include <mbedtls/version.h>
#include <mbedtls/base64.h>
#include <mbedtls/bignum.h>
#include <mbedtls/md5.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/aes.h>
#include <mbedtls/cipher.h>

#include <array>
#include <span>
#include <functional>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <bit>

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

    template<typename T>
    T reflect(T in, std::size_t bits) {
        T out {};

        for (std::size_t i = 0; i < bits; i++) {
            out <<= 1;
            if (in & 0b1)
                out |= 1;
            in >>= 1;
        }
        return out;
    }

    template<typename T>
    T reflect(T in) {
        if constexpr (sizeof(T) == 1) {
            T out { in };

            out = ((out & 0xf0u) >> 4) | ((out & 0x0fu) << 4);
            out = ((out & 0xccu) >> 2) | ((out & 0x33u) << 2);
            out = ((out & 0xaau) >> 1) | ((out & 0x55u) << 1);

            return out;
        } else {
            return reflect(in, sizeof(T) * 8);
        }
    }

    template<size_t NumBits> requires (std::has_single_bit(NumBits))
    class Crc {
        // use reflected algorithm, so we reflect only if refin / refout is FALSE
        // mask values, 0b1 << 64 is UB, so use 0b10 << 63

    public:
        constexpr Crc(u64 polynomial, u64 init, u64 xorOut, bool reflectInput, bool reflectOutput)
            : m_value(0x00), m_init(init & ((0b10ull << (NumBits - 1)) - 1)), m_xorOut(xorOut & ((0b10ull << (NumBits - 1)) - 1)),
              m_reflectInput(reflectInput), m_reflectOutput(reflectOutput),
              m_table([polynomial]() {
                auto reflectedPoly = reflect(polynomial & ((0b10ull << (NumBits - 1)) - 1), NumBits);
                std::array<uint64_t, 256> table = { 0 };

                for (uint32_t i = 0; i < 256; i++) {
                    uint64_t c = i;
                    for (std::size_t j = 0; j < 8; j++) {
                        if (c & 0b1)
                            c = reflectedPoly ^ (c >> 1);
                        else
                            c >>= 1;
                    }
                    table[i] = c;
                }

                return table;
         }()) {
            reset();
        };

        constexpr void reset() {
            this->m_value = reflect(m_init, NumBits);
        }

        constexpr void processBytes(const unsigned char *data, std::size_t size) {
            for (std::size_t i = 0; i < size; i++) {
                u8 byte;
                if (this->m_reflectInput)
                    byte = data[i];
                else
                    byte = reflect(data[i]);

                this->m_value = this->m_table[(this->m_value ^ byte) & 0xFFL] ^ (this->m_value >> 8);
            }
        }

        [[nodiscard]]
        constexpr u64 checksum() const {
            if (this->m_reflectOutput)
                return this->m_value ^ m_xorOut;
            else
                return reflect(this->m_value, NumBits) ^ m_xorOut;
        }

    private:
        u64 m_value;

        u64 m_init;
        u64 m_xorOut;
        bool m_reflectInput;
        bool m_reflectOutput;

        std::array<uint64_t, 256> m_table;
    };

    template<size_t NumBits>
    auto calcCrc(prv::Provider *data, u64 offset, std::size_t size, u32 polynomial, u32 init, u32 xorout, bool reflectIn, bool reflectOut) {
        using Crc = Crc<NumBits>;
        Crc crc(polynomial, init, xorout, reflectIn, reflectOut);

        processDataByChunks(data, offset, size, std::bind(&Crc::processBytes, &crc, _1, _2));

        return crc.checksum();
    }

    u16 crc8(prv::Provider *&data, u64 offset, size_t size, u32 polynomial, u32 init, u32 xorOut, bool reflectIn, bool reflectOut) {
        return calcCrc<8>(data, offset, size, polynomial, init, xorOut, reflectIn, reflectOut);
    }

    u16 crc16(prv::Provider *&data, u64 offset, size_t size, u32 polynomial, u32 init, u32 xorOut, bool reflectIn, bool reflectOut) {
        return calcCrc<16>(data, offset, size, polynomial, init, xorOut, reflectIn, reflectOut);
    }

    u32 crc32(prv::Provider *&data, u64 offset, size_t size, u32 polynomial, u32 init, u32 xorOut, bool reflectIn, bool reflectOut) {
        return calcCrc<32>(data, offset, size, polynomial, init, xorOut, reflectIn, reflectOut);
    }


    std::array<u8, 16> md5(prv::Provider *&data, u64 offset, size_t size) {
        std::array<u8, 16> result = { 0 };

        mbedtls_md5_context ctx;
        mbedtls_md5_init(&ctx);

        mbedtls_md5_starts(&ctx);

        processDataByChunks(data, offset, size, std::bind(mbedtls_md5_update, &ctx, _1, _2));

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

        processDataByChunks(data, offset, size, std::bind(mbedtls_sha1_update, &ctx, _1, _2));

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

        processDataByChunks(data, offset, size, std::bind(mbedtls_sha256_update, &ctx, _1, _2));

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

        processDataByChunks(data, offset, size, std::bind(mbedtls_sha256_update, &ctx, _1, _2));

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

        processDataByChunks(data, offset, size, std::bind(mbedtls_sha512_update, &ctx, _1, _2));

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

        processDataByChunks(data, offset, size, std::bind(mbedtls_sha512_update, &ctx, _1, _2));

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
        mbedtls_base64_decode(nullptr, 0, &written, reinterpret_cast<const unsigned char *>(input.data()), input.size());
        std::vector<u8> output(written, 0x00);
        if (mbedtls_base64_decode(output.data(), output.size(), &written, reinterpret_cast<const unsigned char *>(input.data()), input.size()))
            return {};

        output.resize(written);

        return output;
    }

    std::vector<u8> encode64(const std::vector<u8> &input) {

        size_t written = 0;
        mbedtls_base64_encode(nullptr, 0, &written, reinterpret_cast<const unsigned char *>(input.data()), input.size());

        std::vector<u8> output(written, 0x00);
        if (mbedtls_base64_encode(output.data(), output.size(), &written, reinterpret_cast<const unsigned char *>(input.data()), input.size()))
            return {};

        output.resize(written);

        return output;
    }

    std::vector<u8> decode16(const std::string &input) {
        std::vector<u8> output(input.length() / 2, 0x00);

        mbedtls_mpi ctx;
        mbedtls_mpi_init(&ctx);

        ON_SCOPE_EXIT { mbedtls_mpi_free(&ctx); };

        if (mbedtls_mpi_read_string(&ctx, 16, input.c_str()))
            return {};

        if (mbedtls_mpi_write_binary(&ctx, output.data(), output.size()))
            return {};

        return output;
    }

    std::string encode16(const std::vector<u8> &input) {

        if (input.empty())
            return {};

        std::string output(input.size() * 2, '\0');

        for (size_t i = 0; i < input.size(); i++) {
            output[2 * i + 0] = "0123456789ABCDEF"[input[i] / 16];
            output[2 * i + 1] = "0123456789ABCDEF"[input[i] % 16];
        }

        return output;
    }

    template<typename T>
    static T safeLeftShift(T t, u32 shift) {
        if (shift >= sizeof(t) * 8) {
            return 0;
        } else {
            return t << shift;
        }
    }

    template<typename T>
    static T decodeLeb128(const std::vector<u8> &bytes) {
        T value = 0;
        u32 shift = 0;
        u8 b = 0;
        for (u8 byte : bytes) {
            b = byte;
            value |= safeLeftShift(static_cast<T>(byte & 0x7F), shift);
            shift += 7;
            if ((byte & 0x80) == 0) {
                break;
            }
        }
        if constexpr(std::is_signed<T>::value) {
            if ((b & 0x40) != 0) {
                value |= safeLeftShift(~static_cast<T>(0), shift);
            }
        }
        return value;
    }

    u128 decodeUleb128(const std::vector<u8> &bytes) {
        return decodeLeb128<u128>(bytes);
    }

    i128 decodeSleb128(const std::vector<u8> &bytes) {
        return decodeLeb128<i128>(bytes);
    }

    template<typename T>
    static std::vector<u8> encodeLeb128(T value) {
        std::vector<u8> bytes;
        u8 byte;
        while (true) {
            byte = value & 0x7F;
            value >>= 7;
            if constexpr(std::is_signed<T>::value) {
                if (value == 0 && (byte & 0x40) == 0) {
                    break;
                }
                if (value == -1 && (byte & 0x40) != 0) {
                    break;
                }
            } else {
                if (value == 0) {
                    break;
                }
            }
            bytes.push_back(byte | 0x80);
        }
        bytes.push_back(byte);
        return bytes;
    }

    std::vector<u8> encodeUleb128(u128 value) {
        return encodeLeb128<u128>(value);
    }

    std::vector<u8> encodeSleb128(i128 value) {
        return encodeLeb128<i128>(value);
    }

    static std::vector<u8> aes(mbedtls_cipher_type_t type, mbedtls_operation_t operation, const std::vector<u8> &key, std::array<u8, 8> nonce, std::array<u8, 8> iv, const std::vector<u8> &input) {
        std::vector<u8> output;

        if (input.empty())
            return {};
        if (key.size() > 256)
            return {};

        mbedtls_cipher_context_t ctx;
        auto cipherInfo = mbedtls_cipher_info_from_type(type);


        mbedtls_cipher_setup(&ctx, cipherInfo);
        mbedtls_cipher_setkey(&ctx, key.data(), static_cast<int>(key.size() * 8), operation);

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
            case KeyLength::Key128Bits:
                if (key.size() != 128 / 8) return {};
                break;
            case KeyLength::Key192Bits:
                if (key.size() != 192 / 8) return {};
                break;
            case KeyLength::Key256Bits:
                if (key.size() != 256 / 8) return {};
                break;
            default:
                return {};
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
                return {};
        }

        type = mbedtls_cipher_type_t(type + u8(keyLength));

        return aes(type, MBEDTLS_DECRYPT, key, nonce, iv, input);
    }

}
