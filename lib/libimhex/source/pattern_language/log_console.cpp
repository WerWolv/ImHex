#include <hex/pattern_language/log_console.hpp>

#include <hex/pattern_language/ast/ast_node.hpp>

namespace hex::pl {

    [[noreturn]] void LogConsole::abortEvaluation(const std::string &message, const ASTNode *node) {
        if (node == nullptr)
            throw PatternLanguageError(0, "Evaluator: " + message);
        else
            throw PatternLanguageError(node->getLineNumber(), "Evaluator: " + message);
    }

}