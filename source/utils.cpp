#include "utils.hpp"

#include <windows.h>
#include <shobjidl.h>

#include <array>
#include <bit>
#include <codecvt>
#include <cstring>
#include <locale>
#include <vector>

namespace hex {

    std::optional<std::string> openFileDialog() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (SUCCEEDED(hr)) {
            IFileOpenDialog *pFileOpen;

            hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

            if (SUCCEEDED(hr)) {
                hr = pFileOpen->Show(nullptr);

                if (SUCCEEDED(hr)) {
                    IShellItem *pItem;
                    hr = pFileOpen->GetResult(&pItem);

                    if (SUCCEEDED(hr)) {
                        PWSTR pszFilePath;
                        hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                        if (SUCCEEDED(hr)) {
                            std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;

                            std::string result = converter.to_bytes(pszFilePath);

                            CoTaskMemFree(pszFilePath);

                            return result;
                        }
                        pItem->Release();
                    }
                }
                pFileOpen->Release();
            }
            CoUninitialize();
        }

        return { };
    }

    u16 crc16(u8 *data, size_t size, u16 polynomial, u16 init) {
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

        for (size_t i = 0; i < size; i++) {
            crc = (crc >> 8) ^ table[(crc ^ u16(*data++)) & 0x00FF];
        }

        return crc;
    }

    u32 crc32(u8 *buffer, size_t size, u32 polynomial, u32 init) {
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
        const uint8_t *u = static_cast<const uint8_t *>(buffer);

        for (size_t i = 0; i < size; ++i)
            c = table[(c ^ u[i]) & 0xFF] ^ (c >> 8);

        return ~c;
    }

    std::array<u32, 4> md5(u8 *data, size_t size) {
        constexpr u32 r[] = {   7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                                4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
                            };

        constexpr u32 k[] = {
                0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
                0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
                0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
                0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
                0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
                0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
                0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
                0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
                0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
            };

        std::array<u32, 4> h = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };

        u32 newSize = ((((size + 8) / 64) + 1) * 64) - 8;

        std::vector<u8> buffer(newSize + 64, 0x00);
        std::memcpy(buffer.data(), data, size);
        buffer[size] = 128;

        u32 numBits = 8 * size;
        std::memcpy(buffer.data() + newSize, &numBits, 4);

        for (s32 offset = 0; offset < newSize; offset += (512/8)) {
            u32 *w = reinterpret_cast<u32*>(buffer.data() + offset);

            u32 a = h[0];
            u32 b = h[1];
            u32 c = h[2];
            u32 d = h[3];

            for (u32 i = 0; i < 64; i++) {
                u32 f, g;

                if (i < 16) {
                    f = (b & c) | ((~b) & d);
                    g = i;
                } else if (i < 32) {
                    f = (d & b) | ((~d) & c);
                    g = (5 * i + 1) & 0x0F;
                } else if (i < 48) {
                    f = b ^ c ^ d;
                    g = (3 * i + 5) & 0x0F;
                } else {
                    f = c ^ (b | (~d));
                    g = (7 * i) & 0x0F;
                }

                u32 temp = d;
                d = c;
                c = b;
                b = b + std::rotl(a + f + k[i] + w[g], r[i]);
                a = temp;
            }

            h[0] += a;
            h[1] += b;
            h[2] += c;
            h[3] += d;
        }

        return h;
    }

}