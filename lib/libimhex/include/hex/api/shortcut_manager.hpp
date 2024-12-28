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

    enum class Keys : u32 {
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

        bool operator==(const Key &) const = default;
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
    constexpr static auto CTRLCMD           = Key(static_cast<Keys>(0x4000'0000));

    class Shortcut {
    public:
        Shortcut() = default;
        Shortcut(Keys key);
        explicit Shortcut(std::set<Key> keys);
        Shortcut(const Shortcut &other) = default;
        Shortcut(Shortcut &&) noexcept = default;

        constexpr static auto None = Keys(0);

        Shortcut& operator=(const Shortcut &other) = default;
        Shortcut& operator=(Shortcut &&) noexcept = default;

        Shortcut operator+(const Key &other) const;
        Shortcut &operator+=(const Key &other);
        bool operator<(const Shortcut &other) const;
        bool operator==(const Shortcut &other) const;

        bool isLocal() const;
        std::string toString() const;
        const std::set<Key>& getKeys() const;
        bool has(Key key) const;
        bool matches(const Shortcut &other) const;

    private:
        friend Shortcut operator+(const Key &lhs, const Key &rhs);

        std::set<Key> m_keys;
    };

    Shortcut operator+(const Key &lhs, const Key &rhs);

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

        static void enableMacOSMode();

        [[nodiscard]] static std::optional<Shortcut> getPreviousShortcut();

        [[nodiscard]] static std::vector<ShortcutEntry> getGlobalShortcuts();
        [[nodiscard]] static std::vector<ShortcutEntry> getViewShortcuts(const View *view);

        [[nodiscard]] static bool updateShortcut(const Shortcut &oldShortcut, Shortcut newShortcut, View *view = nullptr);
    };

}