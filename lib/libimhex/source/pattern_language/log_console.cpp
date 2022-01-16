#include <hex/pattern_language/log_console.hpp>

#include <hex/pattern_language/ast_node.hpp>

namespace hex::pl {

    void LogConsole::log(Level level, const std::string &message) {
        switch (level) {
            default:
            case Level::Debug:   this->m_consoleLog.emplace_back(level, "[-] " + message); break;
            case Level::Info:    this->m_consoleLog.emplace_back(level, "[i] " + message); break;
            case Level::Warning: this->m_consoleLog.emplace_back(level, "[*] " + message); break;
            case Level::Error:   this->m_consoleLog.emplace_back(level, "[!] " + message); break;
        }
    }

    [[noreturn]]
    void LogConsole::abortEvaluation(const std::string &message) {
        throw EvaluateError(0, message);
    }

    [[noreturn]]
    void LogConsole::abortEvaluation(const std::string &message, const ASTNode *node) {
        throw EvaluateError(static_cast<const ASTNode*>(node)->getLineNumber(), message);
    }

    void LogConsole::clear() {
        this->m_consoleLog.clear();
        this->m_lastHardError = { };
    }

}