#pragma once

#include "result.hpp"

namespace hex::lang {

    constexpr Result ResultSuccess(0, 0);

    constexpr Result ResultLexicalError(1, 1);
    constexpr Result ResultParseError(2, 1);

}