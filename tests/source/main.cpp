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
    hex::ContentRegistry::PatternLanguageFunctions::Namespace nsStd = { "std" };
    hex::ContentRegistry::PatternLanguageFunctions::add(nsStd, "assert", 2, [](auto &ctx, auto params) {
        auto condition = AS_TYPE(hex::pl::ASTNodeIntegerLiteral, params[0])->getValue();
        auto message = AS_TYPE(hex::pl::ASTNodeStringLiteral, params[1])->getString();

        if (LITERAL_COMPARE(condition, condition == 0))
            ctx.getConsole().abortEvaluation(hex::format("assertion failed \"{0}\"", message.data()));

        return nullptr;
    });
}

int test(int argc, char **argv) {
    auto &testPatterns = TestPattern::getTests();
    ON_SCOPE_EXIT {
        for (auto &[key, value] : TestPattern::getTests())
            delete value;
    };

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
            hex::log::info("Compile error: {}:{}", error->first, error->second);
        else
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
    if (patterns->size() != currTest->getPatterns().size()) {
        hex::log::fatal("Source didn't produce expected number of patterns");
        return EXIT_FAILURE;
    }

    // Check if the produced patterns are the ones expected
    for (u32 i = 0; i < patterns->size(); i++) {
        auto &left = *patterns->at(i);
        auto &right = *currTest->getPatterns().at(i);

        if (left != right) {
            hex::log::fatal("Pattern with name {}:{} didn't match template", patterns->at(i)->getTypeName(), patterns->at(i)->getVariableName());
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    auto result = test(argc, argv);

    if (result == EXIT_SUCCESS)
        hex::log::info("Success!");
    else
        hex::log::info("Failed!");

    return result;
}