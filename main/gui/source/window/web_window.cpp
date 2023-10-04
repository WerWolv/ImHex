#include "window.hpp"

#if defined(OS_EMSCRIPTEN)

#include <emscripten.h>
#include <emscripten/html5.h>

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

namespace hex {

    void nativeErrorMessage(const std::string &message) {
        log::fatal(message);
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
        });
    }

    void Window::setupNativeWindow() {
        resizeCanvas();
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