#include "init/tasks.hpp"

#include <imgui.h>

#include <romfs/romfs.hpp>

#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/api/layout_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/tutorial_manager.hpp>
#include <hex/api/workspace_manager.hpp>

#include <hex/ui/view.hpp>
#include <hex/ui/popup.hpp>
#include <hex/ui/toast.hpp>

#include <nlohmann/json.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/io/file.hpp>

namespace hex::init {

    using namespace std::literals::string_literals;

    bool setupEnvironment() {
        hex::log::debug("Using romfs: '{}'", romfs::name());

        return true;
    }

    bool createDirectories() {
        bool result = true;

        using enum fs::ImHexPath;

        // Try to create all default directories
        for (u32 path = 0; path < u32(fs::ImHexPath::END); path++) {
            for (auto &folder : fs::getDefaultPaths(static_cast<fs::ImHexPath>(path), true)) {
                try {
                    wolv::io::fs::createDirectories(folder);
                } catch (...) {
                    log::error("Failed to create folder {}!", wolv::util::toUTF8String(folder));
                    result = false;
                }
            }
        }

        if (!result)
            ImHexApi::System::impl::addInitArgument("folder-creation-error");

        return result;
    }

    bool deleteSharedData() {
        // This function is called when ImHex is closed. It deletes all shared data that was created by plugins
        // This is a bit of a hack but necessary because when ImHex gets closed, all plugins are unloaded in order for
        // destructors to be called correctly. To prevent crashes when ImHex exits, we need to delete all shared data

        EventImHexClosing::post();
        EventManager::clear();

        // Terminate all asynchronous tasks
        TaskManager::exit();

        while (ImHexApi::Provider::isValid())
            ImHexApi::Provider::remove(ImHexApi::Provider::get());
        ContentRegistry::Provider::impl::getEntries().clear();

        ImHexApi::System::getInitArguments().clear();
        ImHexApi::HexEditor::impl::getBackgroundHighlights().clear();
        ImHexApi::HexEditor::impl::getForegroundHighlights().clear();
        ImHexApi::HexEditor::impl::getBackgroundHighlightingFunctions().clear();
        ImHexApi::HexEditor::impl::getForegroundHighlightingFunctions().clear();
        ImHexApi::HexEditor::impl::getTooltips().clear();
        ImHexApi::HexEditor::impl::getTooltipFunctions().clear();
        ImHexApi::System::getAdditionalFolderPaths().clear();
        ImHexApi::Messaging::impl::getHandlers().clear();
        ImHexApi::Fonts::getCustomFontPath().clear();
        ImHexApi::Fonts::impl::getFonts().clear();

        ContentRegistry::Settings::impl::getSettings().clear();
        ContentRegistry::Settings::impl::getSettingsData().clear();

        ContentRegistry::CommandPaletteCommands::impl::getEntries().clear();
        ContentRegistry::CommandPaletteCommands::impl::getHandlers().clear();

        ContentRegistry::PatternLanguage::impl::getFunctions().clear();
        ContentRegistry::PatternLanguage::impl::getPragmas().clear();
        ContentRegistry::PatternLanguage::impl::getVisualizers().clear();
        ContentRegistry::PatternLanguage::impl::getInlineVisualizers().clear();

        ContentRegistry::Views::impl::getEntries().clear();
        impl::PopupBase::getOpenPopups().clear();
        impl::ToastBase::getQueuedToasts().clear();


        ContentRegistry::Tools::impl::getEntries().clear();
        ContentRegistry::DataInspector::impl::getEntries().clear();

        ContentRegistry::Language::impl::getLanguages().clear();
        ContentRegistry::Language::impl::getLanguageDefinitions().clear();
        LocalizationManager::impl::resetLanguageStrings();

        ContentRegistry::Interface::impl::getWelcomeScreenEntries().clear();
        ContentRegistry::Interface::impl::getFooterItems().clear();
        ContentRegistry::Interface::impl::getToolbarItems().clear();
        ContentRegistry::Interface::impl::getMainMenuItems().clear();
        ContentRegistry::Interface::impl::getMenuItems().clear();
        ContentRegistry::Interface::impl::getSidebarItems().clear();
        ContentRegistry::Interface::impl::getTitleBarButtons().clear();

        ShortcutManager::clearShortcuts();

        ContentRegistry::DataProcessorNode::impl::getEntries().clear();

        ContentRegistry::DataFormatter::impl::getEntries().clear();
        ContentRegistry::FileHandler::impl::getEntries().clear();
        ContentRegistry::Hashes::impl::getHashes().clear();
        ContentRegistry::HexEditor::impl::getVisualizers().clear();

        ContentRegistry::BackgroundServices::impl::stopServices();

        ContentRegistry::CommunicationInterface::impl::getNetworkEndpoints().clear();

        ContentRegistry::Experiments::impl::getExperiments().clear();
        ContentRegistry::Reports::impl::getGenerators().clear();

        WorkspaceManager::reset();
        LayoutManager::reset();

        ThemeManager::reset();

        AchievementManager::getAchievements().clear();
        TutorialManager::reset();

        ProjectFile::getHandlers().clear();
        ProjectFile::getProviderHandlers().clear();
        ProjectFile::setProjectFunctions(nullptr, nullptr);

        fs::setFileBrowserErrorCallback(nullptr);

        // Unlock font atlas so it can be deleted in case of a crash
        if (ImGui::GetCurrentContext() != nullptr)
            ImGui::GetIO().Fonts->Locked = false;

        return true;
    }

    bool loadPlugins() {
        // Load all plugins
        for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Plugins)) {
            PluginManager::load(dir);
        }

        // Get loaded plugins
        auto &plugins = PluginManager::getPlugins();

        // If no plugins were loaded, ImHex wasn't installed properly. This will trigger an error popup later on
        if (plugins.empty()) {
            log::error("No plugins found!");

            ImHexApi::System::impl::addInitArgument("no-plugins");
            return false;
        }

        const auto shouldLoadPlugin = [executablePath = wolv::io::fs::getExecutablePath()](const Plugin &plugin) {
            // In debug builds, ignore all plugins that are not part of the executable directory
            #if !defined(DEBUG)
                return true;
            #endif

            if (!executablePath.has_value())
                return true;

            // Check if the plugin is somewhere in the same directory tree as the executable
            return !std::fs::relative(plugin.getPath(), executablePath->parent_path()).string().starts_with("..");
        };

        // Load library plugins first since plugins might depend on them
        for (const auto &plugin : plugins) {
            if (!plugin.isLibraryPlugin()) continue;

            // Initialize the plugin
            if (!plugin.initializePlugin()) {
                log::error("Failed to initialize library plugin {}", wolv::util::toUTF8String(plugin.getPath().filename()));
            }
        }

        u32 builtinPlugins = 0;
        u32 loadErrors     = 0;

        // Load the builtin plugin first, so it can initialize everything that's necessary for ImHex to work
        for (const auto &plugin : plugins) {
            if (!plugin.isBuiltinPlugin()) continue;
            if (plugin.isLibraryPlugin()) continue;

            if (!shouldLoadPlugin(plugin)) {
                log::debug("Skipping built-in plugin {}", plugin.getPath().string());
                continue;
            }

            // Make sure there's only one built-in plugin
            if (builtinPlugins > 1) continue;

            // Initialize the plugin
            if (!plugin.initializePlugin()) {
                log::error("Failed to initialize plugin {}", wolv::util::toUTF8String(plugin.getPath().filename()));
                loadErrors++;
            } else {
                builtinPlugins++;
            }
        }

        // Load all other plugins
        for (const auto &plugin : plugins) {
            if (plugin.isBuiltinPlugin()) continue;
            if (plugin.isLibraryPlugin()) continue;

            if (!shouldLoadPlugin(plugin)) {
                log::debug("Skipping plugin {}", plugin.getPath().string());
                continue;
            }

            // Initialize the plugin
            if (!plugin.initializePlugin()) {
                log::error("Failed to initialize plugin {}", wolv::util::toUTF8String(plugin.getPath().filename()));
                loadErrors++;
            }
        }

        // If no plugins were loaded successfully, ImHex wasn't installed properly. This will trigger an error popup later on
        if (loadErrors == plugins.size()) {
            log::error("No plugins loaded successfully!");
            ImHexApi::System::impl::addInitArgument("no-plugins");
            return false;
        }

        // ImHex requires exactly one built-in plugin
        // If no built-in plugin or more than one was found, something's wrong and we can't continue
        #if !defined(EMSCRIPTEN)
            if (builtinPlugins == 0) {
                log::error("Built-in plugin not found!");
                ImHexApi::System::impl::addInitArgument("no-builtin-plugin");
                return false;
            } else if (builtinPlugins > 1) {
                log::error("Found more than one built-in plugin!");
                ImHexApi::System::impl::addInitArgument("multiple-builtin-plugins");
                return false;
            }
        #endif

        return true;
    }

    bool deleteOldFiles() {
        bool result = true;

        auto keepNewest = [&](u32 count, fs::ImHexPath pathType) {
            for (const auto &path : fs::getDefaultPaths(pathType)) {
                try {
                    std::vector<std::filesystem::directory_entry> files;

                    for (const auto& file : std::filesystem::directory_iterator(path))
                        files.push_back(file);

                    if (files.size() <= count)
                        return;

                    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
                        return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
                    });

                    for (auto it = files.begin() + count; it != files.end(); it += 1)
                        std::filesystem::remove(it->path());
                } catch (std::filesystem::filesystem_error &e) {
                    log::error("Failed to clear old file! {}", e.what());
                    result = false;
                }
            }
        };

        keepNewest(10, fs::ImHexPath::Logs);
        keepNewest(25, fs::ImHexPath::Backups);

        return result;
    }

    bool unloadPlugins() {
        PluginManager::unload();

        return true;
    }

    bool loadSettings() {
        try {
            // Try to load settings from file
            ContentRegistry::Settings::impl::load();
        } catch (std::exception &e) {
            // If that fails, create a new settings file

            log::error("Failed to load configuration! {}", e.what());

            ContentRegistry::Settings::impl::clear();
            ContentRegistry::Settings::impl::store();

            return false;
        }

        return true;
    }

    bool storeSettings() {
        try {
            ContentRegistry::Settings::impl::store();
            AchievementManager::storeProgress();
        } catch (std::exception &e) {
            log::error("Failed to store configuration! {}", e.what());
            return false;
        }
        return true;
    }

    // Run all exit tasks, and print to console
    void runExitTasks() {
        for (const auto &[name, task, async] : init::getExitTasks()) {
            bool result = task();
            log::info("Exit task '{0}' finished {1}", name, result ? "successfully" : "unsuccessfully");
        }
    }

    std::vector<Task> getInitTasks() {
        return {
            { "Setting up environment",  setupEnvironment,    false },
            { "Creating directories",    createDirectories,   false },
            { "Loading settings",        loadSettings,        false },
            { "Loading plugins",         loadPlugins,         false  },
        };
    }

    std::vector<Task> getExitTasks() {
        return {
            { "Saving settings",         storeSettings,    false },
            { "Cleaning up shared data", deleteSharedData, false },
            { "Unloading plugins",       unloadPlugins,    false },
            { "Deleting old files",      deleteOldFiles,   false },
        };
    }

}
