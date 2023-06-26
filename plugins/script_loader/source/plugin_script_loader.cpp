#include <hex/plugin.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/task.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

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

IMHEX_PLUGIN_SETUP("Script Loader", "WerWolv", "Script Loader plugin") {
    hex::log::debug("Using romfs: '{}'", romfs::name());

    static auto plugins = loadAllPlugins();

    static TaskHolder runnerTask, updaterTask;

    static bool menuJustOpened = true;
    hex::ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.extras" }, 5000, [] {
        if (ImGui::BeginMenu("Run Script...")) {
            if (menuJustOpened) {
                menuJustOpened = false;
                if (!updaterTask.isRunning()) {
                    updaterTask = TaskManager::createBackgroundTask("Updating...", [] (auto&) {
                        plugins = loadAllPlugins();
                    });
                }
            }

            if (updaterTask.isRunning()) {
                ImGui::TextSpinner("Updating...");
            }

            for (const auto &plugin : plugins) {
                const auto &[name, entryPoint] = *plugin;

                if (ImGui::MenuItem(name.c_str())) {
                    runnerTask = TaskManager::createTask("Running script...", TaskManager::NoProgress, [entryPoint](auto&) {
                        entryPoint();
                    });
                }
            }
            
            ImGui::EndMenu();
        } else {
            menuJustOpened = true;
        }
    }, [] {
        return !task.isRunning();
    });
}
