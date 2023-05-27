#include "window.hpp"

#if defined(OS_MACOS)

    #include <hex/api/imhex_api.hpp>
    #include <hex/api/content_registry.hpp>
    #include <hex/api/event.hpp>

    #include <hex/helpers/utils_macos.hpp>
    #include <hex/helpers/logger.hpp>

    #include <nlohmann/json.hpp>
    #include <cstdio>
    #include <unistd.h>

    #include <imgui_impl_glfw.h>

namespace hex {

    void Window::initNative() {
        // Add plugin library folders to dll search path
        for (const auto &path : hex::fs::getDefaultPaths(fs::ImHexPath::Libraries))  {
            if (std::fs::exists(path))
                setenv("LD_LIBRARY_PATH", hex::format("{};{}", hex::getEnvironmentVariable("LD_LIBRARY_PATH").value_or(""), path.string().c_str()).c_str(), true);
        }

        // Various libraries sadly directly print to stderr with no way to disable it
        // We redirect stderr to /dev/null to prevent this
        freopen("/dev/null", "w", stderr);
        setvbuf(stderr, nullptr, _IONBF, 0);

        // Redirect stdout to log file if we're not running in a terminal
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
                EventManager::post<RequestChangeTheme>("Light");
            else
                EventManager::post<RequestChangeTheme>("Dark");
        });

        if (themeFollowSystem)
            EventManager::post<EventOSThemeChanged>();
    }

    void Window::beginNativeWindowFrame() {
    }

    void Window::endNativeWindowFrame() {
    }

}

#endif