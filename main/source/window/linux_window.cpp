#include "window.hpp"

#if defined(OS_LINUX)

    #include <hex/api/content_registry.hpp>
    #include <hex/api/event.hpp>

    #include <hex/helpers/utils.hpp>
    #include <hex/helpers/logger.hpp>

    #include <nlohmann/json.hpp>
    #include <sys/wait.h>
    #include <unistd.h>

namespace hex {

    void Window::initNative() {
        if (!isatty(STDOUT_FILENO)) {
            log::redirectToFile();
        }
    }

    void Window::setupNativeWindow() {
        bool themeFollowSystem = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.color") == 0;
        EventManager::subscribe<EventOSThemeChanged>(this, [themeFollowSystem] {
            if (!themeFollowSystem) return;

            std::array<char, 128> buffer = { 0 };
            std::string result;

            // TODO: In the future maybe support more DEs instead of just GNOME
            FILE *pipe = popen("gsettings get org.gnome.desktop.interface gtk-theme 2>&1", "r");
            if (pipe == nullptr) return;

            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
                result += buffer.data();

            auto exitCode = WEXITSTATUS(pclose(pipe));
            if (exitCode != 0) return;

            EventManager::post<RequestChangeTheme>(hex::containsIgnoreCase(result, "light") ? 2 : 1);
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