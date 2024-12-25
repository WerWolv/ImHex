#include "window.hpp"

#if defined(OS_WEB)

#include <emscripten.h>
#include <emscripten/html5.h>

#include <hex/api/event_manager.hpp>

#include <imgui.h>
#include <imgui_internal.h>

// Function used by c++ to get the size of the html canvas
EM_JS(int, canvas_get_width, (), {
    return Module.canvas.width;
});

// Function used by c++ to get the size of the html canvas
EM_JS(int, canvas_get_height, (), {
    return Module.canvas.height;
});

// Function called by javascript
EM_JS(void, resizeCanvas, (), {
    js_resizeCanvas();
});

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

    void nativeErrorMessage(const std::string &message) {
        log::fatal(message);
        EM_ASM({
            alert(UTF8ToString($0));
        }, message.c_str());
    }

    void Window::configureGLFW() {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_DECORATED, GL_FALSE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);
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

    void Window::setupNativeWindow() {
        resizeCanvas();
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
            resizeCanvas();
            win->fullFrame();
        });

        if (themeFollowSystem)
            EventOSThemeChanged::post();

        if (isMacOS())
            ShortcutManager::enableMacOSMode();
    }

    void Window::beginNativeWindowFrame() {
        static i32 prevWidth = 0;
        static i32 prevHeight = 0;

        auto width = canvas_get_width();
        auto height = canvas_get_height();

        if (prevWidth != width || prevHeight != height) {
            // Size has changed

            prevWidth  = width;
            prevHeight = height;
            this->resize(width, height);
        }
    }

    void Window::endNativeWindowFrame() {
    }

}

#endif