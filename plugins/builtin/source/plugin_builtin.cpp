#include <hex/plugin.hpp>

namespace hex::plugin::builtin {

    void registerDataInspectorEntries();
    void registerToolEntries();
    void registerPatternLanguageFunctions();
    void registerCommandPaletteCommands();
    void registerSettings();

}

IMHEX_PLUGIN_SETUP {

    using namespace hex::plugin::builtin;

    registerDataInspectorEntries();
    registerToolEntries();
    registerPatternLanguageFunctions();
    registerCommandPaletteCommands();
    registerSettings();

}


