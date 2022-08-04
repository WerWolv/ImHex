#include "window.hpp"

#if defined(OS_MACOS)

    #include <hex/api/imhex_api.hpp>
    #include <hex/api/content_registry.hpp>
    #include <hex/api/event.hpp>

    #include <hex/helpers/utils_macos.hpp>
    #include <hex/helpers/logger.hpp>

    #include <nlohmann/json.hpp>
    #include <unistd.h>

    #include <imgui_impl_glfw.h>

namespace hex {

    void Window::initNative() {
        if (!isatty(STDOUT_FILENO)) {
            log::redirectToFile();
        }
    }

    void Window::setupNativeWindow() {
        ImGui_ImplGlfw_SetBorderlessWindowMode(false);

        bool themeFollowSystem = ImHexApi::System::usesSystemThemeDetection();
        EventManager::subscribe<EventOSThemeChanged>(this, [themeFollowSystem] {
            if (!themeFollowSystem) return;

            if (!isMacosSystemDarkModeEnabled())
                EventManager::post<RequestChangeTheme>(2);
            else
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