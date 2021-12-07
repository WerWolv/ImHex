#include <hex/plugin.hpp>

namespace hex::plugin::builtin {

    void registerViews();
    void registerDataInspectorEntries();
    void registerToolEntries();
    void registerPatternLanguageFunctions();
    void registerCommandPaletteCommands();
    void registerSettings();
    void registerDataProcessorNodes();
    void registerProviders();

    void addFooterItems();
    void addToolbarItems();

    void registerLanguageEnUS();
    void registerLanguageDeDE();
    void registerLanguageItIT();
    void registerLanguageZhCN();

}

IMHEX_PLUGIN_SETUP("Built-in", "WerWolv", "Default ImHex functionality") {

    using namespace hex::plugin::builtin;

    registerViews();
    registerDataInspectorEntries();
    registerToolEntries();
    registerPatternLanguageFunctions();
    registerCommandPaletteCommands();
    registerSettings();
    registerDataProcessorNodes();
    registerProviders();

    addFooterItems();
    addToolbarItems();

    registerLanguageEnUS();
    registerLanguageDeDE();
    registerLanguageItIT();
    registerLanguageZhCN();
}


