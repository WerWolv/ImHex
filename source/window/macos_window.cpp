#include "window.hpp"

#if defined(OS_MACOS)

    #include <nlohmann/json.hpp>

    namespace hex {

        void Window::initNative() {

        }

        void Window::setupNativeWindow() {
            bool themeFollowSystem = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.color") == 0;
            EventManager::subscribe<EventOSThemeChanged>(this, [themeFollowSystem]{
                if (!themeFollowSystem) return;

                // TODO: Implement this when MacOS build is working again
                EventManager::post<RequestChangeTheme>(1);
            });

            if (themeFollowSystem)
                EventManager::post<EventOSThemeChanged>();
        }

        void Window::updateNativeWindow() {

        }

        void Window::drawTitleBar() {

        }

    }

#endif