#include <plugin_builtin.hpp>

#include <hex/plugin.hpp>
#include <hex/api/content_registry/settings.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/debugging.hpp>
#include <hex/helpers/utils.hpp>

#include <romfs/romfs.hpp>
#include <nlohmann/json.hpp>

#include "content/command_line_interface.hpp"
#include <banners/banner_icon.hpp>
#include <fonts/vscode_icons.hpp>

using namespace hex;

IMHEX_PLUGIN_SUBCOMMANDS() {
    { "help",            "h", "Print help about this command",                hex::plugin::builtin::handleHelpCommand             },
    { "version",         "",  "Print ImHex version",                          hex::plugin::builtin::handleVersionCommand          },
    { "version-short",   "",  "Print only the version info in plain text",    hex::plugin::builtin::handleVersionShortCommand     },
    { "plugins",         "",  "Lists all plugins that have been installed",   hex::plugin::builtin::handlePluginsCommand          },
    { "language",        "",  "Changes the language ImHex uses",              hex::plugin::builtin::handleLanguageCommand         },
    { "verbose",         "v", "Enables verbose debug logging",                hex::plugin::builtin::handleVerboseCommand          },

    { "open",            "o", "Open files passed as argument. [default]",     hex::plugin::builtin::handleOpenCommand             },
    { "new",             "n", "Create a new empty file",                      hex::plugin::builtin::handleNewCommand              },

    { "select",          "s",  "Select a range of bytes in the Hex Editor",   hex::plugin::builtin::handleSelectCommand           },
    { "pattern",         "p",  "Sets the loaded pattern",                     hex::plugin::builtin::handlePatternCommand          },
    { "calc",            "",  "Evaluate a mathematical expression",           hex::plugin::builtin::handleCalcCommand             },
    { "hash",            "",  "Calculate the hash of a file",                 hex::plugin::builtin::handleHashCommand             },
    { "encode",          "",  "Encode a string",                              hex::plugin::builtin::handleEncodeCommand           },
    { "decode",          "",  "Decode a string",                              hex::plugin::builtin::handleDecodeCommand           },
    { "magic",           "",  "Identify file types",                          hex::plugin::builtin::handleMagicCommand            },
    { "pl",              "",  "Interact with the pattern language",           hex::plugin::builtin::handlePatternLanguageCommand, SubCommand::Type::SubCommand },
    { "hexdump",         "",  "Generate a hex dump of the provided file",     hex::plugin::builtin::handleHexdumpCommand          },
    { "demangle",        "",  "Demangle a mangled symbol",                    hex::plugin::builtin::handleDemangleCommand         },
    { "reset-settings",  "",  "Resets all settings back to default",          hex::plugin::builtin::handleSettingsResetCommand    },
    { "debug-mode",      "",  "Enables debugging features",                   hex::plugin::builtin::handleDebugModeCommand,       },
    { "validate-plugin", "",  "Validates that a plugin can be loaded",        hex::plugin::builtin::handleValidatePluginCommand   },
    { "save-editor",     "",  "Opens a pattern file for save file editing",   hex::plugin::builtin::handleSaveEditorCommand       },
    { "file-info",       "i", "Displays information about a file",            hex::plugin::builtin::handleFileInfoCommand         },
    { "mcp",             "",  "Starts a MCP Server for AI to interact with",  hex::plugin::builtin::handleMCPCommand              },
};

IMHEX_PLUGIN_SETUP_BUILTIN("Built-in", "WerWolv", "Default ImHex functionality") {
    using namespace hex::plugin::builtin;

    // Show a warning banner on debug builds
    #if defined(DEBUG)
        if (!hex::getEnvironmentVariable("NO_DEBUG_BANNER").has_value()) {
            ui::BannerIcon::open(ICON_VS_ERROR, "You're running a Debug build of ImHex. Performance will be degraded!", ImColor(153, 58, 58));
        }
        dbg::setDebugModeEnabled(true);
    #else
        const auto enabled = ContentRegistry::Settings::read<bool>("hex.builtin.setting.general", "hex.builtin.setting.general.debug_mode_enabled", false);
        dbg::setDebugModeEnabled(enabled);
    #endif

    hex::log::debug("Using romfs: '{}'", romfs::name());
    LocalizationManager::addLanguages(romfs::get("lang/languages.json").string(), [](const std::filesystem::path &path) {
        return romfs::get(path).string();
    });

    addInitTasks();
    extractBundledFiles();

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
    registerMainMenuEntries();
    registerThemeHandlers();
    registerStyleHandlers();
    registerBackgroundServices();
    registerNetworkEndpoints();
    registerMCPTools();
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
