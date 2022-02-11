#pragma once

#include <hex.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <hex/pattern_language/error.hpp>

namespace hex::pl {

    class ASTNode;

    class LogConsole {
    public:
        enum Level
        {
            Debug,
            Info,
            Warning,
            Error
        };

        [[nodiscard]] const auto &getLog() const { return this->m_consoleLog; }

        void log(Level level, const std::string &message);

        [[noreturn]] static void abortEvaluation(const std::string &message, const ASTNode *node = nullptr);

        void clear();

        void setHardError(const PatternLanguageError &error) { this->m_lastHardError = error; }

        [[nodiscard]] const std::optional<PatternLanguageError> &getLastHardError() { return this->m_lastHardError; };

    private:
        std::vector<std::pair<Level, std::string>> m_consoleLog;
        std::optional<PatternLanguageError> m_lastHardError;
    };

}