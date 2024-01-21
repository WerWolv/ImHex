#include "window.hpp"

#if defined(OS_LINUX)

    #include <hex/api/imhex_api.hpp>
    #include <hex/api/content_registry.hpp>
    #include <hex/api/event_manager.hpp>

    #include <hex/helpers/utils.hpp>
    #include <hex/helpers/utils_linux.hpp>
    #include <hex/helpers/logger.hpp>

    #include <wolv/utils/core.hpp>

    #include <nlohmann/json.hpp>
    #include <cstdio>
    #include <sys/wait.h>
    #include <unistd.h>

    #include <imgui_impl_glfw.h>
    #include <string.h>
    #include <ranges>

namespace hex {

    bool isFileInPath(const std::fs::path &filename) {
        auto optPathVar = hex::getEnvironmentVariable("PATH");
        if (!optPathVar.has_value()) {
            log::error("Could not find variable named PATH");
            return false;
        }

        for (auto dir : std::views::split(optPathVar.value(), ':')) {
            if (std::fs::exists(std::fs::path(std::string_view(dir)) / filename)) {
                return true;
            }
        }
        return false;
    }

    void nativeErrorMessage(const std::string &message) {
        log::fatal(message);
        if (isFileInPath("zenity")) {
            executeCmd({"zenity", "--error", "--text", message});
        } else if (isFileInPath("notify-send")) {
            executeCmd({"notify-send", "-i", "script-error", "Error", message});
        } // Hopefully one of these commands is installed
    }

    void Window::initNative() {
        // Add plugin library folders to dll search path
        for (const auto &path : hex::fs::getDefaultPaths(fs::ImHexPath::Libraries))  {
            if (std::fs::exists(path))
                setenv("LD_LIBRARY_PATH", hex::format("{};{}", hex::getEnvironmentVariable("LD_LIBRARY_PATH").value_or(""), path.string().c_str()).c_str(), true);
        }

        // Various libraries sadly directly print to stderr with no way to disable it
        // We redirect stderr to /dev/null to prevent this
        wolv::util::unused(freopen("/dev/null", "w", stderr));
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

            std::array<char, 128> buffer = { 0 };
            std::string result;

            // Ask dbus for the current theme. 0 for Light, 1 for Dark
            FILE *pipe = popen("dbus-send --session --print-reply --dest=org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop org.freedesktop.portal.Settings.Read string:'org.freedesktop.appearance' string:'color-scheme' 2>&1", "r");
            if (pipe == nullptr) return;

            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
                result += buffer.data();

            auto exitCode = WEXITSTATUS(pclose(pipe));
            if (exitCode != 0) return;

            RequestChangeTheme::post(hex::containsIgnoreCase(result, "uint32 1") ? "Light" : "Dark");
        });

        // Register file drop callback
        glfwSetDropCallback(m_window, [](GLFWwindow *, int count, const char **paths) {
            for (int i = 0; i < count; i++) {
                EventFileDropped::post(reinterpret_cast<const char8_t *>(paths[i]));
            }
        });

        if (themeFollowSystem)
            EventOSThemeChanged::post();
    }

    void Window::beginNativeWindowFrame() {
    }

    void Window::endNativeWindowFrame() {
    }

}

#endif