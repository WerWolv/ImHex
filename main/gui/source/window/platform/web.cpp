#include "window.hpp"
#include "hex/api/imhex_api/system.hpp"

#if defined(OS_WEB)

#include <emscripten.h>
#include <emscripten/html5.h>
#include <hex/api/imhex_api/system.hpp>
#include <hex/api/events/events_gui.hpp>
#include <hex/api/events/events_interaction.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/api/theme_manager.hpp>

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

EM_JS(void, setupCanvasResizeListener, (), {
    const wrapper = document.getElementById('canvas-wrapper');
    const resize = () => Module._updateFramebufferSize(wrapper.clientWidth, wrapper.clientHeight);
    new ResizeObserver(resize).observe(wrapper);
    resize();
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
    hex::ImHexApi::System::resizeMainWindow(width, height);
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

    void Window::configureWindowBackend(ImHexApi::System::WindowBackend::Config &config) {
        config.glMajor = 3;
        config.glMinor = 1;
        config.decorated = false;
        config.transparent = false;
        config.highPixelDensity = true;
        config.visible = true;
        config.webCanvasSelector = "#canvas";
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

    static float calculateNativeScale(const ImHexApi::System::WindowBackend &backend) {
        auto scaleFactor = backend.getBackingScaleFactor();
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

        if (themeFollowSystem)
            EventOSThemeChanged::post();

        if (isMacOS())
            ShortcutManager::enableMacOSMode();

        m_backend->show();
        setupCanvasResizeListener();
        ImHexApi::System::impl::setNativeScale(calculateNativeScale(*m_backend));
        EventDPIChanged::post(1.0, ImHexApi::System::getBackingScaleFactor());
    }

    void Window::beginNativeWindowFrame() {

    }

    void Window::endNativeWindowFrame() {
        static float prevScaleFactor = 0;
        const float currScaleFactor = ImHexApi::System::getBackingScaleFactor();

        if (prevScaleFactor != 0 && prevScaleFactor != currScaleFactor) {
            EventDPIChanged::post(prevScaleFactor, currScaleFactor);

            ImHexApi::System::impl::setNativeScale(calculateNativeScale(*m_backend));

            ThemeManager::reapplyCurrentTheme();
        }
        prevScaleFactor = currScaleFactor;

        m_backend->setSize(EM_ASM_INT({ return document.getElementById("canvas-wrapper").clientWidth; }), EM_ASM_INT({ return document.getElementById("canvas-wrapper").clientHeight; }));
    }

}

#endif
