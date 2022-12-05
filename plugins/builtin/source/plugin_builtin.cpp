#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>
#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    void registerEventHandlers();
    void registerDataVisualizers();
    void registerDataInspectorEntries();
    void registerToolEntries();
    void registerPatternLanguageFunctions();
    void registerPatternLanguagePragmas();
    void registerCommandPaletteCommands();
    void registerSettings();
    void loadSettings();
    void registerDataProcessorNodes();
    void registerHashes();
    void registerProviders();
    void registerDataFormatters();
    void registerLayouts();
    void registerMainMenuEntries();
    void createWelcomeScreen();
    void registerViews();
    void registerShortcuts();

    void addFooterItems();
    void addToolbarItems();
    void addGlobalUIItems();

    void handleBorderlessWindowMode();

}

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
    registerCommandPaletteCommands();
    registerSettings();
    loadSettings();
    registerDataProcessorNodes();
    registerHashes();
    registerProviders();
    registerDataFormatters();
    createWelcomeScreen();
    registerViews();
    registerShortcuts();

    addFooterItems();
    addToolbarItems();
    addGlobalUIItems();

    registerLayouts();
    registerMainMenuEntries();

    handleBorderlessWindowMode();
}

// This is the default plugin
// DO NOT USE THIS IN ANY OTHER PLUGIN
extern "C" bool isBuiltinPlugin() { return true; }
