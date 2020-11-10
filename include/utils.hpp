#pragma once

#include <hex.hpp>

#include <array>
#include <optional>
#include <string>

namespace hex {

    std::optional<std::string> openFileDialog();

    u16 crc16(u8 *data, size_t size, u16 polynomial, u16 init);
    u32 crc32(u8 *data, size_t size, u32 polynomial, u32 init);
    std::array<u32, 4> md5(u8 *data, size_t size);
}