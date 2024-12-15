#pragma once

#include <hex.hpp>
#include <hex/api/localization_manager.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include <GLFW/glfw3.h>

struct ImGuiWindow;

namespace hex {

    class View;

    enum class Keys {
        Space          = GLFW_KEY_SPACE,
        Apostrophe     = GLFW_KEY_APOSTROPHE,
        Comma          = GLFW_KEY_COMMA,
        Minus          = GLFW_KEY_MINUS,
        Period         = GLFW_KEY_PERIOD,
        Slash          = GLFW_KEY_SLASH,
        Num0           = GLFW_KEY_0,
        Num1           = GLFW_KEY_1,
        Num2           = GLFW_KEY_2,
        Num3           = GLFW_KEY_3,
        Num4           = GLFW_KEY_4,
        Num5           = GLFW_KEY_5,
        Num6           = GLFW_KEY_6,
        Num7           = GLFW_KEY_7,
        Num8           = GLFW_KEY_8,
        Num9           = GLFW_KEY_9,
        Semicolon      = GLFW_KEY_SEMICOLON,
        Equals         = GLFW_KEY_EQUAL,
        A              = GLFW_KEY_A,
        B              = GLFW_KEY_B,
        C              = GLFW_KEY_C,
        D              = GLFW_KEY_D,
        E              = GLFW_KEY_E,
        F              = GLFW_KEY_F,
        G              = GLFW_KEY_G,
        H              = GLFW_KEY_H,
        I              = GLFW_KEY_I,
        J              = GLFW_KEY_J,
        K              = GLFW_KEY_K,
        L              = GLFW_KEY_L,
        M              = GLFW_KEY_M,
        N              = GLFW_KEY_N,
        O              = GLFW_KEY_O,
        P              = GLFW_KEY_P,
        Q              = GLFW_KEY_Q,
        R              = GLFW_KEY_R,
        S              = GLFW_KEY_S,
        T              = GLFW_KEY_T,
        U              = GLFW_KEY_U,
        V              = GLFW_KEY_V,
        W              = GLFW_KEY_W,
        X              = GLFW_KEY_X,
        Y              = GLFW_KEY_Y,
        Z              = GLFW_KEY_Z,
        LeftBracket    = GLFW_KEY_LEFT_BRACKET,
        Backslash      = GLFW_KEY_BACKSLASH,
        RightBracket   = GLFW_KEY_RIGHT_BRACKET,
        GraveAccent    = GLFW_KEY_GRAVE_ACCENT,
        World1         = GLFW_KEY_WORLD_1,
        World2         = GLFW_KEY_WORLD_2,
        Escape         = GLFW_KEY_ESCAPE,
        Enter          = GLFW_KEY_ENTER,
        Tab            = GLFW_KEY_TAB,
        Backspace      = GLFW_KEY_BACKSPACE,
        Insert         = GLFW_KEY_INSERT,
        Delete         = GLFW_KEY_DELETE,
        Right          = GLFW_KEY_RIGHT,
        Left           = GLFW_KEY_LEFT,
        Down           = GLFW_KEY_DOWN,
        Up             = GLFW_KEY_UP,
        PageUp         = GLFW_KEY_PAGE_UP,
        PageDown       = GLFW_KEY_PAGE_DOWN,
        Home           = GLFW_KEY_HOME,
        End            = GLFW_KEY_END,
        CapsLock       = GLFW_KEY_CAPS_LOCK,
        ScrollLock     = GLFW_KEY_SCROLL_LOCK,
        NumLock        = GLFW_KEY_NUM_LOCK,
        PrintScreen    = GLFW_KEY_PRINT_SCREEN,
        Pause          = GLFW_KEY_PAUSE,
        F1             = GLFW_KEY_F1,
        F2             = GLFW_KEY_F2,
        F3             = GLFW_KEY_F3,
        F4             = GLFW_KEY_F4,
        F5             = GLFW_KEY_F5,
        F6             = GLFW_KEY_F6,
        F7             = GLFW_KEY_F7,
        F8             = GLFW_KEY_F8,
        F9             = GLFW_KEY_F9,
        F10            = GLFW_KEY_F10,
        F11            = GLFW_KEY_F11,
        F12            = GLFW_KEY_F12,
        F13            = GLFW_KEY_F13,
        F14            = GLFW_KEY_F14,
        F15            = GLFW_KEY_F15,
        F16            = GLFW_KEY_F16,
        F17            = GLFW_KEY_F17,
        F18            = GLFW_KEY_F18,
        F19            = GLFW_KEY_F19,
        F20            = GLFW_KEY_F20,
        F21            = GLFW_KEY_F21,
        F22            = GLFW_KEY_F22,
        F23            = GLFW_KEY_F23,
        F24            = GLFW_KEY_F24,
        F25            = GLFW_KEY_F25,
        KeyPad0        = GLFW_KEY_KP_0,
        KeyPad1        = GLFW_KEY_KP_1,
        KeyPad2        = GLFW_KEY_KP_2,
        KeyPad3        = GLFW_KEY_KP_3,
        KeyPad4        = GLFW_KEY_KP_4,
        KeyPad5        = GLFW_KEY_KP_5,
        KeyPad6        = GLFW_KEY_KP_6,
        KeyPad7        = GLFW_KEY_KP_7,
        KeyPad8        = GLFW_KEY_KP_8,
        KeyPad9        = GLFW_KEY_KP_9,
        KeyPadDecimal  = GLFW_KEY_KP_DECIMAL,
        KeyPadDivide   = GLFW_KEY_KP_DIVIDE,
        KeyPadMultiply = GLFW_KEY_KP_MULTIPLY,
        KeyPadSubtract = GLFW_KEY_KP_SUBTRACT,
        KeyPadAdd      = GLFW_KEY_KP_ADD,
        KeyPadEnter    = GLFW_KEY_KP_ENTER,
        KeyPadEqual    = GLFW_KEY_KP_EQUAL,
        Menu           = GLFW_KEY_MENU,
    };


    class Key {
    public:
        constexpr Key() = default;
        constexpr Key(Keys key) : m_key(static_cast<u32>(key)) { }

        auto operator<=>(const Key &) const = default;

        [[nodiscard]] constexpr u32 getKeyCode() const { return m_key; }
    private:
        u32 m_key = 0;
    };


    constexpr static auto CTRL              = Key(static_cast<Keys>(0x0100'0000));
    constexpr static auto ALT               = Key(static_cast<Keys>(0x0200'0000));
    constexpr static auto SHIFT             = Key(static_cast<Keys>(0x0400'0000));
    constexpr static auto SUPER             = Key(static_cast<Keys>(0x0800'0000));
    constexpr static auto CurrentView       = Key(static_cast<Keys>(0x1000'0000));
    constexpr static auto AllowWhileTyping  = Key(static_cast<Keys>(0x2000'0000));

    #if defined (OS_MACOS)
        constexpr static auto CTRLCMD = SUPER;
    #else
        constexpr static auto CTRLCMD = CTRL;
    #endif

    class Shortcut {
    public:
        Shortcut() = default;
        Shortcut(Keys key) : m_keys({ key }) { }
        explicit Shortcut(std::set<Key> keys) : m_keys(std::move(keys)) { }
        Shortcut(const Shortcut &other) = default;
        Shortcut(Shortcut &&) noexcept = default;

        Shortcut& operator=(const Shortcut &other) = default;

        Shortcut& operator=(Shortcut &&) noexcept = default;

        constexpr static inline auto None = Keys(0);

        Shortcut operator+(const Key &other) const {
            Shortcut result = *this;
            result.m_keys.insert(other);

            return result;
        }

        Shortcut &operator+=(const Key &other) {
            m_keys.insert(other);

            return *this;
        }

        bool operator<(const Shortcut &other) const {
            return m_keys < other.m_keys;
        }

        bool operator==(const Shortcut &other) const {
            auto thisKeys = m_keys;
            auto otherKeys = other.m_keys;

            thisKeys.erase(CurrentView);
            thisKeys.erase(AllowWhileTyping);
            otherKeys.erase(CurrentView);
            otherKeys.erase(AllowWhileTyping);

            return thisKeys == otherKeys;
        }

        bool isLocal() const {
            return m_keys.contains(CurrentView);
        }

        std::string toString() const {
            std::string result;

            #if defined(OS_MACOS)
                constexpr static auto CTRL_NAME    = "CTRL";
                constexpr static auto ALT_NAME     = "OPT";
                constexpr static auto SHIFT_NAME   = "SHIFT";
                constexpr static auto SUPER_NAME   = "CMD";
            #else
                constexpr static auto CTRL_NAME    = "CTRL";
                constexpr static auto ALT_NAME     = "ALT";
                constexpr static auto SHIFT_NAME   = "SHIFT";
                constexpr static auto SUPER_NAME   = "SUPER";
            #endif

            constexpr static auto Concatination = " + ";

            auto keys = m_keys;
            if (keys.erase(CTRL) > 0) {
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
            if (keys.erase(SUPER) > 0) {
                result += SUPER_NAME;
                result += Concatination;
            }
            keys.erase(CurrentView);

            for (const auto &key : keys) {
                switch (Keys(key.getKeyCode())) {
                    case Keys::Space:           result += "SPACE";          break;
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
                    case Keys::Enter:           result += "ENTER";          break;
                    case Keys::Tab:             result += "TAB";            break;
                    case Keys::Backspace:       result += "BACKSPACE";      break;
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
                    case Keys::CapsLock:        result += "CAPSLOCK";       break;
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

                result += " + ";
            }

            if (result.ends_with(" + "))
                result = result.substr(0, result.size() - 3);

            return result;
        }

        const std::set<Key>& getKeys() const { return m_keys; }

    private:
        friend Shortcut operator+(const Key &lhs, const Key &rhs);

        std::set<Key> m_keys;
    };

    inline Shortcut operator+(const Key &lhs, const Key &rhs) {
        Shortcut result;
        result.m_keys = { lhs, rhs };

        return result;
    }

    /**
     * @brief The ShortcutManager handles global and view-specific shortcuts.
     * New shortcuts can be constructed using the + operator on Key objects. For example: CTRL + ALT + Keys::A
     */
    class ShortcutManager {
    public:
        using Callback = std::function<void()>;
        struct ShortcutEntry {
            Shortcut shortcut;
            std::vector<UnlocalizedString> unlocalizedName;
            Callback callback;
        };

        /**
         * @brief Add a global shortcut. Global shortcuts can be triggered regardless of what view is currently focused
         * @param shortcut The shortcut to add.
         * @param unlocalizedName The unlocalized name of the shortcut
         * @param callback The callback to call when the shortcut is triggered.
         */
        static void addGlobalShortcut(const Shortcut &shortcut, const std::vector<UnlocalizedString> &unlocalizedName, const Callback &callback);
        static void addGlobalShortcut(const Shortcut &shortcut, const UnlocalizedString &unlocalizedName, const Callback &callback);

        /**
         * @brief Add a view-specific shortcut. View-specific shortcuts can only be triggered when the specified view is focused.
         * @param view The view to add the shortcut to.
         * @param shortcut The shortcut to add.
         * @param unlocalizedName The unlocalized name of the shortcut
         * @param callback The callback to call when the shortcut is triggered.
         */
        static void addShortcut(View *view, const Shortcut &shortcut, const std::vector<UnlocalizedString> &unlocalizedName, const Callback &callback);
        static void addShortcut(View *view, const Shortcut &shortcut, const UnlocalizedString &unlocalizedName, const Callback &callback);


        /**
         * @brief Process a key event. This should be called from the main loop.
         * @param currentView Current view to process
         * @param ctrl Whether the CTRL key is pressed
         * @param alt Whether the ALT key is pressed
         * @param shift Whether the SHIFT key is pressed
         * @param super Whether the SUPER key is pressed
         * @param focused Whether the current view is focused
         * @param keyCode The key code of the key that was pressed
         */
        static void process(const View *currentView, bool ctrl, bool alt, bool shift, bool super, bool focused, u32 keyCode);

        /**
         * @brief Process a key event. This should be called from the main loop.
         * @param ctrl Whether the CTRL key is pressed
         * @param alt Whether the ALT key is pressed
         * @param shift Whether the SHIFT key is pressed
         * @param super Whether the SUPER key is pressed
         * @param keyCode The key code of the key that was pressed
         */
        static void processGlobals(bool ctrl, bool alt, bool shift, bool super, u32 keyCode);

        /**
         * @brief Clear all shortcuts
         */
        static void clearShortcuts();

        static void resumeShortcuts();
        static void pauseShortcuts();

        [[nodiscard]] static std::optional<Shortcut> getPreviousShortcut();

        [[nodiscard]] static std::vector<ShortcutEntry> getGlobalShortcuts();
        [[nodiscard]] static std::vector<ShortcutEntry> getViewShortcuts(const View *view);

        [[nodiscard]] static bool updateShortcut(const Shortcut &oldShortcut, const Shortcut &newShortcut, View *view = nullptr);
    };

}