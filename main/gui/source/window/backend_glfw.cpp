#include "window_backend.hpp"

#include <hex/helpers/logger.hpp>

#include <imgui.h>
#undef IMGUI_IMPL_API
#define IMGUI_IMPL_API
#include <imgui_impl_glfw.h>

#if defined(OS_WINDOWS)
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(OS_MACOS)
    #define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(OS_LINUX)
    #if __has_include(<X11/Xlib.h>)
        #define GLFW_EXPOSE_NATIVE_X11
    #endif
    #if __has_include(<wayland-client.h>)
        #define GLFW_EXPOSE_NATIVE_WAYLAND
    #endif
#endif

#include <GLFW/glfw3.h>

#if defined(OS_WINDOWS) || defined(OS_MACOS) || defined(OS_LINUX)
    #include <GLFW/glfw3native.h>
#elif __has_include(<GLFW/emscripten_glfw3.h>)
    #include <GLFW/emscripten_glfw3.h>
    #define IMHEX_HAS_EMSCRIPTEN_GLFW
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <memory>
#include <numeric>
#include <string_view>
#include <system_error>
#include <utility>

namespace hex {

    namespace {

        void glfwErrorCallback(int error, const char *description) {
            const std::string_view message = description != nullptr ? description : "";
            bool isWaylandError = error == GLFW_PLATFORM_ERROR;
            #if defined(GLFW_FEATURE_UNAVAILABLE)
                isWaylandError = isWaylandError || error == GLFW_FEATURE_UNAVAILABLE;
            #endif

            if (isWaylandError && message.find("Wayland") != std::string_view::npos)
                return;

            try {
                log::error("GLFW Error [0x{:05X}] : {}", error, message);
            } catch (const std::system_error &) {
                // Logging must not throw back through GLFW's C callback boundary.
            }
        }

        class GLFWWindowBackend final : public ImHexApi::System::WindowBackend {
        public:
            ~GLFWWindowBackend() override {
                if (m_imguiInitialized)
                    this->shutdownImGui();
                this->destroy();
            }

            bool create(const Config &config, Callbacks callbacks) override {
                if (m_window != nullptr)
                    this->destroy();

                m_callbacks = std::move(callbacks);
                m_webCanvasSelector = config.webCanvasSelector;
                m_decorated = config.decorated;

                glfwDefaultWindowHints();
                glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, config.glMajor);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, config.glMinor);
                glfwWindowHint(GLFW_OPENGL_PROFILE, config.coreProfile ? GLFW_OPENGL_CORE_PROFILE : GLFW_OPENGL_ANY_PROFILE);
                glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, config.forwardCompatible ? GLFW_TRUE : GLFW_FALSE);
                glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
                glfwWindowHint(GLFW_DECORATED, config.decorated ? GLFW_TRUE : GLFW_FALSE);
                glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, config.transparent ? GLFW_TRUE : GLFW_FALSE);
                glfwWindowHint(GLFW_MAXIMIZED, config.maximized ? GLFW_TRUE : GLFW_FALSE);
                glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);

                #if defined(OS_WEB)
                    // A hidden Emscripten canvas produces an incorrect initial cursor offset.
                    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
                #else
                    glfwWindowHint(GLFW_VISIBLE, config.visible ? GLFW_TRUE : GLFW_FALSE);
                #endif

                #if defined(GLFW_SCALE_FRAMEBUFFER)
                    glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, config.highPixelDensity ? GLFW_TRUE : GLFW_FALSE);
                #endif
                #if defined(GLFW_SCALE_TO_MONITOR)
                    glfwWindowHint(GLFW_SCALE_TO_MONITOR, config.scaleToMonitor ? GLFW_TRUE : GLFW_FALSE);
                #endif
                #if defined(OS_MACOS)
                    #if defined(GLFW_COCOA_RETINA_FRAMEBUFFER)
                        glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, config.highPixelDensity ? GLFW_TRUE : GLFW_FALSE);
                    #endif
                    #if defined(GLFW_COCOA_GRAPHICS_SWITCHING)
                        glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, GLFW_TRUE);
                    #endif
                #elif defined(OS_LINUX)
                    #if defined(GLFW_WAYLAND_APP_ID)
                        glfwWindowHintString(GLFW_WAYLAND_APP_ID, config.applicationId.c_str());
                    #endif
                    #if defined(GLFW_X11_CLASS_NAME)
                        glfwWindowHintString(GLFW_X11_CLASS_NAME, config.applicationId.c_str());
                        glfwWindowHintString(GLFW_X11_INSTANCE_NAME, config.applicationId.c_str());
                    #endif
                #endif

                #if defined(IMHEX_HAS_EMSCRIPTEN_GLFW)
                    emscripten::glfw3::SetNextWindowCanvasSelector(m_webCanvasSelector.c_str());
                #endif

                m_window = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
                if (m_window == nullptr)
                    return false;

                s_callbackBackend = this;
                glfwSetWindowUserPointer(m_window, this);
                glfwSetInputMode(m_window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
                this->installCallbacks();

                glfwMakeContextCurrent(m_window);
                glfwSwapInterval(config.swapInterval);
                return true;
            }

            void destroy() override {
                if (m_window == nullptr)
                    return;

                if (m_imguiInitialized)
                    this->shutdownImGui();

                if (s_callbackBackend == this)
                    s_callbackBackend = nullptr;
                glfwDestroyWindow(m_window);
                m_window = nullptr;
                m_callbacks = {};
                m_windowedGeometry.reset();
            }

            void pollEvents() override {
                glfwPollEvents();
            }

            void waitEvents() override {
                glfwWaitEvents();
            }

            bool waitEvents(double timeout) override {
                if (timeout <= 0.0) {
                    glfwPollEvents();
                    return false;
                }

                const auto start = std::chrono::steady_clock::now();
                glfwWaitEventsTimeout(timeout);
                const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                return elapsed < timeout;
            }

            void wakeEventLoop() override {
                glfwPostEmptyEvent();
            }

            bool initializeImGui() override {
                if (m_imguiInitialized)
                    return true;
                if (m_window == nullptr)
                    return false;

                m_imguiInitialized = ImGui_ImplGlfw_InitForOpenGL(m_window, true);
                if (!m_imguiInitialized) {
                    return false;
                }

                #if defined(__EMSCRIPTEN__)
                    ImGui_ImplGlfw_InstallEmscriptenCallbacks(m_window, m_webCanvasSelector.c_str());
                #endif
                ImGui_ImplGlfw_SetCallbacksChainForAllWindows(true);
                return true;
            }

            void shutdownImGui() override {
                if (!m_imguiInitialized)
                    return;

                ImGui_ImplGlfw_Shutdown();
                m_imguiInitialized = false;
            }

            void newImGuiFrame() override {
                if (m_imguiInitialized)
                    ImGui_ImplGlfw_NewFrame();
            }

            void renderImGuiPlatformWindows() override {
                GLFWwindow *previousContext = glfwGetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(previousContext);
            }

            void show() override {
                glfwShowWindow(m_window);
            }

            void hide() override {
                glfwHideWindow(m_window);
            }

            bool shouldClose() const override {
                return glfwWindowShouldClose(m_window) == GLFW_TRUE;
            }

            void setShouldClose(bool close) override {
                glfwSetWindowShouldClose(m_window, close ? GLFW_TRUE : GLFW_FALSE);
            }

            void setPosition(i32 x, i32 y) override {
                glfwSetWindowPos(m_window, x, y);
            }

            void setSize(i32 width, i32 height) override {
                glfwSetWindowSize(m_window, width, height);
            }

            void setSizeLimits(i32 minWidth, i32 minHeight, std::optional<i32> maxWidth, std::optional<i32> maxHeight) override {
                glfwSetWindowSizeLimits(
                    m_window,
                    minWidth,
                    minHeight,
                    maxWidth.value_or(GLFW_DONT_CARE),
                    maxHeight.value_or(GLFW_DONT_CARE));
            }

            void setResizable(bool enabled) override {
                glfwSetWindowAttrib(m_window, GLFW_RESIZABLE, enabled ? GLFW_TRUE : GLFW_FALSE);
            }

            void setTitle(const std::string &title) override {
                glfwSetWindowTitle(m_window, title.c_str());
            }

            void setAlwaysOnTop(bool enabled) override {
                glfwSetWindowAttrib(m_window, GLFW_FLOATING, enabled ? GLFW_TRUE : GLFW_FALSE);
            }

            bool isAlwaysOnTop() const override {
                return glfwGetWindowAttrib(m_window, GLFW_FLOATING) == GLFW_TRUE;
            }

            void setFullscreen(bool enabled) override {
                if (enabled == this->isFullscreen())
                    return;

                if (enabled) {
                    WindowGeometry geometry;
                    glfwGetWindowPos(m_window, &geometry.x, &geometry.y);
                    glfwGetWindowSize(m_window, &geometry.width, &geometry.height);

                    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
                    const GLFWvidmode *mode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
                    if (mode == nullptr)
                        return;

                    m_windowedGeometry = geometry;
                    glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                } else {
                    const auto geometry = m_windowedGeometry.value_or(WindowGeometry {});
                    glfwSetWindowMonitor(m_window, nullptr, geometry.x, geometry.y, geometry.width, geometry.height, GLFW_DONT_CARE);
                    glfwSetWindowAttrib(m_window, GLFW_DECORATED, m_decorated ? GLFW_TRUE : GLFW_FALSE);
                    m_windowedGeometry.reset();
                }
            }

            bool isFullscreen() const override {
                return glfwGetWindowMonitor(m_window) != nullptr;
            }

            void minimize() override {
                glfwIconifyWindow(m_window);
            }

            void maximize() override {
                glfwMaximizeWindow(m_window);
            }

            void restore() override {
                glfwRestoreWindow(m_window);
            }

            bool isMaximized() const override {
                return glfwGetWindowAttrib(m_window, GLFW_MAXIMIZED) == GLFW_TRUE;
            }

            bool isMinimized() const override {
                return glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) == GLFW_TRUE;
            }

            bool isVisible() const override {
                return glfwGetWindowAttrib(m_window, GLFW_VISIBLE) == GLFW_TRUE;
            }

            bool isFocused() const override {
                return glfwGetWindowAttrib(m_window, GLFW_FOCUSED) == GLFW_TRUE;
            }

            void focus() override {
                glfwFocusWindow(m_window);
            }

            void requestAttention() override {
                glfwRequestWindowAttention(m_window);
            }

            void setOpacity(float opacity) override {
                glfwSetWindowOpacity(m_window, std::clamp(opacity, 0.0F, 1.0F));
            }

            ImHexApi::System::InitialWindowProperties getWindowProperties() const override {
                int x = 0, y = 0, width = 0, height = 0;
                glfwGetWindowPos(m_window, &x, &y);
                glfwGetWindowSize(m_window, &width, &height);
                return {
                    .x = x,
                    .y = y,
                    .width = static_cast<u32>(std::max(width, 0)),
                    .height = static_cast<u32>(std::max(height, 0)),
                    .maximized = this->isMaximized(),
                };
            }

            std::pair<i32, i32> getFramebufferSize() const override {
                int width = 0, height = 0;
                glfwGetFramebufferSize(m_window, &width, &height);
                return { width, height };
            }

            float getContentScale() const override {
                float xScale = 1.0F, yScale = 1.0F;
                glfwGetWindowContentScale(m_window, &xScale, &yScale);
                const float scale = std::midpoint(xScale, yScale);
                return scale > 0.0F ? scale : 1.0F;
            }

            float getBackingScaleFactor() const override {
                int windowWidth = 0, framebufferWidth = 0;
                glfwGetWindowSize(m_window, &windowWidth, nullptr);
                glfwGetFramebufferSize(m_window, &framebufferWidth, nullptr);
                return windowWidth > 0 ? static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth) : 1.0F;
            }

            std::optional<Monitor> getPrimaryMonitor() const override {
                GLFWmonitor *monitor = glfwGetPrimaryMonitor();
                const GLFWvidmode *mode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
                if (mode == nullptr)
                    return std::nullopt;

                int x = 0, y = 0;
                glfwGetMonitorPos(monitor, &x, &y);
                return Monitor {
                    .x = x,
                    .y = y,
                    .width = mode->width,
                    .height = mode->height,
                    .refreshRate = static_cast<float>(mode->refreshRate),
                };
            }

            ImHexApi::System::NativeWindow getNativeWindow() const override {
                #if defined(OS_WINDOWS)
                    return { ImHexApi::System::NativeWindowType::Win32, glfwGetWin32Window(m_window), nullptr };
                #elif defined(OS_MACOS)
                    return { ImHexApi::System::NativeWindowType::Cocoa, glfwGetCocoaWindow(m_window), nullptr };
                #elif defined(OS_LINUX)
                    #if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4)
                        #if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
                            if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
                                return { ImHexApi::System::NativeWindowType::Wayland, glfwGetWaylandWindow(m_window), glfwGetWaylandDisplay() };
                        #endif
                        #if defined(GLFW_EXPOSE_NATIVE_X11)
                            if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
                                const auto handle = reinterpret_cast<void *>(static_cast<std::uintptr_t>(glfwGetX11Window(m_window)));
                                return { ImHexApi::System::NativeWindowType::X11, handle, glfwGetX11Display() };
                            }
                        #endif
                    #endif
                    return {};
                #else
                    return {};
                #endif
            }

            bool isWayland() const override {
                #if defined(OS_LINUX) && (GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4))
                    return glfwGetPlatform() == GLFW_PLATFORM_WAYLAND;
                #else
                    return false;
                #endif
            }

            const char* getName() const override {
                return "GLFW";
            }

            void makeContextCurrent() override {
                glfwMakeContextCurrent(m_window);
            }

            void swapBuffers() override {
                glfwSwapBuffers(m_window);
            }

        private:
            struct WindowGeometry {
                int x = 0;
                int y = 0;
                int width = 1280;
                int height = 720;
            };

            static GLFWWindowBackend* getBackend(GLFWwindow *window) {
                if (s_callbackBackend == nullptr || s_callbackBackend->m_window != window)
                    return nullptr;
                return s_callbackBackend;
            }

            void inputActivity() const {
                if (m_callbacks.inputActivity)
                    m_callbacks.inputActivity();
            }

            static bool isModifierKey(int key) {
                return key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL ||
                       key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT ||
                       key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT ||
                       key == GLFW_KEY_LEFT_SUPER || key == GLFW_KEY_RIGHT_SUPER;
            }

            static int translateKey(int key, int scanCode, int modifiers) {
                #if !defined(OS_WEB)
                    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
                        const char *keyName = glfwGetKeyName(key, scanCode);
                        const std::string_view name = keyName != nullptr ? keyName : "";
                        if (name.size() == 1 && static_cast<unsigned char>(name.front()) <= 0x7F)
                            key = std::toupper(static_cast<unsigned char>(name.front()));
                    }
                #else
                    static_cast<void>(scanCode);
                #endif

                #if !defined(OS_MACOS)
                    if ((modifiers & GLFW_MOD_NUM_LOCK) == 0) {
                        switch (key) {
                            case GLFW_KEY_KP_0: key = GLFW_KEY_INSERT;    break;
                            case GLFW_KEY_KP_1: key = GLFW_KEY_END;       break;
                            case GLFW_KEY_KP_2: key = GLFW_KEY_DOWN;      break;
                            case GLFW_KEY_KP_3: key = GLFW_KEY_PAGE_DOWN; break;
                            case GLFW_KEY_KP_4: key = GLFW_KEY_LEFT;      break;
                            case GLFW_KEY_KP_6: key = GLFW_KEY_RIGHT;     break;
                            case GLFW_KEY_KP_7: key = GLFW_KEY_HOME;      break;
                            case GLFW_KEY_KP_8: key = GLFW_KEY_UP;        break;
                            case GLFW_KEY_KP_9: key = GLFW_KEY_PAGE_UP;   break;
                            default: break;
                        }
                    }
                #else
                    static_cast<void>(modifiers);
                #endif

                return key;
            }

            void installCallbacks() {
                glfwSetWindowPosCallback(m_window, [](GLFWwindow *window, int x, int y) {
                    auto *backend = getBackend(window);
                    if (backend == nullptr)
                        return;
                    backend->inputActivity();
                    if (backend->m_callbacks.moved)
                        backend->m_callbacks.moved(x, y);
                });
                glfwSetWindowSizeCallback(m_window, [](GLFWwindow *window, int width, int height) {
                    auto *backend = getBackend(window);
                    if (backend == nullptr)
                        return;
                    backend->inputActivity();
                    if (backend->m_callbacks.resized)
                        backend->m_callbacks.resized(width, height);
                });
                glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow *window, int width, int height) {
                    auto *backend = getBackend(window);
                    if (backend == nullptr)
                        return;
                    if (backend->m_callbacks.framebufferResized)
                        backend->m_callbacks.framebufferResized(width, height);
                });
                glfwSetWindowFocusCallback(m_window, [](GLFWwindow *window, int focused) {
                    auto *backend = getBackend(window);
                    if (backend == nullptr)
                        return;
                    backend->inputActivity();
                    if (backend->m_callbacks.focused)
                        backend->m_callbacks.focused(focused == GLFW_TRUE);
                });
                glfwSetCursorPosCallback(m_window, [](GLFWwindow *window, double, double) {
                    if (auto *backend = getBackend(window); backend != nullptr)
                        backend->inputActivity();
                });
                glfwSetMouseButtonCallback(m_window, [](GLFWwindow *window, int, int, int) {
                    if (auto *backend = getBackend(window); backend != nullptr)
                        backend->inputActivity();
                });
                glfwSetScrollCallback(m_window, [](GLFWwindow *window, double, double) {
                    if (auto *backend = getBackend(window); backend != nullptr)
                        backend->inputActivity();
                });
                glfwSetKeyCallback(m_window, [](GLFWwindow *window, int key, int scanCode, int action, int modifiers) {
                    if (key == GLFW_KEY_UNKNOWN || (action != GLFW_PRESS && action != GLFW_REPEAT) || isModifierKey(key))
                        return;

                    auto *backend = getBackend(window);
                    if (backend == nullptr)
                        return;
                    backend->inputActivity();
                    key = translateKey(key, scanCode, modifiers);
                    const Keys translatedKey = scanCodeToKey(key);
                    if (translatedKey != Keys::Invalid && backend->m_callbacks.keyPressed)
                        backend->m_callbacks.keyPressed(translatedKey);
                });
                glfwSetWindowCloseCallback(m_window, [](GLFWwindow *window) {
                    auto *backend = getBackend(window);
                    if (backend == nullptr)
                        return;
                    backend->inputActivity();
                    if (backend->m_callbacks.closeRequested)
                        backend->m_callbacks.closeRequested();
                });
                glfwSetDropCallback(m_window, [](GLFWwindow *window, int count, const char **paths) {
                    auto *backend = getBackend(window);
                    if (backend == nullptr)
                        return;
                    backend->inputActivity();
                    if (!backend->m_callbacks.fileDropped)
                        return;

                    for (int index = 0; index < count; ++index)
                        backend->m_callbacks.fileDropped(std::fs::path(reinterpret_cast<const char8_t *>(paths[index])));
                });
                glfwSetWindowRefreshCallback(m_window, [](GLFWwindow *window) {
                    auto *backend = getBackend(window);
                    if (backend == nullptr)
                        return;
                    if (backend->m_callbacks.refreshRequested)
                        backend->m_callbacks.refreshRequested();
                });
            }

            GLFWwindow *m_window = nullptr;
            Callbacks m_callbacks;
            std::optional<WindowGeometry> m_windowedGeometry;
            std::string m_webCanvasSelector = "#canvas";
            bool m_decorated = true;
            bool m_imguiInitialized = false;

            inline static GLFWWindowBackend *s_callbackBackend = nullptr;
        };

    }

    bool initializeWindowing() {
        glfwSetErrorCallback(glfwErrorCallback);
        return glfwInit() == GLFW_TRUE;
    }

    void shutdownWindowing() {
        glfwTerminate();
    }

    std::unique_ptr<ImHexApi::System::WindowBackend> createWindowBackend() {
        return std::make_unique<GLFWWindowBackend>();
    }

}
