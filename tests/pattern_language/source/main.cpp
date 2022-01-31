#include <map>
#include <string>
#include <cstdlib>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/pattern_language/pattern_language.hpp>
#include <hex/pattern_language/evaluator.hpp>
#include <hex/pattern_language/ast_node.hpp>
#include <hex/api/content_registry.hpp>

#include "test_provider.hpp"
#include "test_patterns/test_pattern.hpp"

using namespace hex::test;

void addFunctions() {
    hex::ContentRegistry::PatternLanguage::Namespace nsStd = { "std" };
    hex::ContentRegistry::PatternLanguage::addFunction(nsStd, "assert", 2, [](Evaluator *ctx, auto params) -> Token::Literal {
        auto condition = Token::literalToBoolean(params[0]);
        auto message = Token::literalToString(params[1], false);

        if (!condition)
            LogConsole::abortEvaluation(hex::format("assertion failed \"{0}\"", message));

        return {};
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
    bool failing = currTest->getMode() == Mode::Failing;

    auto provider = new TestProvider();
    ON_SCOPE_EXIT { delete provider; };
    if (provider->getActualSize() == 0) {
        hex::log::fatal("Failed to load Testing Data");
        return EXIT_FAILURE;
    }

    hex::pl::PatternLanguage language;
    addFunctions();

    // Check if compilation succeeded
    auto patterns = language.executeString(provider, testPatterns[testName]->getSourceCode());
    if (!patterns.has_value()) {
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

    ON_SCOPE_EXIT {
        for (auto &pattern : *patterns)
            delete pattern;
    };

    // Check if the right number of patterns have been produced
    if (patterns->size() != currTest->getPatterns().size() && !currTest->getPatterns().empty()) {
        hex::log::fatal("Source didn't produce expected number of patterns");
        return EXIT_FAILURE;
    }

    // Check if the produced patterns are the ones expected
    for (u32 i = 0; i < currTest->getPatterns().size(); i++) {
        auto &evaluatedPattern = *patterns->at(i);
        auto &controlPattern = *currTest->getPatterns().at(i);

        if (evaluatedPattern != controlPattern) {
            hex::log::fatal("Pattern with name {}:{} didn't match template", evaluatedPattern.getTypeName(), evaluatedPattern.getVariableName());
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    int result = EXIT_SUCCESS;

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