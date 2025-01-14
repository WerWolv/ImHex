#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>
#include <nlohmann/json.hpp>

#include "content/command_line_interface.hpp"

using namespace hex;

namespace hex::plugin::builtin {

    void registerEventHandlers();
    void registerDataVisualizers();
    void registerMiniMapVisualizers();
    void registerDataInspectorEntries();
    void registerToolEntries();
    void registerPatternLanguageFunctions();
    void registerPatternLanguageTypes();
    void registerPatternLanguagePragmas();
    void registerPatternLanguageVisualizers();
    void registerCommandPaletteCommands();
    void registerSettings();
    void loadSettings();
    void registerDataProcessorNodes();
    void registerProviders();
    void registerDataFormatters();
    void registerMainMenuEntries();
    void createWelcomeScreen();
    void registerViews();
    void registerThemeHandlers();
    void registerStyleHandlers();
    void registerThemes();
    void registerBackgroundServices();
    void registerNetworkEndpoints();
    void registerFileHandlers();
    void registerProjectHandlers();
    void registerAchievements();
    void registerReportGenerators();
    void registerTutorials();
    void registerDataInformationSections();
    void loadWorkspaces();

    void addWindowDecoration();
    void addFooterItems();
    void addTitleBarButtons();
    void addToolbarItems();
    void addGlobalUIItems();
    void addInitTasks();

    void handleBorderlessWindowMode();
    void setupOutOfBoxExperience();

    void extractBundledFiles();

}

IMHEX_PLUGIN_SUBCOMMANDS() {
    { "help",           "h", "Print help about this command",                hex::plugin::builtin::handleHelpCommand             },
    { "version",        "",  "Print ImHex version",                          hex::plugin::builtin::handleVersionCommand          },
    { "plugins",        "",  "Lists all plugins that have been installed",   hex::plugin::builtin::handlePluginsCommand          },
    { "language",       "",  "Changes the language ImHex uses",              hex::plugin::builtin::handleLanguageCommand         },
    { "verbose",        "v", "Enables verbose debug logging",                hex::plugin::builtin::handleVerboseCommand          },

    { "open",           "o", "Open files passed as argument. [default]",     hex::plugin::builtin::handleOpenCommand             },
    { "new",            "n", "Create a new empty file",                      hex::plugin::builtin::handleNewCommand              },

    { "calc",           "",  "Evaluate a mathematical expression",           hex::plugin::builtin::handleCalcCommand             },
    { "hash",           "",  "Calculate the hash of a file",                 hex::plugin::builtin::handleHashCommand             },
    { "encode",         "",  "Encode a string",                              hex::plugin::builtin::handleEncodeCommand           },
    { "decode",         "",  "Decode a string",                              hex::plugin::builtin::handleDecodeCommand           },
    { "magic",          "",  "Identify file types",                          hex::plugin::builtin::handleMagicCommand            },
    { "pl",             "",  "Interact with the pattern language",           hex::plugin::builtin::handlePatternLanguageCommand  },
    { "hexdump",        "",  "Generate a hex dump of the provided file",     hex::plugin::builtin::handleHexdumpCommand          },
    { "demangle",       "",  "Demangle a mangled symbol",                    hex::plugin::builtin::handleDemangleCommand         },
    { "reset-settings", "",  "Resets all settings back to default",          hex::plugin::builtin::handleSettingsResetCommand    },
};

IMHEX_PLUGIN_SETUP("Built-in", "WerWolv", "Default ImHex functionality") {
    using namespace hex::plugin::builtin;

    hex::log::debug("Using romfs: '{}'", romfs::name());
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    addInitTasks();
    extractBundledFiles();

    registerMainMenuEntries();

    addFooterItems();
    addTitleBarButtons();
    addToolbarItems();
    addGlobalUIItems();

    registerEventHandlers();
    registerDataVisualizers();
    registerMiniMapVisualizers();
    registerDataInspectorEntries();
    registerToolEntries();
    registerPatternLanguageFunctions();
    registerPatternLanguageTypes();
    registerPatternLanguagePragmas();
    registerPatternLanguageVisualizers();
    registerCommandPaletteCommands();
    registerThemes();
    registerSettings();
    loadSettings();
    registerDataProcessorNodes();
    registerProviders();
    registerDataFormatters();
    registerViews();
    registerThemeHandlers();
    registerStyleHandlers();
    registerBackgroundServices();
    registerNetworkEndpoints();
    registerFileHandlers();
    registerProjectHandlers();
    registerCommandForwarders();
    registerAchievements();
    registerReportGenerators();
    registerTutorials();
    registerDataInformationSections();
    loadWorkspaces();
    addWindowDecoration();
    createWelcomeScreen();

    setupOutOfBoxExperience();
}
