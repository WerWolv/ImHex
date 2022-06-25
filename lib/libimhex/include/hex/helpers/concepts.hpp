#pragma once

#include <hex.hpp>

#include <type_traits>
#include <memory>

#include <concepts>

namespace hex {

    template<typename T>
    struct always_false : std::false_type { };

    template<typename T, size_t Size>
    concept has_size = sizeof(T) == Size;

    template<typename T>
    class Cloneable {
    public:
        [[nodiscard]] virtual std::unique_ptr<T> clone() const = 0;
    };

}