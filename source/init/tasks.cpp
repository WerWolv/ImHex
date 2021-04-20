#include "init/tasks.hpp"

#include <hex/helpers/net.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/api/content_registry.hpp>

#include "views/view_hexeditor.hpp"
#include "views/view_pattern.hpp"
#include "views/view_pattern_data.hpp"
#include "views/view_hashes.hpp"
#include "views/view_information.hpp"
#include "views/view_help.hpp"
#include "views/view_tools.hpp"
#include "views/view_strings.hpp"
#include "views/view_data_inspector.hpp"
#include "views/view_disassembler.hpp"
#include "views/view_bookmarks.hpp"
#include "views/view_patches.hpp"
#include "views/view_command_palette.hpp"
#include "views/view_settings.hpp"
#include "views/view_data_processor.hpp"
#include "views/view_yara.hpp"

#include "helpers/plugin_manager.hpp"

#include <filesystem>

namespace hex::init {

    static bool checkForUpdates() {
        hex::Net net;

        auto releases = net.getJson("https://api.github.com/repos/WerWolv/ImHex/releases/latest");
        if (releases.code != 200)
        return false;

        if (!releases.response.contains("tag_name") || !releases.response["tag_name"].is_string())
            return false;

        auto currVersion = "v" + std::string(IMHEX_VERSION).substr(0, 5);
        auto latestVersion = releases.response["tag_name"].get<std::string_view>();

        if (latestVersion != currVersion)
            getInitArguments().push_back({ "update-available", latestVersion.data() });

        return true;
    }

    bool createDirectories() {

        std::filesystem::create_directories(hex::getPath(ImHexPath::Patterns)[0]);
        std::filesystem::create_directories(hex::getPath(ImHexPath::PatternsInclude)[0]);
        std::filesystem::create_directories(hex::getPath(ImHexPath::Magic)[0]);
        std::filesystem::create_directories(hex::getPath(ImHexPath::Plugins)[0]);
        std::filesystem::create_directories(hex::getPath(ImHexPath::Resources)[0]);
        std::filesystem::create_directories(hex::getPath(ImHexPath::Config)[0]);

        return true;
    }

    bool loadDefaultViews() {

        ContentRegistry::Views::add<ViewHexEditor>();
        ContentRegistry::Views::add<ViewPattern>();
        ContentRegistry::Views::add<ViewPatternData>();
        ContentRegistry::Views::add<ViewDataInspector>();
        ContentRegistry::Views::add<ViewHashes>();
        ContentRegistry::Views::add<ViewInformation>();
        ContentRegistry::Views::add<ViewStrings>();
        ContentRegistry::Views::add<ViewDisassembler>();
        ContentRegistry::Views::add<ViewBookmarks>();
        ContentRegistry::Views::add<ViewPatches>();
        ContentRegistry::Views::add<ViewTools>();
        ContentRegistry::Views::add<ViewCommandPalette>();
        ContentRegistry::Views::add<ViewHelp>();
        ContentRegistry::Views::add<ViewSettings>();
        ContentRegistry::Views::add<ViewDataProcessor>();
        ContentRegistry::Views::add<ViewYara>();

        return true;
    }

    bool deleteViews() {
        for (auto &view : SharedData::views)
            delete view;
        SharedData::views.clear();

        return true;
    }

    bool loadPlugins() {
        for (const auto &dir : hex::getPath(ImHexPath::Plugins)) {
            PluginManager::load(dir);
        }

        if (PluginManager::getPlugins().empty()) {
            getInitArguments().push_back({ "no-plugins", { } });
            return false;
        }

        for (const auto &plugin : PluginManager::getPlugins()) {
            plugin.initializePlugin();
        }

        return true;
    }

    bool unloadPlugins() {
        PluginManager::unload();

        return true;
    }

    bool loadSettings() {
        ContentRegistry::Settings::load();

        return true;
    }

    bool storeSettings() {
        ContentRegistry::Settings::store();

        return true;
    }

    std::vector<Task> getInitTasks() {
        return {
                { "Checking for updates...",    checkForUpdates     },
                { "Creating directories...",    createDirectories   },
                { "Loading default views...",   loadDefaultViews    },
                { "Loading plugins...",         loadPlugins         },
                { "Loading settings...",        loadSettings        }
        };
    }

    std::vector<Task> getExitTasks() {
        return {
                { "Unloading plugins...",       unloadPlugins       },
                { "Saving settings...",         storeSettings       },
                { "Cleaning up views...",       deleteViews         }
        };
    }

    std::vector<Argument>& getInitArguments() {
        static std::vector<Argument> initArguments;

        return initArguments;
    }

}