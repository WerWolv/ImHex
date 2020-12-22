#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

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

extern int mainArgc;
extern char **mainArgv;

#ifdef OS_WINDOWS
#define MAGIC_PATH_SEPARATOR	";"
#else
#define MAGIC_PATH_SEPARATOR	":"
#endif

template<>
struct std::is_integral<u128> : public std::true_type { };
template<>
struct std::is_integral<s128> : public std::true_type { };
template<>
struct std::is_signed<s128> : public std::true_type { };

#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION <= 12000
#if __has_include(<concepts>)
// Make sure we break when derived_from is implemented in libc++. Then we can fix a compatibility version above
#include <concepts>
#endif
// libcxx 12 still doesn't have many default concepts implemented, as a result we need to define it ourself using clang built-ins.
// [concept.derived] (patch from https://reviews.llvm.org/D74292)
namespace hex {
template<class _Dp, class _Bp>
concept derived_from =
  __is_base_of(_Bp, _Dp) && __is_convertible_to(const volatile _Dp*, const volatile _Bp*);
}

// [concepts.arithmetic] (patch from https://reviews.llvm.org/D88131)
namespace hex {
template<class _Tp>
concept integral = __is_integral(_Tp);

template<class _Tp>
concept signed_integral = integral<_Tp> && __is_signed(_Tp);

template<class _Tp>
concept unsigned_integral = integral<_Tp> && !signed_integral<_Tp>;

template<class _Tp>
concept floating_point = __is_floating_point(_Tp);
}
#else
// Assume supported
#include <concepts>

namespace hex {
    using std::derived_from;

    using std::integral;
    using std::signed_integral;
    using std::unsigned_integral;
    using std::floating_point;
}
#endif