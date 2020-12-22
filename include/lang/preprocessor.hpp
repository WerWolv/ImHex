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

        std::optional<std::string> preprocess(const std::string& code, bool initialRun = true);

        void addPragmaHandler(std::string pragmaType, std::function<bool(std::string)> function);
        void addDefaultPragmaHandlers();

        const std::pair<u32, std::string>& getError() { return this->m_error; }

    private:
        std::unordered_map<std::string, std::function<bool(std::string)>> m_pragmaHandlers;

        std::set<std::pair<std::string, std::string>> m_defines;
        std::set<std::pair<std::string, std::string>> m_pragmas;

        std::pair<u32, std::string> m_error;
    };

}