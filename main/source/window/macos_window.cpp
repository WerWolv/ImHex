#include "window.hpp"

#if defined(OS_MACOS)

    #include <hex/helpers/logger.hpp>

    #include <nlohmann/json.hpp>
    #include <unistd.h>

    namespace hex {

        void Window::initNative() {
            if (!isatty(STDOUT_FILENO)) {
                log::redirectToFile();
            }
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

        void Window::beginNativeWindowFrame() {

        }

        void Window::endNativeWindowFrame() {

        }

        void Window::drawTitleBar() {

        }

    }

#endif