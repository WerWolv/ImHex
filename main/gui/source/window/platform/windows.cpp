#include "window.hpp"

#if defined(OS_WINDOWS)

    #include "messaging.hpp"

    #include <hex/api/imhex_api/system.hpp>
    #include <hex/api/content_registry/settings.hpp>
    #include <hex/api/theme_manager.hpp>

    #include <hex/helpers/utils.hpp>
    #include <hex/helpers/logger.hpp>
    #include <hex/helpers/default_paths.hpp>

    #include <hex/api/events/events_gui.hpp>
    #include <hex/api/events/events_lifecycle.hpp>
    #include <hex/api/events/events_interaction.hpp>
    #include <hex/api/events/requests_gui.hpp>

    #include <imgui.h>
    #include <imgui_internal.h>

    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3.h>
    #include <GLFW/glfw3native.h>
    #undef GLFW_EXPOSE_NATIVE_WIN32

    #include <winbase.h>
    #include <winuser.h>
    #include <dwmapi.h>
    #include <windowsx.h>
    #include <shobjidl.h>
    #include <wrl/client.h>
    #include <fcntl.h>
    #include <shellapi.h>
    #include <timeapi.h>
    #include <VersionHelpers.h>
    #include <cstdio>

    #if !defined(STDIN_FILENO)
        #define STDIN_FILENO 0
    #endif

    #if !defined(STDOUT_FILENO)
        #define STDOUT_FILENO 1
    #endif

    #if !defined(STDERR_FILENO)
        #define STDERR_FILENO 2
    #endif

namespace hex {

    static LONG_PTR s_oldWndProc;
    static float s_titleBarHeight;
    static Microsoft::WRL::ComPtr<ITaskbarList4> s_taskbarList;
    static bool s_useLayeredWindow = true;

    // Custom Window procedure for receiving OS events
    static LRESULT commonWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_DPICHANGED: {
                int interfaceScaleSetting = int(hex::ContentRegistry::Settings::read<float>("hex.builtin.setting.interface", "hex.builtin.setting.interface.scaling_factor", 0.0F) * 10.0F);
                if (interfaceScaleSetting != 0)
                    break;

                const auto newScale = LOWORD(wParam) / 96.0F;
                const auto oldScale = ImHexApi::System::getNativeScale();

                if (u32(oldScale * 10) == u32(newScale * 10))
                    break;

                EventDPIChanged::post(oldScale, newScale);
                ImHexApi::System::impl::setNativeScale(newScale);

                ThemeManager::reapplyCurrentTheme();

                return TRUE;
            }
            case WM_COPYDATA: {
                // Handle opening files in existing instance

                auto message = reinterpret_cast<COPYDATASTRUCT *>(lParam);
                if (message == nullptr)
                    break;

                const auto data = reinterpret_cast<const u8*>(message->lpData);
                const auto size = message->cbData;
                EventNativeMessageReceived::post(std::vector<u8>(data, data + size));
                break;
            }
            case WM_SETTINGCHANGE: {
                // Handle Windows theme changes
                if (lParam == 0) break;

                if (reinterpret_cast<const WCHAR*>(lParam) == std::wstring_view(L"ImmersiveColorSet")) {
                    EventOSThemeChanged::post();
                }

                break;
            }
            case WM_SETCURSOR: {
                if (LOWORD(lParam) != HTCLIENT) {
                    return CallWindowProc(reinterpret_cast<WNDPROC>(s_oldWndProc), hwnd, uMsg, wParam, lParam);
                } else {
                    switch (ImGui::GetMouseCursor()) {
                        case ImGuiMouseCursor_Arrow:
                            SetCursor(LoadCursor(nullptr, IDC_ARROW));
                            break;
                        case ImGuiMouseCursor_Hand:
                            SetCursor(LoadCursor(nullptr, IDC_HAND));
                            break;
                        case ImGuiMouseCursor_ResizeEW:
                            SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                            break;
                        case ImGuiMouseCursor_ResizeNS:
                            SetCursor(LoadCursor(nullptr, IDC_SIZENS));
                            break;
                        case ImGuiMouseCursor_ResizeNWSE:
                            SetCursor(LoadCursor(nullptr, IDC_SIZENWSE));
                            break;
                        case ImGuiMouseCursor_ResizeNESW:
                            SetCursor(LoadCursor(nullptr, IDC_SIZENESW));
                            break;
                        case ImGuiMouseCursor_ResizeAll:
                            SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
                            break;
                        case ImGuiMouseCursor_NotAllowed:
                            SetCursor(LoadCursor(nullptr, IDC_NO));
                            break;
                        case ImGuiMouseCursor_TextInput:
                            SetCursor(LoadCursor(nullptr, IDC_IBEAM));
                            break;
                        default:
                            break;
                    }

                    return TRUE;
                }
            }
            default:
                break;
        }

        return CallWindowProc(reinterpret_cast<WNDPROC>(s_oldWndProc), hwnd, uMsg, wParam, lParam);
    }

    // Custom window procedure for borderless window
    static LRESULT borderlessWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_MOVE: {
                auto imhexWindow = static_cast<Window*>(glfwGetWindowUserPointer(ImHexApi::System::getMainWindowHandle()));
                imhexWindow->fullFrame();
                break;
            }
            case WM_MOUSELAST:
                break;
            case WM_NCACTIVATE:
            case WM_NCPAINT:
                // Handle Windows Aero Snap
                return DefWindowProcW(hwnd, uMsg, wParam, lParam);
            case WM_NCCALCSIZE: {
                // Handle window resizing

                RECT &rect  = *reinterpret_cast<RECT *>(lParam);
                RECT client = rect;

                CallWindowProc(reinterpret_cast<WNDPROC>(s_oldWndProc), hwnd, uMsg, wParam, lParam);

                if (IsMaximized(hwnd)) {
                    WINDOWINFO windowInfo = { };
                    windowInfo.cbSize = sizeof(WINDOWINFO);
                    GetWindowInfo(hwnd, &windowInfo);

                    rect = RECT {
                        .left   = static_cast<LONG>(client.left + windowInfo.cyWindowBorders),
                        .top    = static_cast<LONG>(client.top + windowInfo.cyWindowBorders),
                        .right  = static_cast<LONG>(client.right - windowInfo.cyWindowBorders),
                        .bottom = static_cast<LONG>(client.bottom - windowInfo.cyWindowBorders) + 1
                    };
                } else {
                    rect = client;
                }

                // This code tries to avoid DWM flickering when resizing the window
                // It's not perfect, but it's really the best we can do.

                LARGE_INTEGER performanceFrequency = {};
                QueryPerformanceFrequency(&performanceFrequency);
                TIMECAPS tc = {};
                timeGetDevCaps(&tc, sizeof(tc));

                const auto granularity = tc.wPeriodMin;
                timeBeginPeriod(tc.wPeriodMin);

                DWM_TIMING_INFO dti = {};
                dti.cbSize = sizeof(dti);
                ::DwmGetCompositionTimingInfo(nullptr, &dti);

                LARGE_INTEGER end = {};
                QueryPerformanceCounter(&end);

                const auto period = dti.qpcRefreshPeriod;
                const i64 delta = dti.qpcVBlank - end.QuadPart;

                i64 sleepTicks = 0;
                i64 sleepMilliSeconds = 0;
                if (period > 0) {
                    if (delta >= 0) {
                        sleepTicks = delta / period;
                    } else {
                        sleepTicks = -1 + delta / period;
                    }

                    sleepMilliSeconds = delta - (period * sleepTicks);
                    const double sleepTime = std::round(1000.0 * double(sleepMilliSeconds) / double(performanceFrequency.QuadPart));
                    if (sleepTime >= 0.0) {
                        Sleep(DWORD(sleepTime));
                    }
                }

                timeEndPeriod(granularity);

                return WVR_REDRAW;
            }
            case WM_ERASEBKGND:
                return 1;
            case WM_WINDOWPOSCHANGING: {
                // Make sure that windows discards the entire client area when resizing to avoid flickering
                const auto windowPos = reinterpret_cast<LPWINDOWPOS>(lParam);
                windowPos->flags |= SWP_NOCOPYBITS;
                break;
            }
            case WM_NCHITTEST: {
                // Handle window resizing and moving

                POINT cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

                const POINT border {
                    static_cast<LONG>((::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)) * ImHexApi::System::getGlobalScale()),
                    static_cast<LONG>((::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)) * ImHexApi::System::getGlobalScale())
                };

                if (glfwGetWindowMonitor(ImHexApi::System::getMainWindowHandle()) != nullptr) {
                    return HTCLIENT;
                }

                RECT window;
                if (!::GetWindowRect(hwnd, &window)) {
                    return HTNOWHERE;
                }

                constexpr static auto RegionClient = 0b0000;
                constexpr static auto RegionLeft   = 0b0001;
                constexpr static auto RegionRight  = 0b0010;
                constexpr static auto RegionTop    = 0b0100;
                constexpr static auto RegionBottom = 0b1000;

                const auto result =
                    RegionLeft * (cursor.x < (window.left + border.x)) |
                    RegionRight * (cursor.x >= (window.right - border.x)) |
                    RegionTop * (cursor.y < (window.top + border.y)) |
                    RegionBottom * (cursor.y >= (window.bottom - border.y));

                // If the mouse is hovering over any button, disable resize controls.
                // Without this, the window buttons and menu bar would not be clickable
                // correctly at the edges of the window.
                if (result != 0 && (ImGui::IsAnyItemHovered())) {
                    break;
                }

                if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
                    if (result == RegionClient)
                        return HTCLIENT;
                    else
                        return HTCAPTION;
                }

                std::string_view hoveredWindowName = ImGui::GetCurrentContext()->HoveredWindow == nullptr ? "" : GImGui->HoveredWindow->Name;

                if (!ImHexApi::System::impl::isWindowResizable()) {
                    if (result != RegionClient) {
                        return HTCAPTION;
                    }
                }

                switch (result) {
                    case RegionLeft:
                        return HTLEFT;
                    case RegionRight:
                        return HTRIGHT;
                    case RegionTop:
                        return HTTOP;
                    case RegionBottom:
                        return HTBOTTOM;
                    case RegionTop | RegionLeft:
                        return HTTOPLEFT;
                    case RegionTop | RegionRight:
                        return HTTOPRIGHT;
                    case RegionBottom | RegionLeft:
                        return HTBOTTOMLEFT;
                    case RegionBottom | RegionRight:
                        return HTBOTTOMRIGHT;
                    case RegionClient:
                    default:
                        if (cursor.y < (window.top + s_titleBarHeight * 2)) {
                            if (hoveredWindowName == "##MainMenuBar" || hoveredWindowName == "ImHexDockSpace") {
                                if (!ImGui::IsAnyItemHovered()) {
                                    return HTCAPTION;
                                }
                            }
                        }

                        break;
                }

                break;
            }
            default:
                break;
        }

        return commonWindowProc(hwnd, uMsg, wParam, lParam);
    }


    [[maybe_unused]]
    static void reopenConsoleHandle(u32 stdHandleNumber, i32 stdFileDescriptor, FILE *stdStream) {
        // Get the Windows handle for the standard stream
        HANDLE handle = ::GetStdHandle(stdHandleNumber);

        // Redirect the standard stream to the relevant console stream
        if (stdFileDescriptor == STDIN_FILENO)
            freopen("CONIN$", "rt", stdStream);
        else
            freopen("CONOUT$", "wt", stdStream);

        // Disable buffering
        setvbuf(stdStream, nullptr, _IONBF, 0);

        // Reopen the standard stream as a file descriptor
        auto unboundFd = [stdFileDescriptor, handle]{
            if (stdFileDescriptor == STDIN_FILENO)
                return _open_osfhandle(intptr_t(handle), _O_RDONLY);
            else
                return _open_osfhandle(intptr_t(handle), _O_WRONLY);
        }();

        // Duplicate the file descriptor to the standard stream
        dup2(unboundFd, stdFileDescriptor);
    }

    void enumerateFonts() {
        constexpr static auto FontRegistryPath = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";

        static const std::array RegistryLocations = {
            HKEY_LOCAL_MACHINE,
            HKEY_CURRENT_USER
        };

        for (const auto location : RegistryLocations) {
            HKEY key;
            if (RegOpenKeyExW(location, FontRegistryPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
                continue;
            }

            DWORD index = 0;
            std::wstring valueName(0xFFF, L'\0');
            DWORD valueNameSize = valueName.size() * sizeof(wchar_t);
            std::wstring valueData(0xFFF, L'\0');
            DWORD valueDataSize = valueData.size() * sizeof(wchar_t);
            DWORD valueType;

            while (RegEnumValueW(key, index, valueName.data(), &valueNameSize, nullptr, &valueType, reinterpret_cast<BYTE *>(valueData.data()), &valueDataSize) == ERROR_SUCCESS) {
                if (valueType == REG_SZ) {
                    auto fontName = hex::utf16ToUtf8(valueName.c_str());
                    auto fontPath = std::fs::path(valueData);
                    if (fontPath.is_relative())
                        fontPath = std::fs::path("C:\\Windows\\Fonts") / fontPath;

                    // Windows appends (TrueType) to all font names for some reason. Remove it
                    if (fontName.ends_with(" (TrueType)")) {
                        fontName = fontName.substr(0, fontName.size() - 11);
                    }

                    registerFont(fontName.c_str(), wolv::util::toUTF8String(fontPath).c_str());
                }

                valueNameSize = valueName.size();
                valueDataSize = valueData.size();
                index++;
            }

            RegCloseKey(key);
        }
    }

    void Window::configureGLFW() {
        if (ImHexApi::System::getGLVersion() >= SemanticVersion(4,1,0)) {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        } else {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        }
        glfwWindowHint(GLFW_DECORATED, ImHexApi::System::isBorderlessWindowModeEnabled() ? GL_FALSE : GL_TRUE);

        // Windows versions before Windows 10 have issues with transparent framebuffers
        // causing the entire window to be slightly transparent ignoring all configurations
        if (::IsWindows10OrGreater()) {
            glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        } else {
            glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);
        }
    }


    void Window::initNative() {
        if (ImHexApi::System::isDebugBuild()) {
            // If the application is running in debug mode, ImHex runs under the CONSOLE subsystem,
            // so we don't need to do anything besides enabling ANSI colors
            log::impl::enableColorPrinting();
        } else if (hex::getEnvironmentVariable("__IMHEX_FORWARD_CONSOLE__") == "1") {
            // Check for the __IMHEX_FORWARD_CONSOLE__ environment variable that was set by the forwarder application

            // Enable ANSI colors in the console
            log::impl::enableColorPrinting();
        } else {
            log::impl::redirectToFile();
        }

        // Add plugin library folders to dll search path
        for (const auto &path : paths::Libraries.read())  {
            if (std::fs::exists(path))
                AddDllDirectory(path.c_str());
        }

        enumerateFonts();
    }

    class DropManager : public IDropTarget {
    public:
        DropManager() = default;
        virtual ~DropManager() = default;

        ULONG STDMETHODCALLTYPE AddRef()  override { return 1; }
        ULONG STDMETHODCALLTYPE Release() override { return 0; }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override {
            if (riid == IID_IDropTarget) {
                *ppvObject = this;

                return S_OK;
            }

            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }

        HRESULT STDMETHODCALLTYPE DragEnter(
            IDataObject *,
            DWORD,
            POINTL,
            DWORD *pdwEffect) override
        {
            EventFileDragged::post(true);

            *pdwEffect = DROPEFFECT_COPY;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE DragOver(
            DWORD,
            POINTL,
            DWORD *pdwEffect) override
        {
            *pdwEffect = DROPEFFECT_COPY;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE DragLeave() override
        {
            EventFileDragged::post(false);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Drop(
            IDataObject *pDataObj,
            DWORD,
            POINTL,
            DWORD *pdwEffect) override
        {
            FORMATETC fmte = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
            STGMEDIUM stgm;

            if (SUCCEEDED(pDataObj->GetData(&fmte, &stgm))) {
                auto hdrop = HDROP(stgm.hGlobal);
                auto fileCount = DragQueryFile(hdrop, 0xFFFFFFFF, nullptr, 0);

                for (UINT i = 0; i < fileCount; i++) {
                    WCHAR szFile[MAX_PATH];
                    UINT cch = DragQueryFileW(hdrop, i, szFile, MAX_PATH);
                    if (cch > 0 && cch < MAX_PATH) {
                        EventFileDropped::post(szFile);
                    }
                }

                ReleaseStgMedium(&stgm);
            }

            EventFileDragged::post(false);

            *pdwEffect &= DROPEFFECT_COPY;
            return S_OK;
        }
    };

    void Window::setupNativeWindow() {
        // Setup borderless window
        auto hwnd = glfwGetWin32Window(m_window);

        CoInitialize(nullptr);
        OleInitialize(nullptr);

        static DropManager dm;
        if (RegisterDragDrop(hwnd, &dm) != S_OK) {
            log::warn("Failed to register drop target");

            // Register fallback drop target using glfw
            glfwSetDropCallback(m_window, [](GLFWwindow *, int count, const char **paths) {
                for (int i = 0; i < count; i++) {
                    EventFileDropped::post(reinterpret_cast<const char8_t *>(paths[i]));
                }
            });
            EventFileDropped::subscribe([this] {
                this->unlockFrameRate();
            });
        }

        bool borderlessWindowMode = ImHexApi::System::isBorderlessWindowModeEnabled();

        // Set up the correct window procedure based on the borderless window mode state
        if (borderlessWindowMode) {
            s_oldWndProc = ::SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(borderlessWindowProc));

            MARGINS borderless = { 1, 1, 1, 1 };
            ::DwmExtendFrameIntoClientArea(hwnd, &borderless);

            DWORD attribute = DWMNCRP_ENABLED;
            ::DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &attribute, sizeof(attribute));

            ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE);
        } else {
            s_oldWndProc = ::SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(commonWindowProc));
        }

        // Set up a taskbar progress handler
        {
            if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
                CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskbarList4, &s_taskbarList);
            }

            EventSetTaskBarIconState::subscribe([hwnd](u32 state, u32 type, u32 progress){
                using enum ImHexApi::System::TaskProgressState;
                switch (ImHexApi::System::TaskProgressState(state)) {
                    case Reset:
                        s_taskbarList->SetProgressState(hwnd, TBPF_NOPROGRESS);
                        s_taskbarList->SetProgressValue(hwnd, 0, 0);
                        break;
                    case Flash:
                        FlashWindow(hwnd, true);
                        break;
                    case Progress:
                        s_taskbarList->SetProgressState(hwnd, TBPF_INDETERMINATE);
                        s_taskbarList->SetProgressValue(hwnd, progress, 100);
                        break;
                }

                using enum ImHexApi::System::TaskProgressType;
                switch (ImHexApi::System::TaskProgressType(type)) {
                    case Normal:
                        s_taskbarList->SetProgressState(hwnd, TBPF_NORMAL);
                        break;
                    case Warning:
                        s_taskbarList->SetProgressState(hwnd, TBPF_PAUSED);
                        break;
                    case Error:
                        s_taskbarList->SetProgressState(hwnd, TBPF_ERROR);
                        break;
                }
            });
        }

        struct ACCENTPOLICY {
            u32 accentState;
            u32 accentFlags;
            u32 gradientColor;
            u32 animationId;
        };
        struct WINCOMPATTRDATA {
            int attribute;
            PVOID pData;
            ULONG dataSize;
        };

        EventThemeChanged::subscribe([this]{
            auto hwnd = glfwGetWin32Window(m_window);

            static auto user32Dll = LoadLibraryA("user32.dll");
            if (user32Dll != nullptr) {
                using SetWindowCompositionAttributeFunc = BOOL(WINAPI*)(HWND, WINCOMPATTRDATA*);

                const auto setWindowCompositionAttribute =
                    reinterpret_cast<SetWindowCompositionAttributeFunc>(
                        reinterpret_cast<void*>(
                            GetProcAddress(user32Dll, "SetWindowCompositionAttribute")
                        )
                    );

                if (setWindowCompositionAttribute != nullptr) {
                    ACCENTPOLICY policy = { ImGuiExt::GetCustomStyle().WindowBlur > 0.5F ? 4U : 0U, 0, ImGuiExt::GetCustomColorU32(ImGuiCustomCol_BlurBackground), 0 };
                    WINCOMPATTRDATA data = { 19, &policy, sizeof(ACCENTPOLICY) };
                    setWindowCompositionAttribute(hwnd, &data);
                }
            }
        });
        RequestChangeTheme::subscribe([this](const std::string &theme) {
            auto hwnd = glfwGetWin32Window(m_window);

            BOOL value = theme == "Dark" ? TRUE : FALSE;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
        });

        ImGui::GetIO().ConfigDebugIsDebuggerPresent = ::IsDebuggerPresent();

        glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
            auto *win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->unlockFrameRate();

            glViewport(0, 0, width, height);
            ImHexApi::System::impl::setMainWindowSize(width, height);
        });

        DwmEnableMMCSS(TRUE);
        
        {
            constexpr BOOL value = TRUE;
            DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_ENABLED, &value, sizeof(value));
        }
        {
            constexpr DWMNCRENDERINGPOLICY value = DWMNCRP_ENABLED;
            DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &value, sizeof(value));
        }

        glfwSetWindowRefreshCallback(m_window, [](GLFWwindow *window) {
            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));

            win->fullFrame();
            DwmFlush();
        });

        // AMD GPUs seem to have issues with Layered Window rendering. Until we figure out
        // why that is or AMD fixes the issue on their side, disable it on these GPUs.
        s_useLayeredWindow = ImHexApi::System::getGPUVendor() != "ATI Technologies Inc.";
    }

    void Window::beginNativeWindowFrame() {
        s_titleBarHeight = ImGui::GetCurrentWindowRead()->MenuBarHeight;

        // Remove WS_POPUP style from the window to make various window management tools work
        auto hwnd = glfwGetWin32Window(m_window);
        {
            auto style = GetWindowLong(hwnd, GWL_STYLE);
            style |= WS_OVERLAPPEDWINDOW;
            style &= ~WS_POPUP;

            ::SetWindowLong(hwnd, GWL_STYLE, style);
        }

        // Make window composited and layered when supported to eradicate any window flickering that happens while resizing
        {
            auto style = GetWindowLong(hwnd, GWL_EXSTYLE);
            style |= WS_EX_COMPOSITED;

            if (s_useLayeredWindow)
                style |= WS_EX_LAYERED;

            ::SetWindowLong(hwnd, GWL_EXSTYLE, style);
        }

        if (!ImHexApi::System::impl::isWindowResizable()) {
            if (glfwGetWindowAttrib(m_window, GLFW_MAXIMIZED) == GLFW_TRUE) {
                glfwRestoreWindow(m_window);
            }
        }

    }

    void Window::endNativeWindowFrame() {

    }

}

#endif