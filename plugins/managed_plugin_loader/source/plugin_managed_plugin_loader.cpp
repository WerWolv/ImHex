#include <hex/plugin.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/task.hpp>

#include <loaders/dotnet/dotnet_loader.hpp>

#include <hex/helpers/logger.hpp>
#include <romfs/romfs.hpp>

using namespace hex;
using namespace hex::plugin::loader;

using PluginLoaders = std::tuple<
    DotNetLoader
>;

namespace {

    void loadPlugin(std::vector<const Plugin*> &plugins, auto &loader) {
        loader.loadAll();

        for (auto &plugin : loader.getPlugins())
            plugins.emplace_back(&plugin);
    }

    std::vector<const Plugin*> loadAllPlugins() {
        static PluginLoaders loaders;
        std::vector<const Plugin*> plugins;

        std::apply([&plugins](auto&&... args) {
            try {
                (loadPlugin(plugins, args), ...);
            } catch (const std::exception &exception) {
                log::error("Failed to load plugin: {}", exception.what());
            }
        }, loaders);

        return plugins;
    }

}

IMHEX_PLUGIN_SETUP("Managed Plugin Loader", "WerWolv", "Plugin loader for C# plugins") {
    hex::log::debug("Using romfs: '{}'", romfs::name());

    static auto plugins = loadAllPlugins();

    hex::ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.extras", "Run Script..." }, 5000, [] {
        for (const auto &plugin : plugins) {
            auto &[name, entryPoint] = *plugin;

            if (ImGui::MenuItem(name.c_str())) {
                TaskManager::createTask("Running script...", TaskManager::NoProgress, [entryPoint = std::move(entryPoint)](auto&) {
                    entryPoint();
                });
            }
        }
    });
}
