#include <hex/api/keybinding.hpp>
#include <hex/helpers/shared_data.hpp>
#include <imgui.h>

#include <hex/views/view.hpp>

namespace hex {

    void ShortcutManager::addGlobalShortcut(const Shortcut &shortcut, const std::function<void()> &callback) {
        SharedData::globalShortcuts.insert({ shortcut, callback });
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
        else if (SharedData::globalShortcuts.contains(pressedShortcut))
            SharedData::globalShortcuts[pressedShortcut]();
    }

}