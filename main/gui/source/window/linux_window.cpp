#include "window.hpp"

#if defined(OS_LINUX)

    #include <hex/api/imhex_api.hpp>
    #include <hex/api/content_registry.hpp>
    #include <hex/api/event_manager.hpp>

    #include <hex/helpers/utils.hpp>
    #include <hex/helpers/utils_linux.hpp>
    #include <hex/helpers/logger.hpp>
    #include <hex/helpers/default_paths.hpp>

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

    void enumerateFonts() {
        const std::array FontDirectories = {
            "/usr/share/fonts",
            "/usr/local/share/fonts",
            "~/.fonts",
            "~/.local/share/fonts"
        };

        for (const auto &directory : FontDirectories) {
            if (!std::fs::exists(directory))
                continue;

            for (const auto &entry : std::fs::recursive_directory_iterator(directory)) {
                if (!entry.exists())
                    continue;
                if (!entry.is_regular_file())
                    continue;
                if (entry.path().extension() != ".ttf" && entry.path().extension() != ".otf")
                    continue;

                registerFont(entry.path().stem().c_str(), entry.path().c_str());
            }
        }
    }

    void Window::configureGLFW() {
        glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_DECORATED, ImHexApi::System::isBorderlessWindowModeEnabled() ? GL_FALSE : GL_TRUE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

        #if defined(GLFW_WAYLAND_APP_ID)
                glfwWindowHintString(GLFW_WAYLAND_APP_ID, "imhex");
        #endif
    }

    void Window::initNative() {
        log::impl::enableColorPrinting();

        // Add plugin library folders to dll search path
        for (const auto &path : paths::Libraries.read())  {
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

        enumerateFonts();
    }

    void Window::setupNativeWindow() {
        bool themeFollowSystem = ImHexApi::System::usesSystemThemeDetection();
        EventOSThemeChanged::subscribe(this, [themeFollowSystem] {
            if (!themeFollowSystem) return;

            std::array<char, 128> buffer = { 0 };
            std::string result;

            // Ask dbus for the current theme. 1 for Dark, 2 for Light, 0 for default (Dark for ImHex)
            // https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Settings.html
            FILE *pipe = popen("dbus-send --session --print-reply --dest=org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop org.freedesktop.portal.Settings.Read string:'org.freedesktop.appearance' string:'color-scheme' 2>&1", "r");
            if (pipe == nullptr) return;

            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
                result += buffer.data();

            auto exitCode = WEXITSTATUS(pclose(pipe));
            if (exitCode != 0) return;

            RequestChangeTheme::post(hex::containsIgnoreCase(result, "uint32 2") ? "Light" : "Dark");
        });

        // Register file drop callback
        glfwSetDropCallback(m_window, [](GLFWwindow *, int count, const char **paths) {
            for (int i = 0; i < count; i++) {
                EventFileDropped::post(reinterpret_cast<const char8_t *>(paths[i]));
            }
        });

        glfwSetWindowRefreshCallback(m_window, [](GLFWwindow *window) {
            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->fullFrame();
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