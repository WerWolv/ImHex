#pragma once

#include <hex.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/helpers/keys.hpp>

#include <functional>
#include <optional>
#include <set>
#include <string>

struct ImGuiWindow;

struct KeyEquivalent {
    bool valid;
    bool ctrl, opt, cmd, shift;
    int key;
};

namespace hex {

    class View;

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
        KeyEquivalent toKeyEquivalent() const;
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
        using EnabledCallback = std::function<bool()>;
        struct ShortcutEntry {
            Shortcut shortcut;
            std::vector<UnlocalizedString> unlocalizedName;
            Callback callback;
            EnabledCallback enabledCallback;
        };

        /**
         * @brief Add a global shortcut. Global shortcuts can be triggered regardless of what view is currently focused
         * @param shortcut The shortcut to add.
         * @param unlocalizedName The unlocalized name of the shortcut
         * @param callback The callback to call when the shortcut is triggered.
         * @param enabledCallback Callback that's called to check if this shortcut is enabled
         */
        static void addGlobalShortcut(const Shortcut &shortcut, const std::vector<UnlocalizedString> &unlocalizedName, const Callback &callback, const EnabledCallback &enabledCallback = []{ return true; });
        static void addGlobalShortcut(const Shortcut &shortcut, const UnlocalizedString &unlocalizedName, const Callback &callback, const EnabledCallback &enabledCallback = []{ return true; });

        /**
         * @brief Add a view-specific shortcut. View-specific shortcuts can only be triggered when the specified view is focused.
         * @param view The view to add the shortcut to.
         * @param shortcut The shortcut to add.
         * @param unlocalizedName The unlocalized name of the shortcut
         * @param callback The callback to call when the shortcut is triggered.
         * @param enabledCallback Callback that's called to check if this shortcut is enabled
         */
        static void addShortcut(View *view, const Shortcut &shortcut, const std::vector<UnlocalizedString> &unlocalizedName, const Callback &callback, const EnabledCallback &enabledCallback = []{ return true; });
        static void addShortcut(View *view, const Shortcut &shortcut, const UnlocalizedString &unlocalizedName, const Callback &callback, const EnabledCallback &enabledCallback = []{ return true; });


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

        [[nodiscard]] static std::optional<UnlocalizedString> getLastActivatedMenu();
        static void resetLastActivatedMenu();

        [[nodiscard]] static std::optional<Shortcut> getPreviousShortcut();

        [[nodiscard]] static std::vector<ShortcutEntry> getGlobalShortcuts();
        [[nodiscard]] static std::vector<ShortcutEntry> getViewShortcuts(const View *view);

        [[nodiscard]] static bool updateShortcut(const Shortcut &oldShortcut, Shortcut newShortcut, View *view = nullptr);
    };

}