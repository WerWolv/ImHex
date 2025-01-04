#include "init/tasks.hpp"

#include <imgui.h>

#include <romfs/romfs.hpp>

#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/default_paths.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/api/achievement_manager.hpp>

#include <hex/ui/view.hpp>
#include <hex/ui/popup.hpp>

#include <nlohmann/json.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/io/file.hpp>

namespace hex::init {

    using namespace std::literals::string_literals;

    bool setupEnvironment() {
        hex::log::debug("Using romfs: '{}'", romfs::name());

        return true;
    }

    static bool isSubPathWritable(std::fs::path path) {
        for (u32 i = 0; i < 128; i++) {
            if (hex::fs::isPathWritable(path))
                return true;

            auto parentPath = path.parent_path();
            if (parentPath == path)
                break;

            path = std::move(parentPath);
        }

        return false;
    }

    bool createDirectories() {
        bool result = true;

        // Try to create all default directories
        for (auto path : paths::All) {
            for (auto &folder : path->all()) {
                try {
                    if (isSubPathWritable(folder.parent_path()))
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

    bool prepareExit() {
        // Terminate all asynchronous tasks
        TaskManager::exit();

        // Unlock font atlas, so it can be deleted in case of a crash
        if (ImGui::GetCurrentContext() != nullptr) {
            if (ImGui::GetIO().Fonts != nullptr) {
                ImGui::GetIO().Fonts->Locked = false;
                ImGui::GetIO().Fonts = nullptr;
            }
        }

        // Print a nice message if a crash happened while cleaning up resources
        // To the person fixing this:
        //     ALWAYS wrap static heap allocated objects inside libimhex such as std::vector, std::string, std::function, etc. in a AutoReset<T>
        //     e.g `AutoReset<std::vector<MyStruct>> m_structs;`
        //
        //     The reason this is necessary because each plugin / dynamic library gets its own instance of `std::allocator`
        //     which will try to free the allocated memory when the object is destroyed. However since the storage is static, this
        //     will happen only when libimhex is unloaded after main() returns. At this point all plugins have been unloaded already so
        //     the std::allocator will try to free memory in a heap that does not exist anymore which will cause a crash.
        //     By wrapping the object in a AutoReset<T>, the `EventImHexClosing` event will automatically handle clearing the object
        //     while the heap is still valid.
        //     The heap stays valid right up to the point where `PluginManager::unload()` is called.
        EventAbnormalTermination::subscribe([](int) {
            log::fatal("A crash happened while cleaning up resources during exit!");
            log::fatal("This is most certainly because WerWolv again forgot to mark a heap allocated object as 'AutoReset'.");
            log::fatal("Please report this issue on the ImHex GitHub page!");
            log::fatal("To the person fixing this, read the comment above this message for more information.");
        });

        ImHexApi::System::impl::cleanup();

        EventImHexClosing::post();
        EventManager::clear();

        return true;
    }

    bool loadPlugins() {
        // Load all plugins
        #if !defined(IMHEX_STATIC_LINK_PLUGINS)
            for (const auto &dir : paths::Plugins.read()) {
                PluginManager::addLoadPath(dir);
            }

            PluginManager::loadLibraries();
            PluginManager::load();
        #endif

        // Get loaded plugins
        const auto &plugins = PluginManager::getPlugins();

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

            if (!PluginManager::getPluginLoadPaths().empty())
                return true;

            // Check if the plugin is somewhere in the same directory tree as the executable
            return !std::fs::relative(plugin.getPath(), executablePath->parent_path()).string().starts_with("..");
        };

        u32 loadErrors = 0;
        std::set<std::string> pluginNames;

        // Load library plugins first since plugins might depend on them
        for (const auto &plugin : plugins) {
            if (!plugin.isLibraryPlugin()) continue;

            if (!shouldLoadPlugin(plugin)) {
                log::debug("Skipping library plugin {}", plugin.getPath().string());
                continue;
            }

            // Initialize the library
            if (!plugin.initializePlugin()) {
                log::error("Failed to initialize library plugin {}", wolv::util::toUTF8String(plugin.getPath().filename()));
                loadErrors++;
            }
            pluginNames.insert(plugin.getPluginName());
        }

        // Load all plugins
        for (const auto &plugin : plugins) {
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
            pluginNames.insert(plugin.getPluginName());
        }

        // If no plugins were loaded successfully, ImHex wasn't installed properly. This will trigger an error popup later on
        if (loadErrors == plugins.size()) {
            log::error("No plugins loaded successfully!");
            ImHexApi::System::impl::addInitArgument("no-plugins");
            return false;
        }
        if (pluginNames.size() != plugins.size()) {
            log::error("Duplicate plugins detected!");
            ImHexApi::System::impl::addInitArgument("duplicate-plugins");
            return false;
        }

        return true;
    }

    bool deleteOldFiles() {
        bool result = true;

        auto keepNewest = [&](u32 count, const paths::impl::DefaultPath &pathType) {
            for (const auto &path : pathType.write()) {
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

        keepNewest(10, paths::Logs);
        keepNewest(25, paths::Backups);

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
            log::error("Failed to load configuration! {}", e.what());

            return false;
        }

        return true;
    }

    // Run all exit tasks, and print to console
    void runExitTasks() {
        for (const auto &[name, task, async, running] : init::getExitTasks()) {
            const bool result = task();
            log::info("Exit task '{0}' finished {1}", name, result ? "successfully" : "unsuccessfully");
        }
    }

    std::vector<Task> getInitTasks() {
        return {
            { "Setting up environment",  setupEnvironment,    false, false },
            { "Creating directories",    createDirectories,   false, false },
            { "Loading settings",        loadSettings,        false, false },
            { "Loading plugins",         loadPlugins,         false, false },
        };
    }

    std::vector<Task> getExitTasks() {
        return {
            { "Prepare exit",            prepareExit,      false, false },
            { "Unloading plugins",       unloadPlugins,    false, false },
            { "Deleting old files",      deleteOldFiles,   false, false },
        };
    }

}
