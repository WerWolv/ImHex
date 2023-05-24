#include "window.hpp"

#if defined(OS_LINUX)

    #include <hex/api/imhex_api.hpp>
    #include <hex/api/content_registry.hpp>
    #include <hex/api/event.hpp>

    #include <hex/helpers/utils.hpp>
    #include <hex/helpers/logger.hpp>

    #include <wolv/utils/core.hpp>

    #include <nlohmann/json.hpp>
    #include <cstdio>
    #include <sys/wait.h>
    #include <unistd.h>

    #include <imgui_impl_glfw.h>
    #include <string.h>

namespace hex {

    bool isFileInPath(std::fs::path filename){
        auto path_var = std::string(getenv("PATH"));
        for(auto dir : std::views::split(path_var, ':')){
            if(std::fs::exists(std::fs::path(std::string_view(dir)) / filename)) {
                return true;
            }
        }
        return false;
    }

    void executeCmd(std::vector<std::string> strVector) {
        std::vector<char*> cVector;
        for (std::string& str : strVector) {
            cVector.push_back(const_cast<char*>(str.c_str()));
        }
        cVector.push_back(nullptr);
        
        if (fork() == 0) {
            execvp(cVector[0], &cVector[0]);
            log::error("execvp() failed: {}", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    void nativeErrorMessage(const std::string &message) {
        if (isFileInPath("zenity")) {
            executeCmd({"zenity", "--error", "--text", message});
        } else if(isFileInPath("notify-send")) {
            executeCmd({"notify-send", "-i", "script-error", "Error", message});
        } // hopefully one of these commands is installed
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
            log::redirectToFile();
        }
    }

    void Window::setupNativeWindow() {
        ImGui_ImplGlfw_SetBorderlessWindowMode(false);

        bool themeFollowSystem = ImHexApi::System::usesSystemThemeDetection();
        EventManager::subscribe<EventOSThemeChanged>(this, [themeFollowSystem] {
            if (!themeFollowSystem) return;

            std::array<char, 128> buffer = { 0 };
            std::string result;

            // Ask GNOME for the current theme
            // TODO: In the future maybe support more DEs instead of just GNOME
            FILE *pipe = popen("gsettings get org.gnome.desktop.interface gtk-theme 2>&1", "r");
            if (pipe == nullptr) return;

            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
                result += buffer.data();

            auto exitCode = WEXITSTATUS(pclose(pipe));
            if (exitCode != 0) return;

            EventManager::post<RequestChangeTheme>(hex::containsIgnoreCase(result, "light") ? "Light" : "Dark");
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