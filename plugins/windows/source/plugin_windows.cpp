#include <hex/plugin.hpp>

#include "views/view_tty_console.hpp"

namespace hex::plugin::windows {

    void registerLanguageEnUS();
    void registerLanguageZhCN();

    void addFooterItems();
}


IMHEX_PLUGIN_SETUP("Windows", "WerWolv", "Windows-only features") {
    using namespace hex::plugin::windows;

    ContentRegistry::Views::add<ViewTTYConsole>();

    registerLanguageEnUS();
    registerLanguageZhCN();

    addFooterItems();
}


