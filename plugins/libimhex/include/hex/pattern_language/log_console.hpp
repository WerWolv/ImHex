#pragma once

#include <hex.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <hex/pattern_language/ast_node_base.hpp>

namespace hex::pl {

    class ASTNode;

    class LogConsole {
    public:
        enum Level {
            Debug,
            Info,
            Warning,
            Error
        };

        [[nodiscard]]
        const auto& getLog() const { return this->m_consoleLog; }

        using EvaluateError = std::pair<u32, std::string>;

        void log(Level level, const std::string &message) {
            switch (level) {
                default:
                case Level::Debug:   this->m_consoleLog.emplace_back(level, "[-] " + message); break;
                case Level::Info:    this->m_consoleLog.emplace_back(level, "[i] " + message); break;
                case Level::Warning: this->m_consoleLog.emplace_back(level, "[*] " + message); break;
                case Level::Error:   this->m_consoleLog.emplace_back(level, "[!] " + message); break;
            }
        }

        [[noreturn]]
        static void abortEvaluation(const std::string &message) {
            throw EvaluateError(0, message);
        }

        [[noreturn]]
        static void abortEvaluation(const std::string &message, const auto *node) {
            throw EvaluateError(static_cast<const ASTNode*>(node)->getLineNumber(), message);
        }

        void clear() {
            this->m_consoleLog.clear();
            this->m_lastHardError = { };
        }

        void setHardError(const EvaluateError &error) { this->m_lastHardError = error; }

        [[nodiscard]]
        const std::optional<EvaluateError>& getLastHardError() { return this->m_lastHardError; };

    private:
        std::vector<std::pair<Level, std::string>> m_consoleLog;
        std::optional<EvaluateError> m_lastHardError;
    };

}