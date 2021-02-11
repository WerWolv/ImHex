#include <hex/plugin.hpp>

namespace hex::plugin::builtin {

    void registerLanguageEnUS();

    void registerDataInspectorEntries();
    void registerToolEntries();
    void registerPatternLanguageFunctions();
    void registerCommandPaletteCommands();
    void registerSettings();
    void registerDataProcessorNodes();

}

IMHEX_PLUGIN_SETUP {

    using namespace hex::plugin::builtin;

    registerLanguageEnUS();

    registerDataInspectorEntries();
    registerToolEntries();
    registerPatternLanguageFunctions();
    registerCommandPaletteCommands();
    registerSettings();
    registerDataProcessorNodes();
}


