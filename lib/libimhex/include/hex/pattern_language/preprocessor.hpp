#pragma once

#include <hex.hpp>

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include <hex/helpers/paths.hpp>

#include <hex/pattern_language/error.hpp>

namespace hex::pl {

    class Preprocessor {
    public:
        Preprocessor() = default;

        std::optional<std::string> preprocess(const std::string &code, bool initialRun = true);

        void addPragmaHandler(const std::string &pragmaType, const std::function<bool(const std::string &)> &function);
        void removePragmaHandler(const std::string &pragmaType);
        void addDefaultPragmaHandlers();

        const std::optional<PatternLanguageError> &getError() { return this->m_error; }

        bool shouldOnlyIncludeOnce() {
            return this->m_onlyIncludeOnce;
        }

    private:
        [[noreturn]] static void throwPreprocessorError(const std::string &error, u32 lineNumber) {
            throw PatternLanguageError(lineNumber, "Preprocessor: " + error);
        }

        std::unordered_map<std::string, std::function<bool(std::string)>> m_pragmaHandlers;

        std::set<std::tuple<std::string, std::string, u32>> m_defines;
        std::set<std::tuple<std::string, std::string, u32>> m_pragmas;

        std::set<fs::path> m_onceIncludedFiles;

        std::optional<PatternLanguageError> m_error;

        bool m_onlyIncludeOnce = false;
    };

}