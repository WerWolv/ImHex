#pragma once

#include <pl/pattern_language.hpp>
#include <pl/patterns/pattern.hpp>

namespace hex::plugin::visualizers {

    template<typename T>
    std::vector<T> patternToArray(pl::ptrn::Pattern *pattern){
        const auto bytes = pattern->getBytes();

        std::vector<T> result;
        result.resize(bytes.size() / sizeof(T));
        for (size_t i = 0; i < result.size(); i++)
            std::memcpy(&result[i], &bytes[i * sizeof(T)], sizeof(T));

        return result;
    }

}