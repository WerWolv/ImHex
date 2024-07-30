#include <hex/api/plugin_manager.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/default_paths.hpp>


using namespace hex;
class PluginLoader {
public:
    PluginLoader() {
        for (const auto &dir : paths::Plugins.read()) {
            PluginManager::addLoadPath(dir);
        }

        PluginManager::loadLibraries();
        PluginManager::load();
    }
};
static PluginLoader pluginLoader;