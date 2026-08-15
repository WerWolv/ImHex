#include "window.hpp"

#if defined(OS_MACOS)

    #include <hex/api/content_registry/settings.hpp>
    #include <hex/api/imhex_api/system.hpp>
    #include <hex/api/imhex_api/provider.hpp>
    #include <hex/api/events/events_gui.hpp>
    #include <hex/api/events/events_lifecycle.hpp>
    #include <hex/api/events/requests_gui.hpp>
    #include <hex/api/events/events_interaction.hpp>
    #include <hex/api/task_manager.hpp>

    #include <hex/helpers/utils.hpp>
    #include <hex/helpers/utils_macos.hpp>
    #include <hex/helpers/logger.hpp>
    #include <hex/helpers/default_paths.hpp>

    #include <unistd.h>

    #include <cmath>
    #include <numeric>
    #include <optional>
    #include <utility>

    #include <GLFW/glfw3.h>
    #include <imgui_impl_glfw.h>

namespace hex {

    namespace {

        std::optional<std::pair<float, float>> s_pendingDpiChange;

        void scheduleDpiChange(float oldScale, float newScale) {
            if (s_pendingDpiChange.has_value()) {
                s_pendingDpiChange->second = newScale;
                return;
            }

            s_pendingDpiChange = std::pair(oldScale, newScale);
            TaskManager::doLater([] {
                if (!s_pendingDpiChange.has_value())
                    return;

                const auto [firstScale, lastScale] = s_pendingDpiChange.value();
                s_pendingDpiChange.reset();
                EventDPIChanged::post(firstScale, lastScale);
            });
        }

        void updateContentScale(GLFWwindow *window, float xScale, float yScale) {
            auto newScale = std::midpoint(xScale, yScale);
            const auto backingScale = ::getWindowBackingScaleFactor(window);
            if (backingScale > 0.0F)
                newScale /= backingScale;

            if (!std::isfinite(newScale) || newScale <= 0.0F)
                return;

            const auto oldScale = ImHexApi::System::getNativeScale();
            if (std::abs(oldScale - newScale) < 0.001F)
                return;

            ImHexApi::System::impl::setNativeScale(newScale);

            const auto interfaceScale = ContentRegistry::Settings::read<float>(
                "hex.builtin.setting.interface", "hex.builtin.setting.interface.scaling_factor", 0.0F
            );
            if (interfaceScale == 0.0F)
                scheduleDpiChange(oldScale, newScale);
        }

    }

    void Window::configureGLFW() {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, GLFW_TRUE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    }

    void Window::initNative() {
        log::impl::enableColorPrinting();

        // Add plugin library folders to dll search path
        for (const auto &path : paths::Libraries.read())  {
            if (std::fs::exists(path))
                setenv("LD_LIBRARY_PATH", fmt::format("{};{}", hex::getEnvironmentVariable("LD_LIBRARY_PATH").value_or(""), path.string().c_str()).c_str(), true);
        }

        // Redirect stdout to log file in release builds if we're not running in a terminal
        if (!ImHexApi::System::isDebugBuild() && !hasControllingTerminal()) {
            log::impl::redirectToFile();
        }
        enumerateFontsMacos();
        macosSetupDockMenu();
    }

    void Window::setupNativeWindow() {
        s_pendingDpiChange.reset();

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

        EventProjectSaved::subscribe(this, [this] {
            TaskManager::doLater([this] {
                macosMarkContentEdited(m_window, false);
            });
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

        glfwSetWindowContentScaleCallback(m_window, [](GLFWwindow *window, float xScale, float yScale) {
            updateContentScale(window, xScale, yScale);
        });
    }

    void Window::beginNativeWindowFrame() {
        if (!ImHexApi::Provider::isValid())
            macosMarkContentEdited(m_window, false);
    }

    void Window::endNativeWindowFrame() {

    }

}

#endif
