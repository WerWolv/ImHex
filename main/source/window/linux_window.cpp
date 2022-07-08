#include "window.hpp"

#if defined(OS_LINUX)

    #include <hex/api/imhex_api.hpp>
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
        bool themeFollowSystem = ImHexApi::System::usesSystemThemeDetection();
        EventManager::subscribe<EventOSThemeChanged>(this, [themeFollowSystem] {
            if (!themeFollowSystem) return;

            std::array<char, 128> buffer = { 0 };
            std::string result;

            // TODO: Make a cleaner implementation
            FILE *pipe = popen("dbus-send --session --print-reply=literal --reply-timeout=1000 --dest=org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop org.freedesktop.portal.Settings.Read string:'org.freedesktop.appearance' string:'color-scheme' 2>&1", "r");
            if (pipe == nullptr) return;

            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
                result += buffer.data();

            auto exitCode = WEXITSTATUS(pclose(pipe));
            if (exitCode != 0) return;

            result.erase(0, result.size()-2);
            int themeId;
            try{
                themeId = std::stoi(result);
            }catch(std::invalid_argument& e){
                log::error("Invalid theme id : {}", result);
                return;
            }
            EventManager::post<RequestChangeTheme>(themeId);

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