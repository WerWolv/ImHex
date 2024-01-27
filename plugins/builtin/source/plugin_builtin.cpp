#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>
#include <nlohmann/json.hpp>

#include "content/command_line_interface.hpp"

using namespace hex;

namespace hex::plugin::builtin {

    void registerEventHandlers();
    void registerDataVisualizers();
    void registerDataInspectorEntries();
    void registerToolEntries();
    void registerPatternLanguageFunctions();
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
    void loadWorkspaces();

    void addFooterItems();
    void addTitleBarButtons();
    void addToolbarItems();
    void addGlobalUIItems();
    void addInitTasks();
    void loadFonts();

    void handleBorderlessWindowMode();
    void setupOutOfBoxExperience();

    void extractBundledFiles();

}

IMHEX_PLUGIN_SUBCOMMANDS() {
    { "help",       "Print help about this command",                hex::plugin::builtin::handleHelpCommand             },
    { "version",    "Print ImHex version",                          hex::plugin::builtin::handleVersionCommand          },
    { "plugins",    "Lists all plugins that have been installed",   hex::plugin::builtin::handlePluginsCommand          },

    { "open",       "Open files passed as argument. [default]",     hex::plugin::builtin::handleOpenCommand             },

    { "calc",       "Evaluate a mathematical expression",           hex::plugin::builtin::handleCalcCommand             },
    { "hash",       "Calculate the hash of a file",                 hex::plugin::builtin::handleHashCommand             },
    { "encode",     "Encode a string",                              hex::plugin::builtin::handleEncodeCommand           },
    { "decode",     "Decode a string",                              hex::plugin::builtin::handleDecodeCommand           },
    { "magic",      "Identify file types",                          hex::plugin::builtin::handleMagicCommand            },
    { "pl",         "Interact with the pattern language",           hex::plugin::builtin::handlePatternLanguageCommand  },
    { "hexdump",    "Generate a hex dump of the provided file",     hex::plugin::builtin::handleHexdumpCommand          },
    { "hexdump",    "Demangle a mangled symbol",                    hex::plugin::builtin::handleDemangleCommand         },
};

IMHEX_PLUGIN_SETUP("Built-in", "WerWolv", "Default ImHex functionality") {
    using namespace hex::plugin::builtin;

    hex::log::debug("Using romfs: '{}'", romfs::name());
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    addInitTasks();
    loadFonts();
    extractBundledFiles();

    registerMainMenuEntries();

    registerEventHandlers();
    registerDataVisualizers();
    registerDataInspectorEntries();
    registerToolEntries();
    registerPatternLanguageFunctions();
    registerPatternLanguagePragmas();
    registerPatternLanguageVisualizers();
    registerCommandPaletteCommands();
    registerThemes();
    registerSettings();
    loadSettings();
    registerDataProcessorNodes();
    registerProviders();
    registerDataFormatters();
    createWelcomeScreen();
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
    loadWorkspaces();

    addFooterItems();
    addTitleBarButtons();
    addToolbarItems();
    addGlobalUIItems();

    handleBorderlessWindowMode();
    setupOutOfBoxExperience();
}
