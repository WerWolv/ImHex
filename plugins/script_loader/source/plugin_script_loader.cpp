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
using namespace hex::script::loader;

using ScriptLoaders = std::tuple<
    #if defined(DOTNET_PLUGINS)
        DotNetLoader
    #endif
>;

namespace {

    void loadScript(std::vector<const Script*> &scripts, auto &loader) {
        loader.loadAll();

        for (auto &script : loader.getScripts())
            scripts.emplace_back(&script);
    }

    std::vector<const Script*> loadAllScripts() {
        std::vector<const Script*> plugins;

        try {
            static ScriptLoaders loaders;
            std::apply([&plugins](auto&&... args) {
                    (loadScript(plugins, args), ...);
            }, loaders);
        } catch (const std::exception &e) {
            log::error("Error when loading scripts: {}", e.what());
        }

        return plugins;
    }

}

IMHEX_PLUGIN_SETUP("Script Loader", "WerWolv", "Script Loader plugin") {
    hex::log::debug("Using romfs: '{}'", romfs::name());
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    static std::vector<const Script*> scripts;

    static TaskHolder runnerTask, updaterTask;

    static bool menuJustOpened = true;
    hex::ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.extras" }, 5000, [] {
        if (ImGui::BeginMenu("hex.script_loader.menu.run_script"_lang)) {
            if (menuJustOpened) {
                menuJustOpened = false;
                if (!updaterTask.isRunning()) {
                    updaterTask = TaskManager::createBackgroundTask("Updating Scripts...", [] (auto&) {
                        scripts = loadAllScripts();
                    });
                }
            }

            if (updaterTask.isRunning()) {
                ImGui::TextSpinner("hex.script_loader.menu.loading"_lang);
            } else if (scripts.empty()) {
                ImGui::TextUnformatted("hex.script_loader.menu.no_scripts"_lang);
            }

            for (const auto &script : scripts) {
                const auto &[name, entryPoint] = *script;

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
