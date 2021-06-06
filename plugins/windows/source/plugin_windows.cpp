#include <hex/plugin.hpp>
#include <imgui.h>
#include <imgui_internal.h>

#include "views/view_tty_console.hpp"

namespace hex::plugin::windows {

    void registerLanguageEnUS();

}


IMHEX_PLUGIN_SETUP("Windows", "WerWolv", "Windows-only features") {
    using namespace hex::plugin::windows;

    ContentRegistry::Views::add<ViewTTYConsole>();

    registerLanguageEnUS();
}


