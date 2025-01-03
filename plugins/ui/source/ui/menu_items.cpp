#include <ui/menu_items.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <hex/api/shortcut_manager.hpp>

#if defined(OS_MACOS)
    extern "C" {
        void macosMenuBarInit(void);

        bool macosBeginMainMenuBar(void);
        void macosEndMainMenuBar(void);
        void macosClearMenu(void);

        bool macosBeginMenu(const char* label, bool enabled);
        void macosEndMenu(void);

        bool macosMenuItem(const char* label, KeyEquivalent keyEquivalent, bool selected, bool enabled);
        bool macosMenuItemSelect(const char* label, KeyEquivalent keyEquivalent, bool* p_selected, bool enabled);

        void macosSeparator(void);
    }
#endif

namespace hex::menu {

        static bool s_useNativeMenuBar = false;
        void enableNativeMenuBar(bool enabled) {
            #if !defined (OS_MACOS)
                return;
            #endif

            s_useNativeMenuBar = enabled;

            if (!enabled) {
                macosClearMenu();
            }
        }

        bool isNativeMenuBarUsed() {
            return s_useNativeMenuBar;
        }

        static bool s_initialized = false;
        bool beginMainMenuBar() {
            #if defined(OS_MACOS)
                if (s_useNativeMenuBar) {
                    if (!s_initialized) {
                        macosMenuBarInit();
                        s_initialized = true;
                    }

                    return macosBeginMainMenuBar();
                }
            #endif

            return ImGui::BeginMainMenuBar();
        }

        void endMainMenuBar() {
            #if defined(OS_MACOS)
                if (s_useNativeMenuBar)
                    return macosEndMainMenuBar();
            #endif

            return ImGui::EndMainMenuBar();
        }

        bool beginMenu(const char *label, bool enabled) {
            #if defined(OS_MACOS)
                if (s_useNativeMenuBar)
                    return macosBeginMenu(label, enabled);
            #endif

            return ImGui::BeginMenu(label, enabled);
        }

        bool beginMenuEx(const char *label, const char *icon, bool enabled) {
            #if defined(OS_MACOS)
                if (s_useNativeMenuBar)
                    return macosBeginMenu(label, enabled);
            #endif

            return ImGui::BeginMenuEx(label, icon, enabled);
        }

        void endMenu() {
            #if defined(OS_MACOS)
                if (s_useNativeMenuBar)
                    return macosEndMenu();
            #endif

            return ImGui::EndMenu();
        }


        bool menuItem(const char *label, const Shortcut &shortcut, bool selected, bool enabled) {
            #if defined(OS_MACOS)
                if (s_useNativeMenuBar)
                    return macosMenuItem(label, shortcut.toKeyEquivalent(), selected, enabled);
            #endif

            return ImGui::MenuItem(label, shortcut.toString().c_str(), selected, enabled);
        }

        bool menuItem(const char *label, const Shortcut &shortcut, bool *selected, bool enabled) {
            #if defined(OS_MACOS)
                if (s_useNativeMenuBar)
                    return macosMenuItemSelect(label, shortcut.toKeyEquivalent(), selected, enabled);
            #endif

            return ImGui::MenuItem(label, shortcut.toString().c_str(), selected, enabled);
        }

        bool menuItemEx(const char *label, const char *icon, const Shortcut &shortcut, bool selected, bool enabled) {
            #if defined(OS_MACOS)
                if (s_useNativeMenuBar)
                    return macosMenuItem(label, shortcut.toKeyEquivalent(), selected, enabled);
            #endif

            return ImGui::MenuItemEx(label, icon, shortcut.toString().c_str(), selected, enabled);
        }

        bool menuItemEx(const char *label, const char *icon, const Shortcut &shortcut, bool *selected, bool enabled) {
            #if defined(OS_MACOS)
                if (s_useNativeMenuBar)
                    return macosMenuItemSelect(label, shortcut.toKeyEquivalent(), selected, enabled);
            #endif

            return ImGui::MenuItemEx(label, icon, shortcut.toString().c_str(), selected, enabled);
        }


        void menuSeparator() {
            #if defined(OS_MACOS)
                if (s_useNativeMenuBar)
                    return macosSeparator();
            #endif

            return ImGui::Separator();
        }

}