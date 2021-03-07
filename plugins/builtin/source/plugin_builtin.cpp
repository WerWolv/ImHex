#include <hex/plugin.hpp>

namespace hex::plugin::builtin {

    void registerDataInspectorEntries();
    void registerToolEntries();
    void registerPatternLanguageFunctions();
    void registerCommandPaletteCommands();
    void registerSettings();
    void registerDataProcessorNodes();

    void addFooterItems();

    void registerLanguageEnUS();
    void registerLanguageDeDE();
    void registerLanguageItIT();

}

IMHEX_PLUGIN_SETUP("Built-in", "WerWolv", "Default ImHex functionality") {

    using namespace hex::plugin::builtin;

    registerDataInspectorEntries();
    registerToolEntries();
    registerPatternLanguageFunctions();
    registerCommandPaletteCommands();
    registerSettings();
    registerDataProcessorNodes();

    addFooterItems();

    registerLanguageEnUS();
    registerLanguageDeDE();
    registerLanguageItIT();
}


