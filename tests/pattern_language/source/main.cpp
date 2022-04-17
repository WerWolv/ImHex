#include <map>
#include <string>
#include <cstdlib>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/pattern_language/pattern_language.hpp>
#include <hex/pattern_language/evaluator.hpp>
#include <hex/pattern_language/ast/ast_node.hpp>
#include <hex/pattern_language/patterns/pattern.hpp>
#include <hex/api/content_registry.hpp>

#include "test_provider.hpp"
#include "test_patterns/test_pattern.hpp"

#include <fmt/args.h>

using namespace hex::test;

static std::string format(pl::Evaluator *ctx, const auto &params) {
    auto format = pl::Token::literalToString(params[0], true);
    std::string message;

    fmt::dynamic_format_arg_store<fmt::format_context> formatArgs;

    for (u32 i = 1; i < params.size(); i++) {
        auto &param = params[i];

        std::visit(hex::overloaded {
                       [&](pl::Pattern *value) {
                           formatArgs.push_back(value->toString(ctx->getProvider()));
                       },
                       [&](auto &&value) {
                           formatArgs.push_back(value);
                       } },
            param);
    }

    try {
        return fmt::vformat(format, formatArgs);
    } catch (fmt::format_error &error) {
        pl::LogConsole::abortEvaluation(hex::format("format error: {}", error.what()));
    }
}

void addFunctions() {
    using namespace hex::ContentRegistry::PatternLanguage;

    Namespace nsStd = { "std" };
    hex::ContentRegistry::PatternLanguage::addFunction(nsStd, "assert", ParameterCount::exactly(2), [](Evaluator *ctx, auto params) -> Token::Literal {
        auto condition = Token::literalToBoolean(params[0]);
        auto message   = Token::literalToString(params[1], false);

        if (!condition)
            LogConsole::abortEvaluation(hex::format("assertion failed \"{0}\"", message));

        return {};
    });

    hex::ContentRegistry::PatternLanguage::addFunction(nsStd, "print", ParameterCount::atLeast(1), [](Evaluator *ctx, auto params) -> std::optional<Token::Literal> {
        ctx->getConsole().log(LogConsole::Level::Info, format(ctx, params));

        return std::nullopt;
    });
}

int test(int argc, char **argv) {
    auto &testPatterns = TestPattern::getTests();

    // Check if a test to run has been provided
    if (argc != 2) {
        hex::log::fatal("Invalid number of arguments specified! {}", argc);
        return EXIT_FAILURE;
    }

    // Check if that test exists
    std::string testName = argv[1];
    if (!testPatterns.contains(testName)) {
        hex::log::fatal("No test with name {} found!", testName);
        return EXIT_FAILURE;
    }

    const auto &currTest = testPatterns[testName];
    bool failing         = currTest->getMode() == Mode::Failing;

    auto provider = new TestProvider();
    ON_SCOPE_EXIT { delete provider; };
    if (provider->getActualSize() == 0) {
        hex::log::fatal("Failed to load Testing Data");
        return EXIT_FAILURE;
    }

    pl::PatternLanguage language;

    // Check if compilation succeeded
    auto result = language.executeString(provider, testPatterns[testName]->getSourceCode());
    if (!result) {
        hex::log::fatal("Error during compilation!");

        if (auto error = language.getError(); error.has_value())
            hex::log::info("Compile error: {} : {}", error->getLineNumber(), error->what());
        for (auto &[level, message] : language.getConsoleLog())
            hex::log::info("Evaluate error: {}", message);

        return failing ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (failing) {
        hex::log::fatal("Failing test succeeded!");
        return EXIT_FAILURE;
    }

    // Check if the right number of patterns have been produced
    if (language.getPatterns().size() != currTest->getPatterns().size() && !currTest->getPatterns().empty()) {
        hex::log::fatal("Source didn't produce expected number of patterns");
        return EXIT_FAILURE;
    }

    // Check if the produced patterns are the ones expected
    for (u32 i = 0; i < currTest->getPatterns().size(); i++) {
        auto &evaluatedPattern = *language.getPatterns()[i];
        auto &controlPattern   = *currTest->getPatterns()[i];

        if (evaluatedPattern != controlPattern) {
            hex::log::fatal("Pattern with name {}:{} didn't match template", evaluatedPattern.getTypeName(), evaluatedPattern.getVariableName());
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    int result = EXIT_SUCCESS;

    addFunctions();

    for (u32 i = 0; i < 16; i++) {
        result = test(argc, argv);
        if (result != EXIT_SUCCESS)
            break;
    }

    ON_SCOPE_EXIT {
        for (auto &[key, value] : TestPattern::getTests())
            delete value;
    };

    if (result == EXIT_SUCCESS)
        hex::log::info("Success!");
    else
        hex::log::info("Failed!");

    return result;
}