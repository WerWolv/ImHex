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

    ScriptLoaders s_loaders;

    void loadScript(std::vector<const Script*> &scripts, auto &loader) {
        loader.loadAll();

        for (auto &script : loader.getScripts())
            scripts.emplace_back(&script);
    }

    std::vector<const Script*> loadAllScripts() {
        std::vector<const Script*> plugins;

        try {
            std::apply([&plugins](auto&&... args) {
                (loadScript(plugins, args), ...);
            }, s_loaders);
        } catch (const std::exception &e) {
            log::error("Error when loading scripts: {}", e.what());
        }

        return plugins;
    }

    void initializeLoader(u32 &count, auto &loader) {
        try {
            if (loader.initialize())
                count += 1;
        } catch (const std::exception &e) {
            log::error("Error when initializing script loader: {}", e.what());
        }
    }

    bool initializeAllLoaders() {
        u32 count = 0;

        std::apply([&count](auto&&... args) {
            try {
                (initializeLoader(count, args), ...);
            } catch (const std::exception &e) {
                log::error("Error when initializing script loaders: {}", e.what());
            }
        }, s_loaders);

        return count > 0;
    }

    void addScriptsMenu() {
        static TaskHolder runnerTask, updaterTask;
        hex::ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.extras" }, 5000, [] {
            static bool menuJustOpened = true;
            static std::vector<const Script*> scripts;

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

}

IMHEX_PLUGIN_SETUP("Script Loader", "WerWolv", "Script Loader plugin") {
    hex::log::debug("Using romfs: '{}'", romfs::name());
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    TaskManager::doLater([] {
        if (initializeAllLoaders()) {
            addScriptsMenu();
        }
    });

}
