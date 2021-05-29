#include <hex/plugin.hpp>
#include <imgui.h>
#include <imgui_internal.h>

#include "views/view_tty_console.hpp"

IMHEX_PLUGIN_SETUP("Windows", "WerWolv", "Windows-only features") {
    ContentRegistry::Views::add<hex::ViewTTYConsole>();
}


