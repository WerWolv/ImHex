#include "window.hpp"

#if defined(OS_LINUX)

    #include <hex/api/imhex_api/system.hpp>
    #include <hex/api/events/events_gui.hpp>
    #include <hex/api/events/events_interaction.hpp>
    #include <hex/api/events/requests_gui.hpp>

    #include <hex/helpers/utils.hpp>
    #include <hex/helpers/utils_linux.hpp>
    #include <hex/helpers/logger.hpp>
    #include <hex/helpers/default_paths.hpp>

    #include <wolv/utils/core.hpp>

    #include <nlohmann/json.hpp>
    #include <sys/wait.h>
    #include <unistd.h>

    #include <ranges>

    #if defined(IMHEX_HAS_FONTCONFIG)
        #include <fontconfig/fontconfig.h>
    #endif

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

    #if defined(IMHEX_HAS_FONTCONFIG)
        static bool enumerateFontConfig() {
            if (!FcInit())
                return false;

            ON_SCOPE_EXIT { FcFini(); };

            auto fonts = FcConfigGetFonts(nullptr, FcSetSystem);
            if (fonts == nullptr)
                return false;

            for (int i = 0; i < fonts->nfont; ++i) {
                auto font = fonts->fonts[i];
                FcChar8 *file, *fullName;

                if (FcPatternGetString(font, FC_FILE, 0, &file) != FcResultMatch) {
                    continue;
                }

                if (FcPatternGetString(font, FC_FULLNAME, 0, &fullName) != FcResultMatch
                    && FcPatternGetString(font, FC_FAMILY, 0, &fullName) != FcResultMatch) {
                    continue;
                }

                registerFont(reinterpret_cast<const char *>(fullName), reinterpret_cast<const char *>(file));
            }

            return true;
        }
    #endif

    void enumerateFonts() {
        #if defined(IMHEX_HAS_FONTCONFIG)
            if (enumerateFontConfig())
                return;
        #endif

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

    void Window::configureWindowBackend(ImHexApi::System::WindowBackend::Config &config) {
        config.glMajor = 3;
        config.glMinor = 1;
        config.decorated = !ImHexApi::System::isBorderlessWindowModeEnabled();
        config.transparent = true;
        config.highPixelDensity = true;
        config.scaleToMonitor = true;
        config.applicationId = "imhex";
    }

    void Window::initNative() {
        log::impl::enableColorPrinting();

        // Add plugin library folders to dll search path
        for (const auto &path : paths::Libraries.read())  {
            if (std::fs::exists(path))

            setenv("LD_LIBRARY_PATH", fmt::format("{}:{}", hex::getEnvironmentVariable("LD_LIBRARY_PATH").value_or(""), path.string().c_str()).c_str(), true);
        }

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

            while (fgets(buffer.data(), buffer.size() - 1, pipe) != nullptr)
                result += buffer.data();

            auto exitCode = WEXITSTATUS(pclose(pipe));
            if (exitCode != 0) return;

            RequestChangeTheme::post(hex::containsIgnoreCase(result, "uint32 2") ? "Light" : "Dark");
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
