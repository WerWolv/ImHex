#include "window.hpp"

#if defined(OS_MACOS)

    #include <hex/api/imhex_api.hpp>
    #include <hex/api/event_manager.hpp>

    #include <hex/helpers/utils_macos.hpp>
    #include <hex/helpers/logger.hpp>

    #include <cstdio>
    #include <unistd.h>

    #include <imgui_impl_glfw.h>

namespace hex {

    void nativeErrorMessage(const std::string &message) {
        log::fatal(message);
        errorMessageMacos(message.c_str());
    }

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
            log::impl::redirectToFile();
        }
    }

    void Window::setupNativeWindow() {
        bool themeFollowSystem = ImHexApi::System::usesSystemThemeDetection();
        EventOSThemeChanged::subscribe(this, [themeFollowSystem] {
            if (!themeFollowSystem) return;

            if (!isMacosSystemDarkModeEnabled())
                RequestChangeTheme::post("Light");
            else
                RequestChangeTheme::post("Dark");
        });

        if (themeFollowSystem)
            EventOSThemeChanged::post();

        // Register file drop callback
        glfwSetDropCallback(m_window, [](GLFWwindow *, int count, const char **paths) {
            for (int i = 0; i < count; i++) {
                EventFileDropped::post(reinterpret_cast<const char8_t *>(paths[i]));
            }
        });

        setupMacosWindowStyle(m_window);
    }

    void Window::beginNativeWindowFrame() {

    }

    void Window::endNativeWindowFrame() {

    }

}

#endif