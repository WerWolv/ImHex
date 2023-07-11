#include <hex/plugin.hpp>
#include <hex/subcommands/sub_commands.hpp>

#include <hex/api/event.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>
#include <nlohmann/json.hpp>

using namespace hex;

namespace hex::plugin::builtin {

    void registerEventHandlers();
    void registerDataVisualizers();
    void registerDataInspectorEntries();
    void registerToolEntries();
    void registerPatternLanguageFunctions();
    void registerPatternLanguagePragmas();
    void registerPatternLanguageVisualizers();
    void registerPatternLanguageInlineVisualizers();
    void registerCommandPaletteCommands();
    void registerSettings();
    void loadSettings();
    void registerDataProcessorNodes();
    void registerHashes();
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

    void addFooterItems();
    void addTitleBarButtons();
    void addToolbarItems();
    void addGlobalUIItems();

    void handleBorderlessWindowMode();

    void handleSubCommands();

}

IMHEX_PLUGIN_SUBCOMMANDS() {
    { "version", "Print ImHex version and exit", [](const std::vector<std::string>&) {
        std::string version_str = hex::format("{} (Commit {}@{})", ImHexApi::System::getImHexVersion(), ImHexApi::System::getCommitBranch(), ImHexApi::System::getCommitHash());
        
        hex::print("ImHex version: {}\n", version_str);
        exit(EXIT_SUCCESS);
    }},
    { "help", "Print help about this command and exit", [](const std::vector<std::string>&) {
        hex::print(
            "ImHex - A Hex Editor for Reverse Engineers, Programmers and people who value their retinas when working at 3 AM.\n"
            "\n"
            "usage: imhex [subcommand] [options]\n"
            "registered subcommands:\n"
        );

        for (auto &plugin : PluginManager::getPlugins()) {
            for (auto &subCommand : plugin.getSubCommands()) {
                hex::print("\t--{}\t\t{}\n", subCommand.commandKey.c_str(), subCommand.commandDesc.c_str());
            }
        }
        exit(EXIT_SUCCESS);
    }},
    { "open", "Open files passed as argument. This is the default subcommand is none is entered",
            [](const std::vector<std::string> &args) {

        hex::init::registerSubCommand("open", [](const std::vector<std::string> &args){
            for (auto arg : args) {
                EventManager::post<RequestOpenFile>(arg);
            }
        });
        
        std::vector<std::string> fullPaths;
        bool doubleDashFound = false;
        for (auto &arg : args) {
            
            // Skip the first argument named `--`
            if (arg == "--" && !doubleDashFound) {
                doubleDashFound = true;
            } else {
                auto path = std::filesystem::weakly_canonical(arg);
                fullPaths.push_back(wolv::util::toUTF8String(path));
            }
        }

        hex::init::forwardSubCommand("open", fullPaths);
    }}
};

IMHEX_PLUGIN_SETUP("Built-in", "WerWolv", "Default ImHex functionality") {

    using namespace hex::plugin::builtin;

    hex::log::debug("Using romfs: '{}'", romfs::name());
    for (auto &path : romfs::list("lang"))
        hex::ContentRegistry::Language::addLocalization(nlohmann::json::parse(romfs::get(path).string()));

    registerEventHandlers();
    registerDataVisualizers();
    registerDataInspectorEntries();
    registerToolEntries();
    registerPatternLanguageFunctions();
    registerPatternLanguagePragmas();
    registerPatternLanguageVisualizers();
    registerPatternLanguageInlineVisualizers();
    registerCommandPaletteCommands();
    registerSettings();
    loadSettings();
    registerDataProcessorNodes();
    registerHashes();
    registerProviders();
    registerDataFormatters();
    createWelcomeScreen();
    registerViews();
    registerThemeHandlers();
    registerStyleHandlers();
    registerThemes();
    registerBackgroundServices();
    registerNetworkEndpoints();
    registerFileHandlers();
    registerProjectHandlers();

    addFooterItems();
    addTitleBarButtons();
    addToolbarItems();
    addGlobalUIItems();

    registerMainMenuEntries();

    handleBorderlessWindowMode();
}

// This is the default plugin
// DO NOT USE THIS IN ANY OTHER PLUGIN
extern "C" bool isBuiltinPlugin() { return true; }
