#pragma once

#include <hex.hpp>

#include "token.hpp"

#include <string>
#include <utility>
#include <set>

namespace hex::lang {

    class Preprocessor {
    public:
        Preprocessor();

        std::pair<Result, std::string> preprocess(const std::string& code, bool applyDefines = true);

    private:
        std::set<std::pair<std::string, std::string>> m_defines;
    };

}