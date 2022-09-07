#include <hex/plugin.hpp>

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

    void registerLanguageEnUS();
    void registerLanguageDeDE();
    void registerLanguageItIT();
    void registerLanguageJaJP();
    void registerLanguageZhCN();
    void registerLanguagePtBR();
    void registerLanguageZhTW();
    void registerLanguageKoKR();
}

IMHEX_PLUGIN_SETUP("Built-in", "WerWolv", "Default ImHex functionality") {

    using namespace hex::plugin::builtin;

    registerLanguageEnUS();
    registerLanguageDeDE();
    registerLanguageItIT();
    registerLanguageJaJP();
    registerLanguageZhCN();
    registerLanguagePtBR();
    registerLanguageZhTW();
    registerLanguageKoKR();

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
