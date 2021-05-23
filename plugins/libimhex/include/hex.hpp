#pragma once

#include <cstdint>
#include <cstddef>

#include <hex/helpers/lang.hpp>
#include <hex/helpers/logger.hpp>
using namespace hex::lang_literals;

using u8    = std::uint8_t;
using u16   = std::uint16_t;
using u32   = std::uint32_t;
using u64   = std::uint64_t;
using u128  = __uint128_t;

using s8    = std::int8_t;
using s16   = std::int16_t;
using s32   = std::int32_t;
using s64   = std::int64_t;
using s128  = __int128_t;

#ifdef OS_WINDOWS
#define MAGIC_PATH_SEPARATOR	";"
#else
#define MAGIC_PATH_SEPARATOR	":"
#endif