#pragma once

#include <string_view>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/ostream.h>

#include <fmt/format.h>

#if !defined(LIBWOLV_BUILTIN_UINT128)
    #include <pl/helpers/types.hpp>
#endif