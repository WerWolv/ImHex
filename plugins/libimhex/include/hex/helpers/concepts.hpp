#pragma once

#include <cstddef>
#include <type_traits>

namespace hex {

    template<typename>
    struct is_integral_helper : public std::false_type { };

    template<>
    struct is_integral_helper<u8> : public std::true_type { };

    template<>
    struct is_integral_helper<s8> : public std::true_type { };

    template<>
    struct is_integral_helper<u16> : public std::true_type { };

    template<>
    struct is_integral_helper<s16> : public std::true_type { };

    template<>
    struct is_integral_helper<u32> : public std::true_type { };

    template<>
    struct is_integral_helper<s32> : public std::true_type { };

    template<>
    struct is_integral_helper<u64> : public std::true_type { };

    template<>
    struct is_integral_helper<s64> : public std::true_type { };

    template<>
    struct is_integral_helper<u128> : public std::true_type { };

    template<>
    struct is_integral_helper<s128> : public std::true_type { };

    template<>
    struct is_integral_helper<bool> : public std::true_type { };

    template<>
    struct is_integral_helper<char> : public std::true_type { };

    template<>
    struct is_integral_helper<char8_t> : public std::true_type { };

    template<>
    struct is_integral_helper<char16_t> : public std::true_type { };

    template<>
    struct is_integral_helper<char32_t> : public std::true_type { };

    template<>
    struct is_integral_helper<wchar_t> : public std::true_type { };

    template<typename T>
    struct is_integral : public is_integral_helper<std::remove_cvref_t<T>>::type { };

template<typename>
struct is_signed_helper : public std::false_type { };

template<>
struct is_signed_helper<s8> : public std::true_type { };

template<>
struct is_signed_helper<s16> : public std::true_type { };

template<>
struct is_signed_helper<s32> : public std::true_type { };

template<>
struct is_signed_helper<s64> : public std::true_type { };

template<>
struct is_signed_helper<s128> : public std::true_type { };

template<>
struct is_signed_helper<char> : public std::true_type { };

template<>
struct is_signed_helper<float> : public std::true_type { };

template<>
struct is_signed_helper<double> : public std::true_type { };

template<>
struct is_signed_helper<long double> : public std::true_type { };

template<typename T>
struct is_signed : public is_signed_helper<std::remove_cvref_t<T>>::type { };

template<typename>
struct is_floating_point_helper : public std::false_type { };

template<>
struct is_floating_point_helper<float> : public std::true_type { };

template<>
struct is_floating_point_helper<double> : public std::true_type { };

template<>
struct is_floating_point_helper<long double> : public std::true_type { };

template<typename T>
struct is_floating_point : public is_floating_point_helper<std::remove_cvref_t<T>>::type { };

}

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

#else
// Assume supported
#include <concepts>
namespace hex {
    using std::derived_from;
}
#endif

// [concepts.arithmetic]
namespace hex {
    template<class T>
    concept integral = hex::is_integral<T>::value;

    template<class T>
    concept signed_integral = integral<T> && hex::is_signed<T>::value;

    template<class T>
    concept unsigned_integral = integral<T> && !signed_integral<T>;

    template<class T>
    concept floating_point = std::is_floating_point<T>::value;
}

namespace hex {

    template<typename T>
    struct always_false : std::false_type {};

    template<typename T, size_t Size>
    concept has_size = sizeof(T) == Size;

}

