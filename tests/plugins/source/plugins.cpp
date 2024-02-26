#include <hex/api/plugin_manager.hpp>

#include <hex/helpers/utils.hpp>

using namespace hex;
class PluginLoader {
public:
    PluginLoader() {
        for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Plugins)) {
            PluginManager::addLoadPath(dir);
        }

        PluginManager::load();
    }
};
static PluginLoader pluginLoader;