#include <algorithm>
#include <hex/helpers/crypto.hpp>

#include <hex/providers/provider.hpp>

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
        // Use reflected algorithm, so we reflect only if refin / refout is FALSE
        // mask values, 0b1 << 64 is UB, so use 0b10 << 63

    public:
        constexpr Crc(u64 polynomial, u64 init, u64 xorOut, bool reflectInput, bool reflectOutput)
            :  m_init(init & ((0b10ull << (NumBits - 1)) - 1)), m_xorOut(xorOut & ((0b10ull << (NumBits - 1)) - 1)),
              m_reflectInput(reflectInput), m_reflectOutput(reflectOutput),
              m_table([polynomial] {
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
        }

        constexpr void reset() {
            m_value = reflect(m_init, NumBits);
        }

        constexpr void processBytes(const unsigned char *data, std::size_t size) {
            for (std::size_t i = 0; i < size; i++) {
                u8 byte;
                if (m_reflectInput)
                    byte = data[i];
                else
                    byte = reflect(data[i]);

                m_value = m_table[(m_value ^ byte) & 0xFFL] ^ (m_value >> 8);
            }
        }

        [[nodiscard]]
        constexpr u64 checksum() const {
            if (m_reflectOutput)
                return m_value ^ m_xorOut;
            else
                return reflect(m_value, NumBits) ^ m_xorOut;
        }

    private:
        u64 m_value = 0x00;

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

        processDataByChunks(data, offset, size, [&crc](auto && data, auto && size) { crc.processBytes(data, size); });

        return crc.checksum();
    }

    u8 crc8(prv::Provider *&data, u64 offset, size_t size, u32 polynomial, u32 init, u32 xorOut, bool reflectIn, bool reflectOut) {
        return calcCrc<8>(data, offset, size, polynomial, init, xorOut, reflectIn, reflectOut);
    }

    u16 crc16(prv::Provider *&data, u64 offset, size_t size, u32 polynomial, u32 init, u32 xorOut, bool reflectIn, bool reflectOut) {
        return calcCrc<16>(data, offset, size, polynomial, init, xorOut, reflectIn, reflectOut);
    }

    u32 crc32(prv::Provider *&data, u64 offset, size_t size, u32 polynomial, u32 init, u32 xorOut, bool reflectIn, bool reflectOut) {
        return calcCrc<32>(data, offset, size, polynomial, init, xorOut, reflectIn, reflectOut);
    }


    std::vector<u8> decode16(const std::string &input) {
        if ((input.size() % 2) != 0)
            return {};

        const auto decodeNibble = [](char value) -> int {
            if (value >= '0' && value <= '9')
                return value - '0';
            if (value >= 'a' && value <= 'f')
                return value - 'a' + 10;
            if (value >= 'A' && value <= 'F')
                return value - 'A' + 10;
            return -1;
        };

        std::vector<u8> output(input.size() / 2);
        for (size_t index = 0; index < output.size(); ++index) {
            const auto high = decodeNibble(input[index * 2]);
            const auto low = decodeNibble(input[index * 2 + 1]);
            if (high < 0 || low < 0)
                return {};

            output[index] = static_cast<u8>((high << 4) | low);
        }

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
        if constexpr(std::signed_integral<T>) {
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
            byte = u8(value & 0x7F);
            value >>= 7;
            if constexpr(std::signed_integral<T>) {
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

}
