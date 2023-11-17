#include <hex/api/keybinding.hpp>
#include <imgui.h>
#include <hex/api/content_registry.hpp>

#include <hex/ui/view.hpp>

namespace hex {

    namespace {

        std::map<Shortcut, ShortcutManager::ShortcutEntry> s_globalShortcuts;
        std::atomic<bool> s_paused;
        std::optional<Shortcut> s_prevShortcut;

    }


    void ShortcutManager::addGlobalShortcut(const Shortcut &shortcut, const std::string &unlocalizedName, const std::function<void()> &callback) {
        s_globalShortcuts.insert({ shortcut, { shortcut, unlocalizedName, callback } });
    }

    void ShortcutManager::addShortcut(View *view, const Shortcut &shortcut, const std::string &unlocalizedName, const std::function<void()> &callback) {
        view->m_shortcuts.insert({ shortcut + CurrentView, { shortcut, unlocalizedName, callback } });
    }

    static Shortcut getShortcut(bool ctrl, bool alt, bool shift, bool super, bool focused, u32 keyCode) {
        Shortcut pressedShortcut;

        if (ctrl)
            pressedShortcut += CTRL;
        if (alt)
            pressedShortcut += ALT;
        if (shift)
            pressedShortcut += SHIFT;
        if (super)
            pressedShortcut += SUPER;
        if (focused)
            pressedShortcut += CurrentView;

        pressedShortcut += static_cast<Keys>(keyCode);

        return pressedShortcut;
    }

    void ShortcutManager::process(const std::unique_ptr<View> &currentView, bool ctrl, bool alt, bool shift, bool super, bool focused, u32 keyCode) {
        Shortcut pressedShortcut = getShortcut(ctrl, alt, shift, super, focused, keyCode);

        if (keyCode != 0)
            s_prevShortcut = Shortcut(pressedShortcut.getKeys());

        if (s_paused) return;

        if (ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopupId))
                return;

        if (currentView->m_shortcuts.contains(pressedShortcut + AllowWhileTyping)) {
            currentView->m_shortcuts[pressedShortcut + AllowWhileTyping].callback();
        } else if (currentView->m_shortcuts.contains(pressedShortcut)) {
            if (!ImGui::GetIO().WantTextInput)
                currentView->m_shortcuts[pressedShortcut].callback();
        }
    }

    void ShortcutManager::processGlobals(bool ctrl, bool alt, bool shift, bool super, u32 keyCode) {
        Shortcut pressedShortcut = getShortcut(ctrl, alt, shift, super, false, keyCode);

        if (keyCode != 0)
            s_prevShortcut = pressedShortcut;

        if (s_paused) return;

        if (ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopupId))
            return;

        if (s_globalShortcuts.contains(pressedShortcut + AllowWhileTyping)) {
            s_globalShortcuts[pressedShortcut + AllowWhileTyping].callback();
        } else if (s_globalShortcuts.contains(pressedShortcut)) {
            if (!ImGui::GetIO().WantTextInput)
                s_globalShortcuts[pressedShortcut].callback();
        }
    }

    void ShortcutManager::clearShortcuts() {
        s_globalShortcuts.clear();
    }

    void ShortcutManager::resumeShortcuts() {
        s_paused = false;
    }

    void ShortcutManager::pauseShortcuts() {
        s_paused = true;
        s_prevShortcut.reset();
    }

    std::optional<Shortcut> ShortcutManager::getPreviousShortcut() {
        return s_prevShortcut;
    }

    std::vector<ShortcutManager::ShortcutEntry> ShortcutManager::getGlobalShortcuts() {
        std::vector<ShortcutManager::ShortcutEntry> result;

        for (auto &[shortcut, entry] : s_globalShortcuts)
            result.push_back(entry);

        return result;
    }

    std::vector<ShortcutManager::ShortcutEntry> ShortcutManager::getViewShortcuts(View *view) {
        std::vector<ShortcutManager::ShortcutEntry> result;

        for (auto &[shortcut, entry] : view->m_shortcuts)
            result.push_back(entry);

        return result;
    }

    void ShortcutManager::updateShortcut(const Shortcut &oldShortcut, const Shortcut &newShortcut, View *view) {
        if (oldShortcut == newShortcut)
            return;

        if (view != nullptr) {
            if (view->m_shortcuts.contains(oldShortcut + CurrentView)) {
                view->m_shortcuts[newShortcut + CurrentView] = view->m_shortcuts[oldShortcut + CurrentView];
                view->m_shortcuts[newShortcut + CurrentView].shortcut = newShortcut + CurrentView;
                view->m_shortcuts.erase(oldShortcut + CurrentView);
            }
        } else {
            if (s_globalShortcuts.contains(oldShortcut)) {
                s_globalShortcuts[newShortcut] = s_globalShortcuts[oldShortcut];
                s_globalShortcuts[newShortcut].shortcut = newShortcut;
                s_globalShortcuts.erase(oldShortcut);
            }
        }
    }

}