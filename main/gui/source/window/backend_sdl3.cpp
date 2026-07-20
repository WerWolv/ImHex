#include "window_backend.hpp"

#include <SDL3/SDL.h>

#include <imgui.h>
#undef IMGUI_IMPL_API
#define IMGUI_IMPL_API
#include <imgui_impl_sdl3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>

#include <hex/helpers/logger.hpp>

namespace hex {

    namespace {

        Uint32 s_wakeEventType = 0;
        unsigned int s_windowingReferences = 0;

        Keys translateKey(SDL_Keycode keycode, SDL_Scancode scancode, SDL_Keymod modifiers) {
            [[maybe_unused]] const bool numLockEnabled = (modifiers & SDL_KMOD_NUM) != 0;

            switch (scancode) {
                case SDL_SCANCODE_KP_0:
                    #if !defined(SDL_PLATFORM_MACOS)
                        if (!numLockEnabled) return Keys::Insert;
                    #endif
                    return Keys::KeyPad0;
                case SDL_SCANCODE_KP_1:
                    #if !defined(SDL_PLATFORM_MACOS)
                        if (!numLockEnabled) return Keys::End;
                    #endif
                    return Keys::KeyPad1;
                case SDL_SCANCODE_KP_2:
                    #if !defined(SDL_PLATFORM_MACOS)
                        if (!numLockEnabled) return Keys::Down;
                    #endif
                    return Keys::KeyPad2;
                case SDL_SCANCODE_KP_3:
                    #if !defined(SDL_PLATFORM_MACOS)
                        if (!numLockEnabled) return Keys::PageDown;
                    #endif
                    return Keys::KeyPad3;
                case SDL_SCANCODE_KP_4:
                    #if !defined(SDL_PLATFORM_MACOS)
                        if (!numLockEnabled) return Keys::Left;
                    #endif
                    return Keys::KeyPad4;
                case SDL_SCANCODE_KP_5:
                    #if !defined(SDL_PLATFORM_MACOS)
                        if (!numLockEnabled) return Keys::Invalid;
                    #endif
                    return Keys::KeyPad5;
                case SDL_SCANCODE_KP_6:
                    #if !defined(SDL_PLATFORM_MACOS)
                        if (!numLockEnabled) return Keys::Right;
                    #endif
                    return Keys::KeyPad6;
                case SDL_SCANCODE_KP_7:
                    #if !defined(SDL_PLATFORM_MACOS)
                        if (!numLockEnabled) return Keys::Home;
                    #endif
                    return Keys::KeyPad7;
                case SDL_SCANCODE_KP_8:
                    #if !defined(SDL_PLATFORM_MACOS)
                        if (!numLockEnabled) return Keys::Up;
                    #endif
                    return Keys::KeyPad8;
                case SDL_SCANCODE_KP_9:
                    #if !defined(SDL_PLATFORM_MACOS)
                        if (!numLockEnabled) return Keys::PageUp;
                    #endif
                    return Keys::KeyPad9;
                case SDL_SCANCODE_KP_PERIOD:
                    #if !defined(SDL_PLATFORM_MACOS)
                        if (!numLockEnabled) return Keys::Delete;
                    #endif
                    return Keys::KeyPadDecimal;
                case SDL_SCANCODE_KP_DIVIDE:   return Keys::KeyPadDivide;
                case SDL_SCANCODE_KP_MULTIPLY: return Keys::KeyPadMultiply;
                case SDL_SCANCODE_KP_MINUS:    return Keys::KeyPadSubtract;
                case SDL_SCANCODE_KP_PLUS:     return Keys::KeyPadAdd;
                case SDL_SCANCODE_KP_ENTER:    return Keys::KeyPadEnter;
                case SDL_SCANCODE_KP_EQUALS:   return Keys::KeyPadEqual;
                case SDL_SCANCODE_NONUSHASH:      return Keys::World1;
                case SDL_SCANCODE_NONUSBACKSLASH: return Keys::World2;
                default: break;
            }

            if (keycode >= SDLK_0 && keycode <= SDLK_9)
                return static_cast<Keys>(static_cast<int>(Keys::Num0) + static_cast<int>(keycode - SDLK_0));
            if (keycode >= SDLK_A && keycode <= SDLK_Z)
                return static_cast<Keys>(static_cast<int>(Keys::A) + static_cast<int>(keycode - SDLK_A));
            if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12)
                return static_cast<Keys>(static_cast<int>(Keys::F1) + static_cast<int>(scancode - SDL_SCANCODE_F1));
            if (scancode >= SDL_SCANCODE_F13 && scancode <= SDL_SCANCODE_F24)
                return static_cast<Keys>(static_cast<int>(Keys::F13) + static_cast<int>(scancode - SDL_SCANCODE_F13));

            switch (keycode) {
                case SDLK_SPACE:       return Keys::Space;
                case SDLK_APOSTROPHE:  return Keys::Apostrophe;
                case SDLK_COMMA:       return Keys::Comma;
                case SDLK_MINUS:       return Keys::Minus;
                case SDLK_PERIOD:      return Keys::Period;
                case SDLK_SLASH:       return Keys::Slash;
                case SDLK_SEMICOLON:   return Keys::Semicolon;
                case SDLK_EQUALS:      return Keys::Equals;
                case SDLK_LEFTBRACKET: return Keys::LeftBracket;
                case SDLK_BACKSLASH:   return Keys::Backslash;
                case SDLK_RIGHTBRACKET:return Keys::RightBracket;
                case SDLK_GRAVE:       return Keys::GraveAccent;
                case SDLK_ESCAPE:      return Keys::Escape;
                case SDLK_RETURN:      return Keys::Enter;
                case SDLK_TAB:         return Keys::Tab;
                case SDLK_BACKSPACE:   return Keys::Backspace;
                case SDLK_INSERT:      return Keys::Insert;
                case SDLK_DELETE:      return Keys::Delete;
                case SDLK_RIGHT:       return Keys::Right;
                case SDLK_LEFT:        return Keys::Left;
                case SDLK_DOWN:        return Keys::Down;
                case SDLK_UP:          return Keys::Up;
                case SDLK_PAGEUP:      return Keys::PageUp;
                case SDLK_PAGEDOWN:    return Keys::PageDown;
                case SDLK_HOME:        return Keys::Home;
                case SDLK_END:         return Keys::End;
                case SDLK_CAPSLOCK:    return Keys::CapsLock;
                case SDLK_SCROLLLOCK:  return Keys::ScrollLock;
                case SDLK_NUMLOCKCLEAR:return Keys::NumLock;
                case SDLK_PRINTSCREEN: return Keys::PrintScreen;
                case SDLK_PAUSE:       return Keys::Pause;
                case SDLK_APPLICATION:
                case SDLK_MENU:        return Keys::Menu;
                default:               return Keys::Invalid;
            }
        }

        bool isInputEvent(Uint32 type) {
            switch (type) {
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                case SDL_EVENT_TEXT_EDITING:
                case SDL_EVENT_TEXT_INPUT:
                case SDL_EVENT_MOUSE_MOTION:
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                case SDL_EVENT_MOUSE_WHEEL:
                case SDL_EVENT_FINGER_DOWN:
                case SDL_EVENT_FINGER_UP:
                case SDL_EVENT_FINGER_MOTION:
                case SDL_EVENT_FINGER_CANCELED:
                case SDL_EVENT_PEN_PROXIMITY_IN:
                case SDL_EVENT_PEN_PROXIMITY_OUT:
                case SDL_EVENT_PEN_DOWN:
                case SDL_EVENT_PEN_UP:
                case SDL_EVENT_PEN_BUTTON_DOWN:
                case SDL_EVENT_PEN_BUTTON_UP:
                case SDL_EVENT_PEN_MOTION:
                case SDL_EVENT_PEN_AXIS:
                    return true;
                default:
                    return false;
            }
        }

        class SDL3WindowBackend final : public ImHexApi::System::WindowBackend {
        public:
            ~SDL3WindowBackend() override {
                this->destroy();
            }

            bool create(const Config &config, Callbacks callbacks) override {
                this->destroy();

                SDL_SetHint(SDL_HINT_APP_ID, config.applicationId.c_str());
                SDL_SetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR, config.webCanvasSelector.c_str());
                SDL_SetAppMetadata(config.title.c_str(), nullptr, config.applicationId.c_str());

                if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, config.glMajor) ||
                    !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, config.glMinor) ||
                    !SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, config.forwardCompatible ? SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG : 0) ||
                    !SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, config.coreProfile ? SDL_GL_CONTEXT_PROFILE_CORE : 0) ||
                    !SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, config.transparent ? 8 : 0) ||
                    !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1))
                    return false;

                SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
                if (config.resizable)       flags |= SDL_WINDOW_RESIZABLE;
                if (!config.decorated)      flags |= SDL_WINDOW_BORDERLESS;
                if (config.transparent)     flags |= SDL_WINDOW_TRANSPARENT;
                if (!config.visible)        flags |= SDL_WINDOW_HIDDEN;
                if (config.maximized)       flags |= SDL_WINDOW_MAXIMIZED;
                if (config.highPixelDensity) flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;

                m_callbacks = std::move(callbacks);
                m_resizable = config.resizable;
                m_window = SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags);
                if (m_window == nullptr) {
                    m_callbacks = {};
                    return false;
                }

                if (!config.decorated)
                    SDL_SetWindowHitTest(m_window, hitTest, this);

                m_context = SDL_GL_CreateContext(m_window);
                if (m_context == nullptr || !SDL_GL_MakeCurrent(m_window, m_context)) {
                    this->destroy();
                    return false;
                }

                SDL_GL_SetSwapInterval(config.swapInterval);
                m_windowId = SDL_GetWindowID(m_window);
                if (m_windowId == 0) {
                    this->destroy();
                    return false;
                }
                m_shouldClose = false;
                return true;
            }

            void destroy() override {
                this->shutdownImGui();

                if (m_context != nullptr) {
                    SDL_GL_DestroyContext(m_context);
                    m_context = nullptr;
                }
                if (m_window != nullptr) {
                    SDL_DestroyWindow(m_window);
                    m_window = nullptr;
                }

                m_windowId = 0;
                m_shouldClose = false;
                m_resizable = false;
                m_sizeLimits.reset();
                m_callbacks = {};
            }

            void pollEvents() override {
                SDL_Event event;
                while (SDL_PollEvent(&event))
                    this->processEvent(event);
            }

            void waitEvents() override {
                SDL_Event event;
                if (SDL_WaitEvent(&event)) {
                    this->processEvent(event);
                    this->pollEvents();
                }
            }

            bool waitEvents(double timeout) override {
                if (std::isinf(timeout) && timeout > 0.0) {
                    SDL_Event event;
                    if (!SDL_WaitEvent(&event))
                        return false;
                    this->processEvent(event);
                    this->pollEvents();
                    return true;
                }

                const double milliseconds = std::isnan(timeout) ? 0.0 : std::ceil(std::max(timeout, 0.0) * 1000.0);
                const auto timeoutMilliseconds = static_cast<Sint32>(std::min(
                    milliseconds,
                    static_cast<double>(std::numeric_limits<Sint32>::max())));

                SDL_Event event;
                if (!SDL_WaitEventTimeout(&event, timeoutMilliseconds))
                    return false;

                this->processEvent(event);
                this->pollEvents();
                return true;
            }

            void wakeEventLoop() override {
                if (s_wakeEventType == 0)
                    return;

                SDL_Event event = {};
                event.type = s_wakeEventType;
                SDL_PushEvent(&event);
            }

            bool initializeImGui() override {
                if (m_imguiInitialized)
                    return true;
                if (m_window == nullptr || m_context == nullptr)
                    return false;

                m_imguiInitialized = ImGui_ImplSDL3_InitForOpenGL(m_window, m_context);
                return m_imguiInitialized;
            }

            void shutdownImGui() override {
                if (m_imguiInitialized) {
                    if (ImGui::GetCurrentContext() != nullptr)
                        ImGui_ImplSDL3_Shutdown();
                    m_imguiInitialized = false;
                }
            }

            void newImGuiFrame() override {
                if (m_imguiInitialized)
                    ImGui_ImplSDL3_NewFrame();
            }

            void renderImGuiPlatformWindows() override {
                if (!m_imguiInitialized || (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
                    return;

                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                SDL_GL_MakeCurrent(m_window, m_context);
            }

            void show() override {
                if (m_window != nullptr) SDL_ShowWindow(m_window);
            }

            void hide() override {
                if (m_window != nullptr) SDL_HideWindow(m_window);
            }

            bool shouldClose() const override {
                return m_shouldClose;
            }

            void setShouldClose(bool close) override {
                m_shouldClose = close;
            }

            void setPosition(i32 x, i32 y) override {
                if (m_window != nullptr) SDL_SetWindowPosition(m_window, x, y);
            }

            void setSize(i32 width, i32 height) override {
                if (m_window != nullptr) SDL_SetWindowSize(m_window, width, height);
            }

            void setSizeLimits(i32 minWidth, i32 minHeight, std::optional<i32> maxWidth, std::optional<i32> maxHeight) override {
                if (m_window == nullptr)
                    return;

                const SizeLimits limits {
                    .minWidth = minWidth,
                    .minHeight = minHeight,
                    .maxWidth = maxWidth.value_or(0),
                    .maxHeight = maxHeight.value_or(0),
                };
                if (m_sizeLimits == limits)
                    return;

                if (SDL_SetWindowMinimumSize(m_window, limits.minWidth, limits.minHeight) &&
                    SDL_SetWindowMaximumSize(m_window, limits.maxWidth, limits.maxHeight))
                    m_sizeLimits = limits;
            }

            void setResizable(bool enabled) override {
                m_resizable = enabled;
                if (m_window != nullptr) SDL_SetWindowResizable(m_window, enabled);
            }

            void setTitle(const std::string &title) override {
                if (m_window != nullptr) SDL_SetWindowTitle(m_window, title.c_str());
            }

            void setAlwaysOnTop(bool enabled) override {
                if (m_window != nullptr) SDL_SetWindowAlwaysOnTop(m_window, enabled);
            }

            bool isAlwaysOnTop() const override {
                return this->hasWindowFlag(SDL_WINDOW_ALWAYS_ON_TOP);
            }

            void setFullscreen(bool enabled) override {
                if (m_window != nullptr) SDL_SetWindowFullscreen(m_window, enabled);
            }

            bool isFullscreen() const override {
                return this->hasWindowFlag(SDL_WINDOW_FULLSCREEN);
            }

            void minimize() override {
                if (m_window != nullptr) SDL_MinimizeWindow(m_window);
            }

            void maximize() override {
                if (m_window != nullptr) SDL_MaximizeWindow(m_window);
            }

            void restore() override {
                if (m_window != nullptr) SDL_RestoreWindow(m_window);
            }

            bool isMaximized() const override {
                return this->hasWindowFlag(SDL_WINDOW_MAXIMIZED);
            }

            bool isMinimized() const override {
                return this->hasWindowFlag(SDL_WINDOW_MINIMIZED);
            }

            bool isVisible() const override {
                return m_window != nullptr && !this->hasWindowFlag(SDL_WINDOW_HIDDEN);
            }

            bool isFocused() const override {
                return this->hasWindowFlag(SDL_WINDOW_INPUT_FOCUS);
            }

            void focus() override {
                if (m_window != nullptr) SDL_RaiseWindow(m_window);
            }

            void requestAttention() override {
                if (m_window != nullptr) SDL_FlashWindow(m_window, SDL_FLASH_UNTIL_FOCUSED);
            }

            void setOpacity(float opacity) override {
                if (m_window != nullptr) SDL_SetWindowOpacity(m_window, std::clamp(opacity, 0.0F, 1.0F));
            }

            ImHexApi::System::InitialWindowProperties getWindowProperties() const override {
                ImHexApi::System::InitialWindowProperties properties = {};
                if (m_window == nullptr)
                    return properties;

                int width = 0, height = 0;
                SDL_GetWindowPosition(m_window, &properties.x, &properties.y);
                SDL_GetWindowSize(m_window, &width, &height);
                properties.width = width > 0 ? static_cast<u32>(width) : 0;
                properties.height = height > 0 ? static_cast<u32>(height) : 0;
                properties.maximized = this->isMaximized();
                return properties;
            }

            std::pair<i32, i32> getFramebufferSize() const override {
                int width = 0, height = 0;
                if (m_window != nullptr)
                    SDL_GetWindowSizeInPixels(m_window, &width, &height);
                return { width, height };
            }

            float getContentScale() const override {
                return m_window != nullptr ? SDL_GetWindowDisplayScale(m_window) : 1.0F;
            }

            float getBackingScaleFactor() const override {
                return m_window != nullptr ? SDL_GetWindowPixelDensity(m_window) : 1.0F;
            }

            std::optional<Monitor> getPrimaryMonitor() const override {
                const SDL_DisplayID display = SDL_GetPrimaryDisplay();
                SDL_Rect bounds = {};
                const SDL_DisplayMode *mode = display != 0 ? SDL_GetDesktopDisplayMode(display) : nullptr;
                if (display == 0 || mode == nullptr || !SDL_GetDisplayBounds(display, &bounds))
                    return std::nullopt;

                return Monitor {
                    .x = bounds.x,
                    .y = bounds.y,
                    .width = bounds.w,
                    .height = bounds.h,
                    .refreshRate = mode->refresh_rate > 0.0F ? mode->refresh_rate : 60.0F,
                };
            }

            ImHexApi::System::NativeWindow getNativeWindow() const override {
                if (m_window == nullptr)
                    return {};

                const SDL_PropertiesID properties = SDL_GetWindowProperties(m_window);
                if (properties == 0)
                    return {};

                if (void *handle = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr); handle != nullptr)
                    return { ImHexApi::System::NativeWindowType::Win32, handle, nullptr };
                if (void *handle = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr); handle != nullptr)
                    return { ImHexApi::System::NativeWindowType::Cocoa, handle, nullptr };

                if (void *display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr); display != nullptr) {
                    void *surface = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
                    return { ImHexApi::System::NativeWindowType::Wayland, surface, display };
                }

                if (void *display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr); display != nullptr) {
                    const auto window = static_cast<std::uintptr_t>(SDL_GetNumberProperty(
                        properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
                    return { ImHexApi::System::NativeWindowType::X11, reinterpret_cast<void *>(window), display };
                }

                return {};
            }

            bool isWayland() const override {
                const char *driver = SDL_GetCurrentVideoDriver();
                return driver != nullptr && std::string_view(driver) == "wayland";
            }

            const char* getName() const override {
                return "SDL3";
            }

            void makeContextCurrent() override {
                if (m_window != nullptr && m_context != nullptr)
                    SDL_GL_MakeCurrent(m_window, m_context);
            }

            void swapBuffers() override {
                if (m_window != nullptr) SDL_GL_SwapWindow(m_window);
            }

        private:
            struct SizeLimits {
                i32 minWidth;
                i32 minHeight;
                i32 maxWidth;
                i32 maxHeight;

                bool operator==(const SizeLimits &) const = default;
            };

            static SDL_HitTestResult SDLCALL hitTest(SDL_Window *window, const SDL_Point *area, void *data) {
                auto *backend = static_cast<SDL3WindowBackend *>(data);
                if (backend == nullptr || !backend->m_resizable ||
                    backend->hasWindowFlag(SDL_WINDOW_MAXIMIZED) ||
                    backend->hasWindowFlag(SDL_WINDOW_FULLSCREEN))
                    return SDL_HITTEST_NORMAL;

                int width = 0, height = 0;
                if (!SDL_GetWindowSize(window, &width, &height))
                    return SDL_HITTEST_NORMAL;

                constexpr int ResizeBorder = 8;
                const bool left = area->x < ResizeBorder;
                const bool right = area->x >= width - ResizeBorder;
                const bool top = area->y < ResizeBorder;
                const bool bottom = area->y >= height - ResizeBorder;

                if (top && left) return SDL_HITTEST_RESIZE_TOPLEFT;
                if (top && right) return SDL_HITTEST_RESIZE_TOPRIGHT;
                if (bottom && left) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
                if (bottom && right) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
                if (top) return SDL_HITTEST_RESIZE_TOP;
                if (right) return SDL_HITTEST_RESIZE_RIGHT;
                if (bottom) return SDL_HITTEST_RESIZE_BOTTOM;
                if (left) return SDL_HITTEST_RESIZE_LEFT;
                return SDL_HITTEST_NORMAL;
            }

            bool hasWindowFlag(SDL_WindowFlags flag) const {
                return m_window != nullptr && (SDL_GetWindowFlags(m_window) & flag) != 0;
            }

            bool isMainWindowEvent(const SDL_Event &event) const {
                SDL_Window *eventWindow = SDL_GetWindowFromEvent(&event);
                return eventWindow != nullptr && SDL_GetWindowID(eventWindow) == m_windowId;
            }

            void processEvent(const SDL_Event &event) {
                if (m_imguiInitialized)
                    ImGui_ImplSDL3_ProcessEvent(&event);

                if (event.type == SDL_EVENT_QUIT) {
                    m_shouldClose = true;
                    if (m_callbacks.inputActivity) m_callbacks.inputActivity();
                    if (m_callbacks.closeRequested) m_callbacks.closeRequested();
                    return;
                }

                if (!this->isMainWindowEvent(event))
                    return;

                if (isInputEvent(event.type) && m_callbacks.inputActivity)
                    m_callbacks.inputActivity();

                switch (event.type) {
                    case SDL_EVENT_WINDOW_MOVED:
                        if (m_callbacks.inputActivity) m_callbacks.inputActivity();
                        if (m_callbacks.moved) m_callbacks.moved(event.window.data1, event.window.data2);
                        break;
                    case SDL_EVENT_WINDOW_RESIZED:
                        if (m_callbacks.inputActivity) m_callbacks.inputActivity();
                        if (m_callbacks.resized) m_callbacks.resized(event.window.data1, event.window.data2);
                        break;
                    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                        if (m_callbacks.framebufferResized) m_callbacks.framebufferResized(event.window.data1, event.window.data2);
                        break;
                    case SDL_EVENT_WINDOW_FOCUS_GAINED:
                        if (m_callbacks.inputActivity) m_callbacks.inputActivity();
                        if (m_callbacks.focused) m_callbacks.focused(true);
                        break;
                    case SDL_EVENT_WINDOW_FOCUS_LOST:
                        if (m_callbacks.inputActivity) m_callbacks.inputActivity();
                        if (m_callbacks.focused) m_callbacks.focused(false);
                        break;
                    case SDL_EVENT_KEY_DOWN: {
                        const Keys key = translateKey(event.key.key, event.key.scancode, event.key.mod);
                        if (key != Keys::Invalid && m_callbacks.keyPressed)
                            m_callbacks.keyPressed(key);
                        break;
                    }
                    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                        m_shouldClose = true;
                        if (m_callbacks.inputActivity) m_callbacks.inputActivity();
                        if (m_callbacks.closeRequested) m_callbacks.closeRequested();
                        break;
                    case SDL_EVENT_DROP_FILE:
                        if (m_callbacks.inputActivity) m_callbacks.inputActivity();
                        if (event.drop.data != nullptr) {
                            const std::fs::path path(reinterpret_cast<const char8_t *>(event.drop.data));
                            SDL_free(const_cast<char *>(event.drop.data));
                            if (m_callbacks.fileDropped)
                                m_callbacks.fileDropped(path);
                        }
                        break;
                    case SDL_EVENT_WINDOW_EXPOSED:
                        if (m_callbacks.refreshRequested) m_callbacks.refreshRequested();
                        break;
                    default:
                        break;
                }
            }

            SDL_Window *m_window = nullptr;
            SDL_GLContext m_context = nullptr;
            SDL_WindowID m_windowId = 0;
            Callbacks m_callbacks;
            bool m_shouldClose = false;
            bool m_resizable = false;
            bool m_imguiInitialized = false;
            std::optional<SizeLimits> m_sizeLimits;
        };

    }

    bool initializeWindowing() {
        if (s_windowingReferences++ != 0)
            return true;

        SDL_SetHint(SDL_HINT_APP_ID, "imhex");
        #if defined(OS_LINUX)
            const char *waylandDisplay = std::getenv("WAYLAND_DISPLAY");
            if (waylandDisplay != nullptr && waylandDisplay[0] != '\0')
                SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
        #endif

        if (!SDL_Init(SDL_INIT_VIDEO)) {
            s_windowingReferences = 0;
            return false;
        }

        s_wakeEventType = SDL_RegisterEvents(1);
        if (s_wakeEventType == 0) {
            SDL_Quit();
            s_windowingReferences = 0;
            return false;
        }

        return true;
    }

    void shutdownWindowing() {
        if (s_windowingReferences == 0 || --s_windowingReferences != 0)
            return;

        s_wakeEventType = 0;
        SDL_Quit();
    }

    std::unique_ptr<ImHexApi::System::WindowBackend> createWindowBackend() {
        return std::make_unique<SDL3WindowBackend>();
    }

}
