#include <hex/plugin.hpp>

namespace hex::plugin::builtin {

    void registerViews();
    void registerDataInspectorEntries();
    void registerToolEntries();
    void registerPatternLanguageFunctions();
    void registerPatternLanguagePragmas();
    void registerCommandPaletteCommands();
    void registerSettings();
    void registerDataProcessorNodes();
    void registerProviders();
    void registerDataFormatters();
    void registerLayouts();
    void registerMainMenuEntries();
    void createWelcomeScreen();

    void addFooterItems();
    void addToolbarItems();

    void registerLanguageEnUS();
    void registerLanguageDeDE();
    void registerLanguageItIT();
    void registerLanguageJaJP();
    void registerLanguageZhCN();

}

IMHEX_PLUGIN_SETUP("Built-in", "WerWolv", "Default ImHex functionality") {

    using namespace hex::plugin::builtin;

    registerLanguageEnUS();
    registerLanguageDeDE();
    registerLanguageItIT();
    registerLanguageJaJP();
    registerLanguageZhCN();

    registerViews();
    registerDataInspectorEntries();
    registerToolEntries();
    registerPatternLanguageFunctions();
    registerPatternLanguagePragmas();
    registerCommandPaletteCommands();
    registerSettings();
    registerDataProcessorNodes();
    registerProviders();
    registerDataFormatters();
    createWelcomeScreen();

    addFooterItems();
    addToolbarItems();
    registerLayouts();
    registerMainMenuEntries();
}

// This is the default plugin
// DO NOT USE THIS IN ANY OTHER PLUGIN
extern "C" bool isBuiltinPlugin() { return true; }
