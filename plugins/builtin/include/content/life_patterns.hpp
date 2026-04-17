#pragma once
// life_patterns.hpp

#include <cstddef>

namespace hex::plugin::builtin::life {
    size_t numPatterns();
    const char* getPattern(int idx);
}
