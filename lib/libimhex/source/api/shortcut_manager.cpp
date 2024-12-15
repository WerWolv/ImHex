#include <hex/api/shortcut_manager.hpp>
#include <imgui.h>
#include <hex/api/content_registry.hpp>
#include <hex/helpers/auto_reset.hpp>

#include <hex/ui/view.hpp>

namespace hex {

    namespace {

        AutoReset<std::map<Shortcut, ShortcutManager::ShortcutEntry>> s_globalShortcuts;
        std::atomic<bool> s_paused;
        std::optional<Shortcut> s_prevShortcut;

    }


    void ShortcutManager::addGlobalShortcut(const Shortcut &shortcut, const std::vector<UnlocalizedString> &unlocalizedName, const std::function<void()> &callback) {
        s_globalShortcuts->insert({ shortcut, { shortcut, unlocalizedName, callback } });
    }

    void ShortcutManager::addGlobalShortcut(const Shortcut &shortcut, const UnlocalizedString &unlocalizedName, const std::function<void()> &callback) {
        s_globalShortcuts->insert({ shortcut, { shortcut, { unlocalizedName }, callback } });
    }

    void ShortcutManager::addShortcut(View *view, const Shortcut &shortcut, const std::vector<UnlocalizedString> &unlocalizedName, const std::function<void()> &callback) {
        view->m_shortcuts.insert({ shortcut + CurrentView, { shortcut, unlocalizedName, callback } });
    }

    void ShortcutManager::addShortcut(View *view, const Shortcut &shortcut, const UnlocalizedString &unlocalizedName, const std::function<void()> &callback) {
        view->m_shortcuts.insert({ shortcut + CurrentView, { shortcut, { unlocalizedName }, callback } });
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

    static void processShortcut(const Shortcut &shortcut, const std::map<Shortcut, ShortcutManager::ShortcutEntry> &shortcuts) {
        if (s_paused) return;

        if (ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopupId))
            return;

        if (shortcuts.contains(shortcut + AllowWhileTyping)) {
            shortcuts.at(shortcut + AllowWhileTyping).callback();
        } else if (shortcuts.contains(shortcut)) {
            if (!ImGui::GetIO().WantTextInput)
                shortcuts.at(shortcut).callback();
        }
    }

    void ShortcutManager::process(const View *currentView, bool ctrl, bool alt, bool shift, bool super, bool focused, u32 keyCode) {
        const Shortcut pressedShortcut = getShortcut(ctrl, alt, shift, super, focused, keyCode);
        if (keyCode != 0)
            s_prevShortcut = Shortcut(pressedShortcut.getKeys());

        processShortcut(pressedShortcut, currentView->m_shortcuts);
    }

    void ShortcutManager::processGlobals(bool ctrl, bool alt, bool shift, bool super, u32 keyCode) {
        const Shortcut pressedShortcut = getShortcut(ctrl, alt, shift, super, false, keyCode);
        if (keyCode != 0)
            s_prevShortcut = Shortcut(pressedShortcut.getKeys());

        processShortcut(pressedShortcut, s_globalShortcuts);
    }

    void ShortcutManager::clearShortcuts() {
        s_globalShortcuts->clear();
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
        std::vector<ShortcutEntry> result;

        for (auto &[shortcut, entry] : *s_globalShortcuts)
            result.push_back(entry);

        return result;
    }

    std::vector<ShortcutManager::ShortcutEntry> ShortcutManager::getViewShortcuts(const View *view) {
        std::vector<ShortcutEntry> result;

        result.reserve(view->m_shortcuts.size());
        for (auto &[shortcut, entry] : view->m_shortcuts)
            result.push_back(entry);

        return result;
    }

    static bool updateShortcutImpl(const Shortcut &oldShortcut, const Shortcut &newShortcut, std::map<Shortcut, ShortcutManager::ShortcutEntry> &shortcuts) {
        if (shortcuts.contains(oldShortcut)) {
            if (shortcuts.contains(newShortcut))
                return false;

            shortcuts[newShortcut] = shortcuts[oldShortcut];
            shortcuts[newShortcut].shortcut = newShortcut;
            shortcuts.erase(oldShortcut);
        }

        return true;
    }

    bool ShortcutManager::updateShortcut(const Shortcut &oldShortcut, const Shortcut &newShortcut, View *view) {
        if (oldShortcut == newShortcut)
            return true;

        bool result;
        if (view != nullptr) {
            result = updateShortcutImpl(oldShortcut + CurrentView, newShortcut + CurrentView , view->m_shortcuts);
        } else {
            result = updateShortcutImpl(oldShortcut, newShortcut, s_globalShortcuts);
        }

        if (result) {
            for (auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMenuItems()) {
                if (menuItem.view == view && *menuItem.shortcut == oldShortcut) {
                    *menuItem.shortcut = newShortcut;
                    break;
                }
            }
        }

        return result;
    }

}