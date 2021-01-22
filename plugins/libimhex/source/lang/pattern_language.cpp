#include <hex/lang/pattern_language.hpp>

#include <hex/providers/provider.hpp>

#include <hex/lang/preprocessor.hpp>
#include <hex/lang/lexer.hpp>
#include <hex/lang/parser.hpp>
#include <hex/lang/validator.hpp>
#include <hex/lang/evaluator.hpp>
#include <hex/lang/pattern_data.hpp>

#include <unistd.h>

namespace hex::lang {

    PatternLanguage::PatternLanguage(prv::Provider *provider) : m_provider(provider) {
        this->m_preprocessor = new Preprocessor();
        this->m_lexer = new Lexer();
        this->m_parser = new Parser();
        this->m_validator = new Validator();
        this->m_evaluator = new Evaluator(provider);
    }

    PatternLanguage::~PatternLanguage() {
        delete this->m_preprocessor;
        delete this->m_lexer;
        delete this->m_parser;
        delete this->m_validator;
        delete this->m_evaluator;
    }


    std::optional<std::vector<PatternData*>> PatternLanguage::executeString(std::string_view string) {
        this->m_currError.reset();
        this->m_evaluator->getConsole().clear();

        hex::lang::Preprocessor preprocessor;

        std::endian defaultEndian;
        preprocessor.addPragmaHandler("endian", [&defaultEndian](std::string value) {
            if (value == "big") {
                defaultEndian = std::endian::big;
                return true;
            } else if (value == "little") {
                defaultEndian = std::endian::little;
                return true;
            } else if (value == "native") {
                defaultEndian = std::endian::native;
                return true;
            } else
                return false;
        });
        preprocessor.addDefaultPragmaHandlers();

        auto preprocessedCode = preprocessor.preprocess(string.data());
        if (!preprocessedCode.has_value()) {
            this->m_currError = preprocessor.getError();
            return { };
        }

        hex::lang::Lexer lexer;
        auto tokens = lexer.lex(preprocessedCode.value());
        if (!tokens.has_value()) {
            this->m_currError = lexer.getError();
            return { };
        }

        hex::lang::Parser parser;
        auto ast = parser.parse(tokens.value());
        if (!ast.has_value()) {
            this->m_currError = parser.getError();
            return { };
        }

        SCOPE_EXIT( for(auto &node : ast.value()) delete node; );

        hex::lang::Validator validator;
        auto validatorResult = validator.validate(ast.value());
        if (!validatorResult) {
            this->m_currError = validator.getError();
            return { };
        }

        auto provider = SharedData::currentProvider;
        hex::lang::Evaluator evaluator(provider, defaultEndian);

        auto patternData = evaluator.evaluate(ast.value());
        if (!patternData.has_value())
            return { };

        return patternData.value();
    }

    std::optional<std::vector<PatternData*>> PatternLanguage::executeFile(std::string_view path) {
        FILE *file = fopen(path.data(), "r");
        if (file == nullptr)
            return { };

        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        rewind(file);

        std::string code(size + 1, 0x00);
        fread(code.data(), size, 1, file);

        fclose(file);

        return this->executeString(code);
    }


    std::vector<std::pair<LogConsole::Level, std::string>> PatternLanguage::getConsoleLog() {
        return this->m_evaluator->getConsole().getLog();
    }

    std::optional<std::pair<u32, std::string>> PatternLanguage::getError() {
        return this->m_currError;
    }

}