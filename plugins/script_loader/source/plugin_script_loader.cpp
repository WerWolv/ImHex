#include <fonts/vscode_icons.hpp>
#include <hex/plugin.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/localization_manager.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

#include <loaders/dotnet/dotnet_loader.hpp>

#include <romfs/romfs.hpp>
#include <nlohmann/json.hpp>

using namespace hex;
using namespace hex::script::loader;

using ScriptLoaders = std::tuple<
    #if defined(IMHEX_DOTNET_SCRIPT_SUPPORT)
        DotNetLoader
    #endif
>;

namespace {

    ScriptLoaders s_loaders;

    void loadScript(std::vector<const Script*> &scripts, auto &loader) {
        loader.loadAll();

        for (auto &script : std::as_const(loader).getScripts())
            scripts.emplace_back(&script);
    }

std::vector<const Script*> loadAllScripts() {
    std::vector<const Script*> scripts;

    try {
        std::apply([&scripts](auto&&... args) {
            (loadScript(scripts, std::forward<decltype(args)>(args)), ...);
        }, s_loaders);
    } catch (const std::exception &e) {
        log::error("Error when loading scripts: {}", e.what());
        return {}; 
    }

    std::vector<hex::Feature> features;
    features.reserve(scripts.size()); 

    for (const auto &script : scripts) {
        if (script->background) {
            features.emplace_back(script->name, true);
        }
    }

    IMHEX_PLUGIN_FEATURES = std::move(features); 

    return scripts;
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
        static std::vector<const Script*> scripts;
        static TaskHolder runnerTask, updaterTask;
        hex::ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.extras" }, 5000, [] {
            static bool menuJustOpened = true;

            if (ImGui::BeginMenuEx("hex.script_loader.menu.run_script"_lang, ICON_VS_LIBRARY)) {
                if (menuJustOpened) {
                    menuJustOpened = false;
                    if (!updaterTask.isRunning()) {
                        updaterTask = TaskManager::createBackgroundTask("hex.script_loader.task.updating"_lang, [] (auto&) {
                            scripts = loadAllScripts();
                        });
                    }
                }

                if (updaterTask.isRunning()) {
                    ImGuiExt::TextSpinner("hex.script_loader.menu.loading"_lang);
                } else if (scripts.empty()) {
                    ImGui::TextUnformatted("hex.script_loader.menu.no_scripts"_lang);
                }

                for (const auto &script : scripts) {
                    const auto &[name, path, background, entryPoint, loader] = *script;
                    if (background)
                        continue;

                    if (ImGui::MenuItem(name.c_str(), loader->getTypeName().c_str())) {
                        runnerTask = TaskManager::createTask("hex.script_loader.task.running"_lang, TaskManager::NoProgress, [entryPoint](auto&) {
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

        updaterTask = TaskManager::createBackgroundTask("hex.script_loader.task.updating"_lang, [] (auto&) {
            scripts = loadAllScripts();
        });
    }

}

IMHEX_PLUGIN_SETUP("Script Loader", "WerWolv", "Script Loader plugin") {
    hex::log::debug("Using romfs: '{}'", romfs::name());
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    if (initializeAllLoaders()) {
        addScriptsMenu();
    }
}