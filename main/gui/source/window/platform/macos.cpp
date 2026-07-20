#include "window.hpp"

#if defined(OS_MACOS)

    #include <hex/api/project_file_manager.hpp>
    #include <hex/api/imhex_api/system.hpp>
    #include <hex/api/imhex_api/provider.hpp>
    #include <hex/api/events/events_gui.hpp>
    #include <hex/api/events/requests_gui.hpp>
    #include <hex/api/events/events_interaction.hpp>
    #include <hex/api/task_manager.hpp>

    #include <hex/helpers/utils.hpp>
    #include <hex/helpers/utils_macos.hpp>
    #include <hex/helpers/logger.hpp>
    #include <hex/helpers/default_paths.hpp>

    #include <unistd.h>

namespace hex {

    void Window::configureWindowBackend(ImHexApi::System::WindowBackend::Config &config) {
        config.glMajor = 3;
        config.glMinor = 2;
        config.coreProfile = true;
        config.highPixelDensity = true;
        config.transparent = true;
        config.decorated = true;
    }

    void Window::initNative() {
        log::impl::enableColorPrinting();

        // Add plugin library folders to dll search path
        for (const auto &path : paths::Libraries.read())  {
            if (std::fs::exists(path))
                setenv("LD_LIBRARY_PATH", fmt::format("{};{}", hex::getEnvironmentVariable("LD_LIBRARY_PATH").value_or(""), path.string().c_str()).c_str(), true);
        }

        // Redirect stdout to log file if we're not running in a terminal
        if (!isatty(STDOUT_FILENO)) {
            log::impl::redirectToFile();
        }

        enumerateFontsMacos();
    }

    void Window::setupNativeWindow() {
        macosSetupDockMenu();

        bool themeFollowSystem = ImHexApi::System::usesSystemThemeDetection();
        EventOSThemeChanged::subscribe(this, [themeFollowSystem] {
            if (!themeFollowSystem) return;

            if (!isMacosSystemDarkModeEnabled())
                RequestChangeTheme::post("Light");
            else
                RequestChangeTheme::post("Dark");
        });

        EventProviderDirtied::subscribe([this](prv::Provider *) {
            TaskManager::doLater([this] {
                macosMarkContentEdited(ImHexApi::System::impl::getNativeWindow().handle);
            });
        });

        ProjectFile::registerHandler({
            .basePath = "",
            .required = true,
            .load = [](const std::fs::path &, Tar &) {
                return true;
            },
            .store = [this](const std::fs::path &, Tar &) {
                TaskManager::doLater([this] {
                    macosMarkContentEdited(ImHexApi::System::impl::getNativeWindow().handle, false);
                });

                return true;
            }
        });

        if (themeFollowSystem)
            EventOSThemeChanged::post();

        setupMacosWindowStyle(ImHexApi::System::impl::getNativeWindow().handle, ImHexApi::System::isBorderlessWindowModeEnabled());
    }

    void Window::beginNativeWindowFrame() {
        if (!ImHexApi::Provider::isValid())
            macosMarkContentEdited(ImHexApi::System::impl::getNativeWindow().handle, false);
    }

    void Window::endNativeWindowFrame() {

    }

}

#endif
