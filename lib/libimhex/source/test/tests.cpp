#include <hex/test/tests.hpp>

namespace hex::test {
    std::map<std::string, Test> Tests::s_tests;

    bool initPluginInt(std::string name) {
        if (name != "Built-in") {
            if(!initPluginInt("Built-in")) return false;
        }

        hex::Plugin *plugin = hex::PluginManager::getPlugin(name);
        if (plugin == nullptr) {
            hex::log::fatal("Plugin '{}' was not found !", name);
            return false;
        }else if (!plugin->initializePlugin()) {
            hex::log::fatal("Failed to initialize plugin '{}' !", name);
            return false;
        }

        return true;
    }
}