#include <hex/api/keybinding.hpp>
#include <imgui.h>

#include <hex/ui/view.hpp>

namespace hex {

    std::map<Shortcut, std::function<void()>> ShortcutManager::s_globalShortcuts;

    void ShortcutManager::addGlobalShortcut(const Shortcut &shortcut, const std::function<void()> &callback) {
        ShortcutManager::s_globalShortcuts.insert({ shortcut, callback });
    }

    void ShortcutManager::addShortcut(View *view, const Shortcut &shortcut, const std::function<void()> &callback) {
        view->m_shortcuts.insert({ shortcut, callback });
    }

    void ShortcutManager::process(View *currentView, bool ctrl, bool alt, bool shift, bool super, bool focused, u32 keyCode) {
        Shortcut pressedShortcut;

        if (ctrl)
            pressedShortcut += CTRL;
        if (alt)
            pressedShortcut += ALT;
        if (shift)
            pressedShortcut += SHIFT;
        if (super)
            pressedShortcut += SUPER;

        pressedShortcut += static_cast<Keys>(keyCode);

        if (focused && currentView->m_shortcuts.contains(pressedShortcut))
            currentView->m_shortcuts[pressedShortcut]();
        else if (ShortcutManager::s_globalShortcuts.contains(pressedShortcut))
            ShortcutManager::s_globalShortcuts[pressedShortcut]();
    }

    void ShortcutManager::clearShortcuts() {
        ShortcutManager::s_globalShortcuts.clear();
    }

}