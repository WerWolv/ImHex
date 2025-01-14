#include <hex/api/shortcut_manager.hpp>
#include <imgui.h>
#include <hex/api/content_registry.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/auto_reset.hpp>

#include <hex/ui/view.hpp>

namespace hex {

    namespace {

        AutoReset<std::map<Shortcut, ShortcutManager::ShortcutEntry>> s_globalShortcuts;
        std::atomic<bool> s_paused;
        std::optional<Shortcut> s_prevShortcut;
        bool s_macOSMode = false;
        AutoReset<std::optional<UnlocalizedString>> s_lastShortcutMainMenu;

    }

    Shortcut operator+(const Key &lhs, const Key &rhs) {
        Shortcut result;
        result.m_keys = { lhs, rhs };

        return result;
    }

    Shortcut::Shortcut(Keys key) : m_keys({ key }) {

    }
    Shortcut::Shortcut(std::set<Key> keys) : m_keys(std::move(keys)) {

    }

    Shortcut Shortcut::operator+(const Key &other) const {
        Shortcut result = *this;
        result.m_keys.insert(other);

        return result;
    }

    Shortcut& Shortcut::operator+=(const Key &other) {
        m_keys.insert(other);

        return *this;
    }

    bool Shortcut::operator<(const Shortcut &other) const {
        return m_keys < other.m_keys;
    }

    bool Shortcut::operator==(const Shortcut &other) const {
        return m_keys == other.m_keys;
    }

    bool Shortcut::isLocal() const {
        return m_keys.contains(CurrentView);
    }

    const std::set<Key>& Shortcut::getKeys() const {
        return m_keys;
    }

    bool Shortcut::has(Key key) const {
        return m_keys.contains(key);
    }

    bool Shortcut::matches(const Shortcut& other) const {
        auto left = this->m_keys;
        auto right = other.m_keys;

        left.erase(CurrentView);
        left.erase(AllowWhileTyping);
        right.erase(CurrentView);
        right.erase(AllowWhileTyping);

        return left == right;
    }


    std::string Shortcut::toString() const {
        std::string result;

        const auto CTRL_NAME     = s_macOSMode ? "⌃" : "CTRL";
        const auto ALT_NAME      = s_macOSMode ? "⌥" : "ALT";
        const auto SHIFT_NAME    = s_macOSMode ? "⇧" : "SHIFT";
        const auto SUPER_NAME    = s_macOSMode ? "⌘" : "SUPER";
        const auto Concatination = s_macOSMode ? " " : " + ";

        auto keys = m_keys;
        if (keys.erase(CTRL) > 0 || (!s_macOSMode && keys.erase(CTRLCMD) > 0)) {
            result += CTRL_NAME;
            result += Concatination;
        }
        if (keys.erase(ALT) > 0) {
            result += ALT_NAME;
            result += Concatination;
        }
        if (keys.erase(SHIFT) > 0) {
            result += SHIFT_NAME;
            result += Concatination;
        }
        if (keys.erase(SUPER) > 0 || (s_macOSMode && keys.erase(CTRLCMD) > 0)) {
            result += SUPER_NAME;
            result += Concatination;
        }
        keys.erase(CurrentView);

        for (const auto &key : keys) {
            switch (Keys(key.getKeyCode())) {
                case Keys::Space:           result += "⎵";              break;
                case Keys::Apostrophe:      result += "'";              break;
                case Keys::Comma:           result += ",";              break;
                case Keys::Minus:           result += "-";              break;
                case Keys::Period:          result += ".";              break;
                case Keys::Slash:           result += "/";              break;
                case Keys::Num0:            result += "0";              break;
                case Keys::Num1:            result += "1";              break;
                case Keys::Num2:            result += "2";              break;
                case Keys::Num3:            result += "3";              break;
                case Keys::Num4:            result += "4";              break;
                case Keys::Num5:            result += "5";              break;
                case Keys::Num6:            result += "6";              break;
                case Keys::Num7:            result += "7";              break;
                case Keys::Num8:            result += "8";              break;
                case Keys::Num9:            result += "9";              break;
                case Keys::Semicolon:       result += ";";              break;
                case Keys::Equals:          result += "=";              break;
                case Keys::A:               result += "A";              break;
                case Keys::B:               result += "B";              break;
                case Keys::C:               result += "C";              break;
                case Keys::D:               result += "D";              break;
                case Keys::E:               result += "E";              break;
                case Keys::F:               result += "F";              break;
                case Keys::G:               result += "G";              break;
                case Keys::H:               result += "H";              break;
                case Keys::I:               result += "I";              break;
                case Keys::J:               result += "J";              break;
                case Keys::K:               result += "K";              break;
                case Keys::L:               result += "L";              break;
                case Keys::M:               result += "M";              break;
                case Keys::N:               result += "N";              break;
                case Keys::O:               result += "O";              break;
                case Keys::P:               result += "P";              break;
                case Keys::Q:               result += "Q";              break;
                case Keys::R:               result += "R";              break;
                case Keys::S:               result += "S";              break;
                case Keys::T:               result += "T";              break;
                case Keys::U:               result += "U";              break;
                case Keys::V:               result += "V";              break;
                case Keys::W:               result += "W";              break;
                case Keys::X:               result += "X";              break;
                case Keys::Y:               result += "Y";              break;
                case Keys::Z:               result += "Z";              break;
                case Keys::LeftBracket:     result += "[";              break;
                case Keys::Backslash:       result += "\\";             break;
                case Keys::RightBracket:    result += "]";              break;
                case Keys::GraveAccent:     result += "`";              break;
                case Keys::World1:          result += "WORLD1";         break;
                case Keys::World2:          result += "WORLD2";         break;
                case Keys::Escape:          result += "ESC";            break;
                case Keys::Enter:           result += "⏎";              break;
                case Keys::Tab:             result += "⇥";              break;
                case Keys::Backspace:       result += "⌫";              break;
                case Keys::Insert:          result += "INSERT";         break;
                case Keys::Delete:          result += "DELETE";         break;
                case Keys::Right:           result += "RIGHT";          break;
                case Keys::Left:            result += "LEFT";           break;
                case Keys::Down:            result += "DOWN";           break;
                case Keys::Up:              result += "UP";             break;
                case Keys::PageUp:          result += "PAGEUP";         break;
                case Keys::PageDown:        result += "PAGEDOWN";       break;
                case Keys::Home:            result += "HOME";           break;
                case Keys::End:             result += "END";            break;
                case Keys::CapsLock:        result += "⇪";              break;
                case Keys::ScrollLock:      result += "SCROLLLOCK";     break;
                case Keys::NumLock:         result += "NUMLOCK";        break;
                case Keys::PrintScreen:     result += "PRINTSCREEN";    break;
                case Keys::Pause:           result += "PAUSE";          break;
                case Keys::F1:              result += "F1";             break;
                case Keys::F2:              result += "F2";             break;
                case Keys::F3:              result += "F3";             break;
                case Keys::F4:              result += "F4";             break;
                case Keys::F5:              result += "F5";             break;
                case Keys::F6:              result += "F6";             break;
                case Keys::F7:              result += "F7";             break;
                case Keys::F8:              result += "F8";             break;
                case Keys::F9:              result += "F9";             break;
                case Keys::F10:             result += "F10";            break;
                case Keys::F11:             result += "F11";            break;
                case Keys::F12:             result += "F12";            break;
                case Keys::F13:             result += "F13";            break;
                case Keys::F14:             result += "F14";            break;
                case Keys::F15:             result += "F15";            break;
                case Keys::F16:             result += "F16";            break;
                case Keys::F17:             result += "F17";            break;
                case Keys::F18:             result += "F18";            break;
                case Keys::F19:             result += "F19";            break;
                case Keys::F20:             result += "F20";            break;
                case Keys::F21:             result += "F21";            break;
                case Keys::F22:             result += "F22";            break;
                case Keys::F23:             result += "F23";            break;
                case Keys::F24:             result += "F24";            break;
                case Keys::F25:             result += "F25";            break;
                case Keys::KeyPad0:         result += "KP0";            break;
                case Keys::KeyPad1:         result += "KP1";            break;
                case Keys::KeyPad2:         result += "KP2";            break;
                case Keys::KeyPad3:         result += "KP3";            break;
                case Keys::KeyPad4:         result += "KP4";            break;
                case Keys::KeyPad5:         result += "KP5";            break;
                case Keys::KeyPad6:         result += "KP6";            break;
                case Keys::KeyPad7:         result += "KP7";            break;
                case Keys::KeyPad8:         result += "KP8";            break;
                case Keys::KeyPad9:         result += "KP9";            break;
                case Keys::KeyPadDecimal:   result += "KPDECIMAL";      break;
                case Keys::KeyPadDivide:    result += "KPDIVIDE";       break;
                case Keys::KeyPadMultiply:  result += "KPMULTIPLY";     break;
                case Keys::KeyPadSubtract:  result += "KPSUBTRACT";     break;
                case Keys::KeyPadAdd:       result += "KPADD";          break;
                case Keys::KeyPadEnter:     result += "KPENTER";        break;
                case Keys::KeyPadEqual:     result += "KPEQUAL";        break;
                case Keys::Menu:            result += "MENU";           break;
                default:
                    continue;
            }

            result += Concatination;
        }

        if (result.ends_with(Concatination))
            result = result.substr(0, result.size() - strlen(Concatination));

        return result;
    }

    KeyEquivalent Shortcut::toKeyEquivalent() const {
        #if defined(OS_MACOS)
            if (*this == None)
                return { };

            KeyEquivalent result = {};
            result.valid = true;

            for (const auto &key : m_keys) {
                switch (key.getKeyCode()) {
                    case CTRL.getKeyCode():
                        result.ctrl = true;
                        break;
                    case SHIFT.getKeyCode():
                        result.shift = true;
                        break;
                    case ALT.getKeyCode():
                        result.opt = true;
                        break;
                    case SUPER.getKeyCode():
                    case CTRLCMD.getKeyCode():
                        result.cmd = true;
                        break;
                    case CurrentView.getKeyCode(): break;
                    case AllowWhileTyping.getKeyCode(): break;
                    default:
                        macosGetKey(Keys(key.getKeyCode()), &result.key);
                        break;
                }
            }

            return result;
        #else
            return { };
        #endif
    }



    void ShortcutManager::addGlobalShortcut(const Shortcut &shortcut, const std::vector<UnlocalizedString> &unlocalizedName, const std::function<void()> &callback, const EnabledCallback &enabledCallback) {
        log::debug("Adding global shortcut {} for {}", shortcut.toString(), unlocalizedName.back().get());
        auto [it, inserted] = s_globalShortcuts->insert({ shortcut, { shortcut, unlocalizedName, callback, enabledCallback } });
        if (!inserted) log::error("Failed to add shortcut!");
    }

    void ShortcutManager::addGlobalShortcut(const Shortcut &shortcut, const UnlocalizedString &unlocalizedName, const std::function<void()> &callback, const EnabledCallback &enabledCallback) {
        log::debug("Adding global shortcut {} for {}", shortcut.toString(), unlocalizedName.get());
        auto [it, inserted] = s_globalShortcuts->insert({ shortcut, { shortcut, { unlocalizedName }, callback, enabledCallback } });
        if (!inserted) log::error("Failed to add shortcut!");
    }

    void ShortcutManager::addShortcut(View *view, const Shortcut &shortcut, const std::vector<UnlocalizedString> &unlocalizedName, const std::function<void()> &callback, const EnabledCallback &enabledCallback) {
        log::debug("Adding shortcut {} for {}", shortcut.toString(), unlocalizedName.back().get());
        auto [it, inserted] = view->m_shortcuts.insert({ shortcut + CurrentView, { shortcut, unlocalizedName, callback, enabledCallback } });
        if (!inserted) log::error("Failed to add shortcut!");
    }

    void ShortcutManager::addShortcut(View *view, const Shortcut &shortcut, const UnlocalizedString &unlocalizedName, const std::function<void()> &callback, const EnabledCallback &enabledCallback) {
        log::debug("Adding shortcut {} for {}", shortcut.toString(), unlocalizedName.get());
        auto [it, inserted] = view->m_shortcuts.insert({ shortcut + CurrentView, { shortcut, { unlocalizedName }, callback, enabledCallback } });
        if (!inserted) log::error("Failed to add shortcut!");
    }

    static Shortcut getShortcut(bool ctrl, bool alt, bool shift, bool super, bool focused, u32 keyCode) {
        Shortcut pressedShortcut;

        if (ctrl)
            pressedShortcut += s_macOSMode ? CTRL : CTRLCMD;
        if (alt)
            pressedShortcut += ALT;
        if (shift)
            pressedShortcut += SHIFT;
        if (super)
            pressedShortcut += s_macOSMode ? CTRLCMD : SUPER;
        if (focused)
            pressedShortcut += CurrentView;

        pressedShortcut += static_cast<Keys>(keyCode);

        return pressedShortcut;
    }

    static void processShortcut(Shortcut shortcut, const std::map<Shortcut, ShortcutManager::ShortcutEntry> &shortcuts) {
        if (s_paused) return;

        if (ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopupId))
            return;

        const bool currentlyTyping = ImGui::GetIO().WantTextInput;

        auto it = shortcuts.find(shortcut + AllowWhileTyping);
        if (!currentlyTyping && it == shortcuts.end()) {
            if (it == shortcuts.end())
                it = shortcuts.find(shortcut);
        }

        if (it != shortcuts.end()) {
            const auto &[foundShortcut, entry] = *it;

            if (entry.enabledCallback()) {
                entry.callback();

                if (!entry.unlocalizedName.empty()) {
                    s_lastShortcutMainMenu = entry.unlocalizedName.front();
                }
            }
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

    std::optional<UnlocalizedString> ShortcutManager::getLastActivatedMenu() {
        return *s_lastShortcutMainMenu;
    }

    void ShortcutManager::resetLastActivatedMenu() {
        s_lastShortcutMainMenu->reset();
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

    bool ShortcutManager::updateShortcut(const Shortcut &oldShortcut, Shortcut newShortcut, View *view) {
        if (oldShortcut.matches(newShortcut))
            return true;

        if (oldShortcut.has(AllowWhileTyping))
            newShortcut += AllowWhileTyping;

        bool result;
        if (view != nullptr) {
            result = updateShortcutImpl(oldShortcut + CurrentView, newShortcut + CurrentView , view->m_shortcuts);
        } else {
            result = updateShortcutImpl(oldShortcut, newShortcut, s_globalShortcuts);
        }

        if (result) {
            for (auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMenuItemsMutable()) {
                if (menuItem.view == view && menuItem.shortcut == oldShortcut) {
                    menuItem.shortcut = newShortcut;
                    break;
                }
            }
        }

        return result;
    }

    void ShortcutManager::enableMacOSMode() {
        s_macOSMode = true;
    }


}