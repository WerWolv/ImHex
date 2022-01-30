#include <hex/pattern_language/pattern_language.hpp>

#include <hex/helpers/file.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/logger.hpp>

#include <hex/pattern_language/preprocessor.hpp>
#include <hex/pattern_language/lexer.hpp>
#include <hex/pattern_language/parser.hpp>
#include <hex/pattern_language/validator.hpp>
#include <hex/pattern_language/evaluator.hpp>

#include <unistd.h>

namespace hex::pl {

    class PatternData;

    PatternLanguage::PatternLanguage() {
        this->m_preprocessor = new Preprocessor();
        this->m_lexer = new Lexer();
        this->m_parser = new Parser();
        this->m_validator = new Validator();
        this->m_evaluator = new Evaluator();

        this->m_preprocessor->addDefaultPragmaHandlers();

        this->m_preprocessor->addPragmaHandler("endian", [this](std::string value) {
            if (value == "big") {
                this->m_evaluator->setDefaultEndian(std::endian::big);
                return true;
            } else if (value == "little") {
                this->m_evaluator->setDefaultEndian(std::endian::little);
                return true;
            } else if (value == "native") {
                this->m_evaluator->setDefaultEndian(std::endian::native);
                return true;
            } else
                return false;
        });

        this->m_preprocessor->addPragmaHandler("eval_depth", [this](std::string value) {
            auto limit = strtol(value.c_str(), nullptr, 0);

            if (limit <= 0)
                return false;

            this->m_evaluator->setEvaluationDepth(limit);
            return true;
        });

        this->m_preprocessor->addPragmaHandler("array_limit", [this](const std::string &value) {
            auto limit = strtol(value.c_str(), nullptr, 0);

            if (limit <= 0)
                return false;

            this->m_evaluator->setArrayLimit(limit);
            return true;
        });

        this->m_preprocessor->addPragmaHandler("pattern_limit", [this](const std::string &value) {
            auto limit = strtol(value.c_str(), nullptr, 0);

            if (limit <= 0)
                return false;

            this->m_evaluator->setPatternLimit(limit);
            return true;
        });

        this->m_preprocessor->addPragmaHandler("loop_limit", [this](const std::string &value) {
            auto limit = strtol(value.c_str(), nullptr, 0);

            if (limit <= 0)
                return false;

            this->m_evaluator->setLoopLimit(limit);
            return true;
        });

        this->m_preprocessor->addPragmaHandler("base_address", [](const std::string &value) {
            auto baseAddress = strtoull(value.c_str(), nullptr, 0);

            ImHexApi::Provider::get()->setBaseAddress(baseAddress);
            return true;
        });
    }

    PatternLanguage::~PatternLanguage() {
        delete this->m_preprocessor;
        delete this->m_lexer;
        delete this->m_parser;
        delete this->m_validator;
    }

    std::optional<std::vector<ASTNode *>> PatternLanguage::parseString(const std::string &code) {
        auto preprocessedCode = this->m_preprocessor->preprocess(code);
        if (!preprocessedCode.has_value()) {
            this->m_currError = this->m_preprocessor->getError();
            return std::nullopt;
        }

        auto tokens = this->m_lexer->lex(preprocessedCode.value());
        if (!tokens.has_value()) {
            this->m_currError = this->m_lexer->getError();
            return std::nullopt;
        }

        auto ast = this->m_parser->parse(tokens.value());
        if (!ast.has_value()) {
            this->m_currError = this->m_parser->getError();
            return std::nullopt;
        }

        return ast;
    }

    std::optional<std::vector<PatternData *>> PatternLanguage::executeString(prv::Provider *provider, const std::string &code, const std::map<std::string, Token::Literal> &envVars, const std::map<std::string, Token::Literal> &inVariables) {
        this->m_currError.reset();
        this->m_evaluator->getConsole().clear();
        this->m_evaluator->setProvider(provider);
        this->m_evaluator->setDefaultEndian(std::endian::native);
        this->m_evaluator->setEvaluationDepth(32);
        this->m_evaluator->setArrayLimit(0x1000);
        this->m_evaluator->setPatternLimit(0x2000);
        this->m_evaluator->setLoopLimit(0x1000);
        this->m_evaluator->setInVariables(inVariables);

        for (const auto &[name, value] : envVars)
            this->m_evaluator->setEnvVariable(name, value);

        for (auto &node : this->m_currAST)
            delete node;
        this->m_currAST.clear();

        auto ast = this->parseString(code);
        if (!ast)
            return std::nullopt;

        this->m_currAST = ast.value();

        auto patterns = this->m_evaluator->evaluate(ast.value());
        if (!patterns.has_value()) {
            this->m_currError = this->m_evaluator->getConsole().getLastHardError();
            return std::nullopt;
        }

        return patterns;
    }

    std::optional<std::vector<PatternData *>> PatternLanguage::executeFile(prv::Provider *provider, const fs::path &path, const std::map<std::string, Token::Literal> &envVars, const std::map<std::string, Token::Literal> &inVariables) {
        File file(path, File::Mode::Read);

        return this->executeString(provider, file.readString(), envVars, inVariables);
    }

    void PatternLanguage::abort() {
        this->m_evaluator->abort();
    }

    const std::vector<ASTNode *> &PatternLanguage::getCurrentAST() const {
        return this->m_currAST;
    }

    [[nodiscard]] std::map<std::string, Token::Literal> PatternLanguage::getOutVariables() const {
        return this->m_evaluator->getOutVariables();
    }


    const std::vector<std::pair<LogConsole::Level, std::string>> &PatternLanguage::getConsoleLog() {
        return this->m_evaluator->getConsole().getLog();
    }

    const std::optional<std::pair<u32, std::string>> &PatternLanguage::getError() {
        return this->m_currError;
    }

    u32 PatternLanguage::getCreatedPatternCount() {
        return this->m_evaluator->getPatternCount();
    }

    u32 PatternLanguage::getMaximumPatternCount() {
        return this->m_evaluator->getPatternLimit();
    }


    void PatternLanguage::allowDangerousFunctions(bool allow) {
        this->m_evaluator->allowDangerousFunctions(allow);
    }

    bool PatternLanguage::hasDangerousFunctionBeenCalled() const {
        return this->m_evaluator->hasDangerousFunctionBeenCalled();
    }

}