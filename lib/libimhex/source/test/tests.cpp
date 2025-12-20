#include <hex/test/tests.hpp>

namespace hex::test {

    static std::map<std::string, Test> s_tests;
    int Tests::addTest(const std::string &name, Function func, bool shouldFail) noexcept {
        s_tests.insert({
            name, { .function=func, .shouldFail=shouldFail }
        });

        return 0;
    }

    std::map<std::string, Test> &Tests::get() noexcept {
        return s_tests;
    }

    bool initPluginImpl(std::string name) {
        if (name != "Built-in") {
            if(!initPluginImpl("Built-in")) return false;
        }

        const auto *plugin = hex::PluginManager::getPlugin(name);
        if (plugin == nullptr) {
            hex::log::fatal("Plugin '{}' was not found !", name);
            return false;
        }else if (!plugin->initializePlugin()) {
            hex::log::fatal("Failed to initialize plugin '{}' !", name);
            return false;
        }

        hex::log::info("Initialized plugin '{}' successfully", name);
        return true;
    }
}