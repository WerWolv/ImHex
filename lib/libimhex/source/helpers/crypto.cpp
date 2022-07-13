#include <hex/helpers/crypto.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/concepts.hpp>

#include <gnutls/crypto.h>

#include <array>
#include <span>
#include <functional>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <bit>

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

        gnutls_hash_hd_t ctx;
        gnutls_hash_init(&ctx, GNUTLS_DIG_MD5);

        processDataByChunks(data, offset, size, std::bind(gnutls_hash, ctx, _1, _2));
        gnutls_hash_output(ctx, result.data());
        
        gnutls_hash_deinit(ctx, nullptr);

        return result;
    }

    std::array<u8, 16> md5(const std::vector<u8> &data) {
        std::array<u8, 16> result = { 0 };

        gnutls_hash_hd_t ctx;
        gnutls_hash_init(&ctx, GNUTLS_DIG_MD5);

        gnutls_hash(ctx, data.data(), data.size());
        gnutls_hash_output(ctx, result.data());
        
        gnutls_hash_deinit(ctx, nullptr);

        return result;
    }

    std::array<u8, 20> sha1(prv::Provider *&data, u64 offset, size_t size) {
        std::array<u8, 20> result = { 0 };

        gnutls_hash_hd_t ctx;
        gnutls_hash_init(&ctx, GNUTLS_DIG_SHA1);
        
        processDataByChunks(data, offset, size, std::bind(gnutls_hash, ctx, _1, _2));
        gnutls_hash_output(ctx, result.data());

        gnutls_hash_deinit(ctx, nullptr);

        return result;
    }

    std::array<u8, 20> sha1(const std::vector<u8> &data) {
        std::array<u8, 20> result = { 0 };

        gnutls_hash_hd_t ctx;
        gnutls_hash_init(&ctx, GNUTLS_DIG_SHA1);

        gnutls_hash(ctx, data.data(), data.size());
        gnutls_hash_output(ctx, result.data());

        gnutls_hash_deinit(ctx, nullptr);

        return result;
    }

    std::array<u8, 28> sha224(prv::Provider *&data, u64 offset, size_t size) {
        std::array<u8, 28> result = { 0 };

        gnutls_hash_hd_t ctx;
        gnutls_hash_init(&ctx, GNUTLS_DIG_SHA224);

        processDataByChunks(data, offset, size, std::bind(gnutls_hash, ctx, _1, _2));
        gnutls_hash_output(ctx, result.data());

        gnutls_hash_deinit(ctx, nullptr);

        return result;
    }

    std::array<u8, 28> sha224(const std::vector<u8> &data) {
        std::array<u8, 28> result = { 0 };

        gnutls_hash_hd_t ctx;
        gnutls_hash_init(&ctx, GNUTLS_DIG_SHA224);

        gnutls_hash(ctx, data.data(), data.size());
        gnutls_hash_output(ctx, result.data());

        gnutls_hash_deinit(ctx, nullptr);

        return result;
    }

    std::array<u8, 32> sha256(prv::Provider *&data, u64 offset, size_t size) {
        std::array<u8, 32> result = { 0 };

        gnutls_hash_hd_t ctx;
        gnutls_hash_init(&ctx, GNUTLS_DIG_SHA256);

        processDataByChunks(data, offset, size, std::bind(gnutls_hash, ctx, _1, _2));
        gnutls_hash_output(ctx, result.data());

        gnutls_hash_deinit(ctx, nullptr);

        return result;
    }

    std::array<u8, 32> sha256(const std::vector<u8> &data) {
        std::array<u8, 32> result = { 0 };

        gnutls_hash_hd_t ctx;
        gnutls_hash_init(&ctx, GNUTLS_DIG_SHA256);

        gnutls_hash(ctx, data.data(), data.size());
        gnutls_hash_output(ctx, result.data());

        gnutls_hash_deinit(ctx, nullptr);

        return result;
    }

    std::array<u8, 48> sha384(prv::Provider *&data, u64 offset, size_t size) {
        std::array<u8, 48> result = { 0 };

        gnutls_hash_hd_t ctx;
        gnutls_hash_init(&ctx, GNUTLS_DIG_SHA384);

        processDataByChunks(data, offset, size, std::bind(gnutls_hash, ctx, _1, _2));
        gnutls_hash_output(ctx, result.data());

        gnutls_hash_deinit(ctx, nullptr);

        return result;
    }

    std::array<u8, 48> sha384(const std::vector<u8> &data) {
        std::array<u8, 48> result = { 0 };

        gnutls_hash_hd_t ctx;
        gnutls_hash_init(&ctx, GNUTLS_DIG_SHA384);

        gnutls_hash(ctx, data.data(), data.size());

        gnutls_hash_output(ctx, result.data());

        gnutls_hash_deinit(ctx, nullptr);

        return result;
    }

    std::array<u8, 64> sha512(prv::Provider *&data, u64 offset, size_t size) {
        std::array<u8, 64> result = { 0 };

        gnutls_hash_hd_t ctx;
        gnutls_hash_init(&ctx, GNUTLS_DIG_SHA512);

        processDataByChunks(data, offset, size, std::bind(gnutls_hash, ctx, _1, _2));
        gnutls_hash_output(ctx, result.data());

        gnutls_hash_deinit(ctx, nullptr);

        return result;
    }

    std::array<u8, 64> sha512(const std::vector<u8> &data) {
        std::array<u8, 64> result = { 0 };

        gnutls_hash_hd_t ctx;
        gnutls_hash_init(&ctx, GNUTLS_DIG_SHA512);

        gnutls_hash(ctx, data.data(), data.size());
        gnutls_hash_output(ctx, result.data());

        gnutls_hash_deinit(ctx, nullptr);

        return result;
    }


    std::vector<u8> decode64(const std::vector<u8> &input) {

        const gnutls_datum_t gnutls_input{
            .data = (unsigned char*) input.data(), // gnutls shouldn't modify the input
            .size = (unsigned int) input.size()
        };
        gnutls_datum_t gnutls_output;
        gnutls_base64_decode2(&gnutls_input, &gnutls_output);

        return {gnutls_output.data, gnutls_output.data+gnutls_output.size };

    }

    std::vector<u8> encode64(const std::vector<u8> &input) {
        const gnutls_datum_t gnutls_input{
            .data = (unsigned char*) input.data(),
            .size = (unsigned int) input.size()
        };
        gnutls_datum_t gnutls_output;
        gnutls_base64_encode2(&gnutls_input, &gnutls_output);

        return {gnutls_output.data, gnutls_output.data+gnutls_output.size };
    }

    std::vector<u8> decode16(const std::string &input) {
        const gnutls_datum_t gnutls_input{
            .data = (unsigned char*) input.c_str(),
            .size = (unsigned int) input.size()
        };

        gnutls_datum_t gnutls_output;
        gnutls_hex_decode2(&gnutls_input, &gnutls_output);

        return { gnutls_output.data, gnutls_output.data+gnutls_output.size };
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

    static std::vector<u8> _aesDecrypt(gnutls_cipher_algorithm_t type, const std::vector<u8> &key, std::array<u8, 8> nonce, std::array<u8, 8> iv, const std::vector<u8> &input) {
        std::vector<u8> output;

        if (input.empty())
            return {};
        if (key.size() > 256)
            return {};


        u8 nonceCounter[16] = {0};
        std::copy(nonce.begin(), nonce.end(), nonceCounter);
        std::copy(iv.begin(), iv.end(), nonceCounter + 8);

        gnutls_datum_t gnutls_key;
        gnutls_key.data = (unsigned char*) key.data();
        gnutls_key.size = key.size();

        gnutls_datum_t gnutls_iv;
        gnutls_iv.data = nonceCounter;
        gnutls_iv.size = 16;

        gnutls_cipher_hd_t ctx;
        gnutls_cipher_init(&ctx, type, &gnutls_key, &gnutls_iv);

        // Why do you add a block size ?
        size_t outputSize = input.size() + gnutls_cipher_get_block_size(type);
        output.resize(outputSize, 0x00);

        gnutls_cipher_decrypt2(ctx, input.data(), input.size(), output.data(), outputSize);

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

        gnutls_cipher_algorithm_t type;
        switch (mode) {
            case AESMode::CBC:
                type = GNUTLS_CIPHER_AES_128_CBC;
                break;
            case AESMode::GCM:
                type = GNUTLS_CIPHER_AES_128_GCM;
                break;
            case AESMode::CCM:
                type = GNUTLS_CIPHER_AES_128_CCM;
                break;
            case AESMode::XTS:
                type = GNUTLS_CIPHER_AES_128_XTS;
                break;
            case AESMode::CFB8:
                type = GNUTLS_CIPHER_AES_128_CFB8;
                break;
            case AESMode::CCM_8:
                type = GNUTLS_CIPHER_AES_128_CCM_8;
                break;
            case AESMode::SIV:
                type = GNUTLS_CIPHER_AES_128_SIV;
                break;
            default:
                return {};
        }

        return _aesDecrypt(type, key, nonce, iv, input);
    }
}