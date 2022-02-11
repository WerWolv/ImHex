#include <hex/pattern_language/log_console.hpp>

#include <hex/pattern_language/ast_node.hpp>

namespace hex::pl {

    void LogConsole::log(Level level, const std::string &message) {
        this->m_consoleLog.emplace_back(level, message);
    }

    [[noreturn]] void LogConsole::abortEvaluation(const std::string &message, const ASTNode *node) {
        if (node == nullptr)
            throw PatternLanguageError(0, "Evaluator: " + message);
        else
            throw PatternLanguageError(node->getLineNumber(), "Evaluator: " + message);
    }

    void LogConsole::clear() {
        this->m_consoleLog.clear();
        this->m_lastHardError = {};
    }

}