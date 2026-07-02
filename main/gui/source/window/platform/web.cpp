#include <GLFW/glfw3.h>

#include "window.hpp"
#include "hex/api/imhex_api/system.hpp"

#if defined(OS_WEB)

#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLFW/emscripten_glfw3.h>

#include <hex/api/imhex_api/system.hpp>
#include <hex/api/events/events_gui.hpp>
#include <hex/api/events/events_interaction.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/api/theme_manager.hpp>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_internal.h>


EM_JS(bool, isMacOS, (), {
    return navigator.userAgent.indexOf('Mac OS X') != -1
});

EM_JS(void, fixCanvasInPlace, (), {
    document.getElementById('canvas').classList.add('canvas-fixed');
});

EM_JS(void, setupThemeListener, (), {
    window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', event => {
        Module._handleThemeChange();
    });
});

EM_JS(bool, isDarkModeEnabled, (), {
    return window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches
});

EMSCRIPTEN_KEEPALIVE
extern "C" void handleThemeChange() {
    hex::EventOSThemeChanged::post();
}

EMSCRIPTEN_KEEPALIVE
extern "C" void updateFramebufferSize(int width, int height) {
    glfwSetWindowSize(hex::ImHexApi::System::getMainWindowHandle(), width, height);
}


EM_JS(void, setupInputModeListener, (), {
    Module.canvas.addEventListener('mousedown', function() {
        Module._enterMouseMode();
    });

    Module.canvas.addEventListener('touchstart', function() {
        Module._enterTouchMode();
    });
});

EMSCRIPTEN_KEEPALIVE
extern "C" void enterMouseMode() {
    ImGui::GetIO().AddMouseSourceEvent(ImGuiMouseSource_Mouse);
}

EMSCRIPTEN_KEEPALIVE
extern "C" void enterTouchMode() {
    ImGui::GetIO().AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
}

namespace hex {

    void Window::configureGLFW() {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_DECORATED, GL_FALSE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);
        glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);

        emscripten::glfw3::SetNextWindowCanvasSelector("#canvas");
    }

    void Window::initNative() {
        EM_ASM({
            // Save data directory
            FS.mkdir("/home/web_user/.local");
            FS.mount(IDBFS, {}, '/home/web_user/.local');

            FS.syncfs(true, function (err) {
                if (!err)
                    return;
                alert("Failed to load permanent file system: "+err);
            });

            // Center splash screen
            document.getElementById('canvas').classList.remove('canvas-fixed');
        });
    }

    static float calculateNativeScale(GLFWwindow *window) {
        int windowW, windowH;
        int displayW, displayH;
        glfwGetWindowSize(window, &windowW, &windowH);
        glfwGetFramebufferSize(window, &displayW, &displayH);

        const auto xScale = (windowW > 0) ? float(displayW) / windowW : 1.0f;
        const auto yScale = (windowH > 0) ? float(displayH) / windowH : 1.0f;

        auto scaleFactor = std::midpoint(xScale, yScale);
        if (scaleFactor <= 0.0F)
            scaleFactor = 1.0F;

        return scaleFactor;
    }

    void Window::setupNativeWindow() {
        setupThemeListener();
        setupInputModeListener();
        fixCanvasInPlace();

        bool themeFollowSystem = ImHexApi::System::usesSystemThemeDetection();
        EventOSThemeChanged::subscribe(this, [themeFollowSystem] {
            if (!themeFollowSystem) return;

            RequestChangeTheme::post(!isDarkModeEnabled() ? "Light" : "Dark");
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

        if (emscripten::glfw3::IsRuntimePlatformApple())
            ShortcutManager::enableMacOSMode();

        glfwSetWindowAttrib(m_window, GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
        glfwShowWindow(m_window);
        emscripten::glfw3::MakeCanvasResizable(m_window, "#canvas-wrapper");
        ImHexApi::System::impl::setNativeScale(calculateNativeScale(m_window));
        EventDPIChanged::post(1.0, ImHexApi::System::getBackingScaleFactor());
    }

    void Window::beginNativeWindowFrame() {

    }

    void Window::endNativeWindowFrame() {
        static float prevScaleFactor = 0;
        const float currScaleFactor = ImHexApi::System::getBackingScaleFactor();

        if (prevScaleFactor != 0 && prevScaleFactor != currScaleFactor) {
            EventDPIChanged::post(prevScaleFactor, currScaleFactor);

            ImHexApi::System::impl::setNativeScale(calculateNativeScale(m_window));

            ThemeManager::reapplyCurrentTheme();
        }
        prevScaleFactor = currScaleFactor;

        glfwSetWindowSize(m_window, EM_ASM_INT({ return document.getElementById("canvas-wrapper").clientWidth; }), EM_ASM_INT({ return document.getElementById("canvas-wrapper").clientHeight; }));
    }

}

#endif