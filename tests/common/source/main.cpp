#include <hex.hpp>

#include <hex/api/event_manager.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/test/tests.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/event_manager.hpp>

#include <cstdlib>

int test(int argc, char **argv) {
    // Check if a test to run has been provided
    if (argc != 2) {
        hex::log::fatal("Invalid number of arguments specified! {}", argc);
        return EXIT_FAILURE;
    }

    // Check if that test exists
    std::string testName = argv[1];
    if (!hex::test::Tests::get().contains(testName)) {
        hex::log::fatal("No test with name {} found!", testName);
        return EXIT_FAILURE;
    }

    auto test = hex::test::Tests::get()[testName];

    auto result = test.function();

    if (test.shouldFail) {
        switch (result) {
            case EXIT_SUCCESS:
                return EXIT_FAILURE;
            case EXIT_FAILURE:
                return EXIT_SUCCESS;
            default:
                return result;
        }
    } else {
        return result;
    }
}

int main(int argc, char **argv) {
    int result = test(argc, argv);

    if (result == EXIT_SUCCESS)
        hex::log::info("Success!");
    else
        hex::log::info("Failed!");

    hex::TaskManager::exit();
    hex::EventImHexClosing::post();
    hex::EventManager::clear();
    hex::PluginManager::unload();

    return result;
}