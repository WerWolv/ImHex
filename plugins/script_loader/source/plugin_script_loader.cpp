#include <hex/plugin.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/task.hpp>
#include <hex/api/localization.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

#include <loaders/dotnet/dotnet_loader.hpp>

#include <romfs/romfs.hpp>
#include <nlohmann/json.hpp>

using namespace hex;
using namespace hex::plugin::loader;

using PluginLoaders = std::tuple<
    #if defined(DOTNET_PLUGINS)
        DotNetLoader
    #endif
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
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    static auto plugins = loadAllPlugins();

    static TaskHolder runnerTask, updaterTask;

    static bool menuJustOpened = true;
    hex::ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.extras" }, 5000, [] {
        if (ImGui::BeginMenu("hex.script_loader.menu.run_script"_lang)) {
            if (menuJustOpened) {
                menuJustOpened = false;
                if (!updaterTask.isRunning()) {
                    updaterTask = TaskManager::createBackgroundTask("Updating Scripts...", [] (auto&) {
                        plugins = loadAllPlugins();
                    });
                }
            }

            if (updaterTask.isRunning()) {
                ImGui::TextSpinner("hex.script_loader.menu.loading"_lang);
            } else if (plugins.empty()) {
                ImGui::TextUnformatted("hex.script_loader.menu.no_scripts"_lang);
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
        return !runnerTask.isRunning();
    });
}
