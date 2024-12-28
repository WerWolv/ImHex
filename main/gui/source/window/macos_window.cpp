#include "window.hpp"

#if defined(OS_MACOS)

    #include <hex/api/project_file_manager.hpp>
    #include <hex/api/imhex_api.hpp>
    #include <hex/api/event_manager.hpp>
    #include <hex/api/task_manager.hpp>

    #include <hex/helpers/utils_macos.hpp>
    #include <hex/helpers/logger.hpp>
    #include <hex/helpers/default_paths.hpp>

    #include <unistd.h>

    #include <imgui_impl_glfw.h>

namespace hex {

    void nativeErrorMessage(const std::string &message) {
        log::fatal(message);
        errorMessageMacos(message.c_str());
    }

    void Window::configureGLFW() {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, GLFW_TRUE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    }

    void Window::initNative() {
        log::impl::enableColorPrinting();

        // Add plugin library folders to dll search path
        for (const auto &path : paths::Libraries.read())  {
            if (std::fs::exists(path))
                setenv("LD_LIBRARY_PATH", hex::format("{};{}", hex::getEnvironmentVariable("LD_LIBRARY_PATH").value_or(""), path.string().c_str()).c_str(), true);
        }

        // Redirect stdout to log file if we're not running in a terminal
        if (!isatty(STDOUT_FILENO)) {
            log::impl::redirectToFile();
        }

        enumerateFontsMacos();
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

        EventProviderDirtied::subscribe([this](prv::Provider *) {
            TaskManager::doLater([this] {
                macosMarkContentEdited(m_window);
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
                    macosMarkContentEdited(m_window, false);
                });

                return true;
            }
        });

        if (themeFollowSystem)
            EventOSThemeChanged::post();

        // Register file drop callback
        glfwSetDropCallback(m_window, [](GLFWwindow *, int count, const char **paths) {
            for (int i = 0; i < count; i++) {
                EventFileDropped::post(reinterpret_cast<const char8_t *>(paths[i]));
            }
        });

        setupMacosWindowStyle(m_window, ImHexApi::System::isBorderlessWindowModeEnabled());

        glfwSetWindowRefreshCallback(m_window, [](GLFWwindow *window) {
            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->fullFrame();
        });
    }

    void Window::beginNativeWindowFrame() {

    }

    void Window::endNativeWindowFrame() {

    }

}

#endif