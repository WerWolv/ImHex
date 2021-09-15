#include <hex/pattern_language/evaluator.hpp>
#include <hex/pattern_language/ast_node.hpp>

namespace hex::pl {

    std::optional<std::vector<PatternData*>> Evaluator::evaluate(const std::vector<ASTNode*> &ast) {
        std::vector<PatternData*> patterns;

        try {
            pushScope(patterns);
            for (auto node : ast) {
                // Don't create patterns out of type declarations
                if (dynamic_cast<ASTNodeTypeDecl*>(node))
                    continue;
                else if (dynamic_cast<ASTNodeFunctionCall*>(node)) {
                    delete node->evaluate(this);
                    continue;
                }

                auto newPatterns = node->createPatterns(this);
                patterns.insert(patterns.end(), newPatterns.begin(), newPatterns.end());
            }
            popScope();
        } catch (const LogConsole::EvaluateError &error) {
            this->m_console.log(LogConsole::Level::Error, error.second);
            this->m_console.setHardError(error);

            for (auto &pattern : patterns)
                delete pattern;
            patterns.clear();

            return std::nullopt;
        }

        return patterns;
    }

}