#pragma once
#include <hex/api/shortcut_manager.hpp>

namespace hex::menu {

    void enableNativeMenuBar(bool enabled);
    bool isNativeMenuBarUsed();

    bool beginMainMenuBar();
    void endMainMenuBar();

    bool beginMenu(const char *label, bool enabled = true);
    void endMenu();

    bool beginMenuEx(const char* label, const char* icon, bool enabled = true);

    bool menuItem(const char *label, const Shortcut &shortcut = Shortcut::None, bool selected = false, bool enabled = true);
    bool menuItem(const char *label, const Shortcut &shortcut, bool *selected, bool enabled = true);

    bool menuItemEx(const char *label, const char *icon, const Shortcut &shortcut = Shortcut::None, bool selected = false, bool enabled = true);
    bool menuItemEx(const char *label, const char *icon, const Shortcut &shortcut, bool *selected, bool enabled = true);

    void menuSeparator();

}
