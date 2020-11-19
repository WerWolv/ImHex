#pragma once

#include "result.hpp"

namespace hex::lang {

    constexpr Result ResultSuccess(0, 0);

    constexpr Result ResultPreprocessingError(1, 1);
    constexpr Result ResultLexicalError(2, 1);
    constexpr Result ResultParseError(3, 1);
    constexpr Result ResultEvaluatorError(4, 1);

}