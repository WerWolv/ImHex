#include <map>
#include <string>
#include <cstdlib>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/pattern_language/pattern_language.hpp>

#include "test_provider.hpp"
#include "test_patterns/test_pattern_placement.hpp"

using namespace hex::test;

static std::map<std::string, TestPattern*> testPatterns {
        { "Placement", new TestPatternPlacement() }
};

int main(int argc, char **argv) {
    ON_SCOPE_EXIT {
        for (auto &[key, value] : testPatterns)
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

    auto provider = new TestProvider();
    ON_SCOPE_EXIT { delete provider; };
    if (provider->getActualSize() == 0) {
        hex::log::fatal("Failed to load Testing Data");
        return EXIT_FAILURE;
    }

    hex::pl::PatternLanguage language;

    // Check if compilation succeeded
    auto patterns = language.executeString(provider, testPatterns[testName]->getSourceCode());
    if (!patterns.has_value()) {
        hex::log::fatal("Error during compilation!");
        for (auto &[level, line] : language.getConsoleLog())
            hex::log::info("PL: {}", line);

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

    hex::log::info("Success!");

    return EXIT_SUCCESS;
}