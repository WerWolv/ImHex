#include <hex/plugin.hpp>

#include "views/view_tty_console.hpp"

namespace hex::plugin::windows {

    void registerLanguageEnUS();
    void registerLanguageZhCN();

    void addFooterItems();
    void registerSettings();
}


IMHEX_PLUGIN_SETUP("Windows", "WerWolv", "Windows-only features") {
    using namespace hex::plugin::windows;

    hex::ContentRegistry::Views::add<ViewTTYConsole>();

    registerLanguageEnUS();
    registerLanguageZhCN();

    addFooterItems();
    registerSettings();
}


