#pragma once

#include <hex.hpp>

#include <hex/api/shortcut_manager.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

#include <string>
#include <vector>
#include <map>
#include <functional>

EXPORT_MODULE namespace hex {

    /* User Interface Registry. Allows adding new items to various interfaces */
    namespace ContentRegistry::UserInterface {

        struct Icon {
            Icon(const char *glyph, ImGuiCustomCol color = ImGuiCustomCol(0x00)) : glyph(glyph), color(color) {}

            std::string glyph;
            ImGuiCustomCol color;
        };

        namespace impl {

            using DrawCallback      = std::function<void()>;
            using MenuCallback      = std::function<void()>;
            using EnabledCallback   = std::function<bool()>;
            using SelectedCallback  = std::function<bool()>;
            using ClickCallback     = std::function<void()>;
            using ToggleCallback    = std::function<void(bool)>;

            struct MainMenuItem {
                UnlocalizedString unlocalizedName;
            };

            struct MenuItem {
                std::vector<UnlocalizedString> unlocalizedNames;
                Icon icon;
                Shortcut shortcut;
                View *view;
                MenuCallback callback;
                EnabledCallback enabledCallback;
                SelectedCallback selectedCallback;
                i32 toolbarIndex;
            };

            struct SidebarItem {
                std::string icon;
                DrawCallback callback;
                EnabledCallback enabledCallback;
            };

            struct TitleBarButton {
                std::string icon;
                ImGuiCustomCol color;
                UnlocalizedString unlocalizedTooltip;
                ClickCallback callback;
            };

            struct WelcomeScreenQuickSettingsToggle {
                std::string onIcon, offIcon;
                UnlocalizedString unlocalizedTooltip;
                ToggleCallback callback;
                mutable bool state;
            };

            constexpr static auto SeparatorValue = "$SEPARATOR$";
            constexpr static auto SubMenuValue = "$SUBMENU$";
            constexpr static auto TaskBarMenuValue = "$TASKBAR$";

            const std::multimap<u32, MainMenuItem>& getMainMenuItems();

            const std::multimap<u32, MenuItem>& getMenuItems();
            const std::vector<MenuItem*>& getToolbarMenuItems();
            std::multimap<u32, MenuItem>& getMenuItemsMutable();

            const std::vector<DrawCallback>& getWelcomeScreenEntries();
            const std::vector<DrawCallback>& getFooterItems();
            const std::vector<DrawCallback>& getToolbarItems();
            const std::vector<SidebarItem>& getSidebarItems();
            const std::vector<TitleBarButton>& getTitlebarButtons();
            const std::vector<WelcomeScreenQuickSettingsToggle>& getWelcomeScreenQuickSettingsToggles();

        }

        /**
         * @brief Adds a new top-level main menu entry
         * @param unlocalizedName The unlocalized name of the entry
         * @param priority The priority of the entry. Lower values are displayed first
         */
        void registerMainMenuItem(const UnlocalizedString &unlocalizedName, u32 priority);

        /**
         * @brief Adds a new main menu entry
         * @param unlocalizedMainMenuNames The unlocalized names of the main menu entries
         * @param icon The icon to use for the entry
         * @param priority The priority of the entry. Lower values are displayed first
         * @param shortcut The shortcut to use for the entry
         * @param function The function to call when the entry is clicked
         * @param enabledCallback The function to call to determine if the entry is enabled
         * @param view The view to use for the entry. If nullptr, the shortcut will work globally
         */
        void addMenuItem(
            const std::vector<UnlocalizedString> &unlocalizedMainMenuNames,
            const Icon &icon,
            u32 priority,
            const Shortcut &shortcut,
            const impl::MenuCallback &function,
            const impl::EnabledCallback& enabledCallback, View *view
        );

        /**
         * @brief Adds a new main menu entry
         * @param unlocalizedMainMenuNames The unlocalized names of the main menu entries
         * @param icon The icon to use for the entry
         * @param priority The priority of the entry. Lower values are displayed first
         * @param shortcut The shortcut to use for the entry
         * @param function The function to call when the entry is clicked
         * @param enabledCallback The function to call to determine if the entry is enabled
         * @param selectedCallback The function to call to determine if the entry is selected
         * @param view The view to use for the entry. If nullptr, the shortcut will work globally
         */
        void addMenuItem(
            const std::vector<UnlocalizedString> &unlocalizedMainMenuNames,
            const Icon &icon,
            u32 priority,
            Shortcut shortcut,
            const impl::MenuCallback &function,
            const impl::EnabledCallback& enabledCallback = []{ return true; },
            const impl::SelectedCallback &selectedCallback = []{ return false; },
            View *view = nullptr
        );

        /**
         * @brief Adds a new main menu entry
         * @param unlocalizedMainMenuNames The unlocalized names of the main menu entries
         * @param priority The priority of the entry. Lower values are displayed first
         * @param shortcut The shortcut to use for the entry
         * @param function The function to call when the entry is clicked
         * @param enabledCallback The function to call to determine if the entry is enabled
         * @param selectedCallback The function to call to determine if the entry is selected
         * @param view The view to use for the entry. If nullptr, the shortcut will work globally
         */
        void addMenuItem(
            const std::vector<UnlocalizedString> &unlocalizedMainMenuNames,
            u32 priority,
            const Shortcut &shortcut,
            const impl::MenuCallback &function,
            const impl::EnabledCallback& enabledCallback = []{ return true; },
            const impl::SelectedCallback &selectedCallback = []{ return false; },
            View *view = nullptr
        );

        /**
         * @brief Adds a new main menu sub-menu entry
         * @param unlocalizedMainMenuNames The unlocalized names of the main menu entries
         * @param priority The priority of the entry. Lower values are displayed first
         * @param function The function to call when the entry is clicked
         * @param enabledCallback The function to call to determine if the entry is enabled
         * @param view The view to use for the entry. If nullptr, the item will always be visible
         * @param showOnWelcomeScreen If this entry should be shown on the welcome screen
         */
        void addMenuItemSubMenu(
            std::vector<UnlocalizedString> unlocalizedMainMenuNames,
            u32 priority,
            const impl::MenuCallback &function,
            const impl::EnabledCallback& enabledCallback = []{ return true; },
            View *view = nullptr,
            bool showOnWelcomeScreen = false
        );

        /**
         * @brief Adds a new main menu sub-menu entry
         * @param unlocalizedMainMenuNames The unlocalized names of the main menu entries
         * @param icon The icon to use for the entry
         * @param priority The priority of the entry. Lower values are displayed first
         * @param function The function to call when the entry is clicked
         * @param enabledCallback The function to call to determine if the entry is enabled
         * @param view The view to use for the entry. If nullptr, the item will always be visible
         * @param showOnWelcomeScreen If this entry should be shown on the welcome screen
         */
        void addMenuItemSubMenu(
            std::vector<UnlocalizedString> unlocalizedMainMenuNames,
            const char *icon,
            u32 priority,
            const impl::MenuCallback &function,
            const impl::EnabledCallback& enabledCallback = []{ return true; },
            View *view = nullptr,
            bool showOnWelcomeScreen = false
        );


        /**
         * @brief Adds a new main menu separator
         * @param unlocalizedMainMenuNames The unlocalized names of the main menu entries
         * @param priority The priority of the entry. Lower values are displayed first
         * @param view The view to use for the entry. If nullptr, the item will always be visible
         */
        void addMenuItemSeparator(std::vector<UnlocalizedString> unlocalizedMainMenuNames, u32 priority, View *view = nullptr);

        /**
         * @brief Adds a new main menu entry
         * @param unlocalizedMainMenuNames The unlocalized names of the main menu entries
         * @param priority The priority of the entry. Lower values are displayed first
         * @param function The function to call when the entry is clicked
         * @param enabledCallback The function to call to determine if the entry is enabled
         */
        void addTaskBarMenuItem(
            std::vector<UnlocalizedString> unlocalizedMainMenuNames,
            u32 priority,
            const impl::MenuCallback &function,
            const impl::EnabledCallback& enabledCallback
        );

        /**
         * @brief Adds a new welcome screen entry
         * @param function The function to call to draw the entry
         */
        void addWelcomeScreenEntry(const impl::DrawCallback &function);

        /**
         * @brief Adds a new footer item
         * @param function The function to call to draw the item
         */
        void addFooterItem(const impl::DrawCallback &function);

        /**
         * @brief Adds a new toolbar item
         * @param function The function to call to draw the item
         */
        void addToolbarItem(const impl::DrawCallback &function);

        /**
         * @brief Adds a menu item to the toolbar
         * @param unlocalizedNames Unlocalized name of the menu item
         * @param color Color of the toolbar icon
         */
        void addMenuItemToToolbar(const std::vector<UnlocalizedString> &unlocalizedNames, ImGuiCustomCol color);

        /**
         * @brief Reconstructs the toolbar items list after they have been modified
         */
        void updateToolbarItems();

        /**
         * @brief Adds a new sidebar item
         * @param icon The icon to use for the item
         * @param function The function to call to draw the item
         * @param enabledCallback The function
         */
        void addSidebarItem(
            const std::string &icon,
            const impl::DrawCallback &function,
            const impl::EnabledCallback &enabledCallback = []{ return true; }
        );

        /**
         * @brief Adds a new title bar button
         * @param icon The icon to use for the button
         * @param color The color of the icon
         * @param unlocalizedTooltip The unlocalized tooltip to use for the button
         * @param function The function to call when the button is clicked
         */
        void addTitleBarButton(
            const std::string &icon,
            ImGuiCustomCol color,
            const UnlocalizedString &unlocalizedTooltip,
            const impl::ClickCallback &function
        );

        /**
         * @brief Adds a new welcome screen quick settings toggle
         * @param icon The icon to use for the button
         * @param unlocalizedTooltip The unlocalized tooltip to use for the button
         * @param defaultState The default state of the toggle
         * @param function The function to call when the button is clicked
         */
        void addWelcomeScreenQuickSettingsToggle(
            const std::string &icon,
            const UnlocalizedString &unlocalizedTooltip,
            bool defaultState,
            const impl::ToggleCallback &function
        );

        /**
         * @brief Adds a new welcome screen quick settings toggle
         * @param onIcon The icon to use for the button when it's on
         * @param offIcon The icon to use for the button when it's off
         * @param unlocalizedTooltip The unlocalized tooltip to use for the button
         * @param defaultState The default state of the toggle
         * @param function The function to call when the button is clicked
         */
        void addWelcomeScreenQuickSettingsToggle(
            const std::string &onIcon,
            const std::string &offIcon,
            const UnlocalizedString &unlocalizedTooltip,
            bool defaultState,
            const impl::ToggleCallback &function
        );

    }

}