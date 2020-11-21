#pragma once

#include <hex.hpp>

#include "token.hpp"

#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

namespace hex::lang {

    class Preprocessor {
    public:
        Preprocessor();

        std::pair<Result, std::string> preprocess(const std::string& code, bool initialRun = true);

        void addPragmaHandler(std::string pragmaType, std::function<bool(std::string)> function);
        void addDefaultPragramHandlers();

    private:
        std::unordered_map<std::string, std::function<bool(std::string)>> m_pragmaHandlers;

        std::set<std::pair<std::string, std::string>> m_defines;
        std::set<std::pair<std::string, std::string>> m_pragmas;
    };

}