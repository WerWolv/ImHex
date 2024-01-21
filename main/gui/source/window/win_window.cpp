#include "window.hpp"

#include "messaging.hpp"

#if defined(OS_WINDOWS)

    #include <hex/helpers/utils.hpp>
    #include <hex/helpers/logger.hpp>

    #include <imgui.h>
    #include <imgui_internal.h>

    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
    #undef GLFW_EXPOSE_NATIVE_WIN32

    #include <winbase.h>
    #include <winuser.h>
    #include <dwmapi.h>
    #include <windowsx.h>
    #include <shobjidl.h>
    #include <wrl/client.h>
    #include <fcntl.h>
    #include <oleidl.h>

    #include <cstdio>

namespace hex {

    template<typename T>
    using WinUniquePtr = std::unique_ptr<std::remove_pointer_t<T>, BOOL(*)(T)>;

    static LONG_PTR s_oldWndProc;
    static float s_titleBarHeight;
    static Microsoft::WRL::ComPtr<ITaskbarList4> s_taskbarList;

    void nativeErrorMessage(const std::string &message) {
        log::fatal(message);
        MessageBox(nullptr, message.c_str(), "Error", MB_ICONERROR | MB_OK);
    }

    // Custom Window procedure for receiving OS events
    static LRESULT commonWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_COPYDATA: {
                // Handle opening files in existing instance

                auto message = reinterpret_cast<COPYDATASTRUCT *>(lParam);
                if (message == nullptr) break;

                ssize_t nullIndex = -1;

                auto messageData = static_cast<const char*>(message->lpData);
                size_t messageSize = message->cbData;

                for (size_t i = 0; i < messageSize; i++) {
                    if (messageData[i] == '\0') {
                        nullIndex = i;
                        break;
                    }
                }

                if (nullIndex == -1) {
                    log::warn("Received invalid forwarded event");
                    break;
                }

                std::string eventName(messageData, nullIndex);

                std::vector<u8> eventData(messageData + nullIndex + 1, messageData + messageSize);

                hex::messaging::messageReceived(eventName, eventData);
                break;
            }
            case WM_SETTINGCHANGE: {
                // Handle Windows theme changes
                if (lParam == 0) break;

                if (LPCTSTR(lParam) == std::string_view("ImmersiveColorSet")) {
                    EventOSThemeChanged::post();
                }

                break;
            }
            case WM_SETCURSOR: {
                if (LOWORD(lParam) != HTCLIENT) {
                    return CallWindowProc((WNDPROC)s_oldWndProc, hwnd, uMsg, wParam, lParam);
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
                    }

                    return TRUE;
                }
            }
            default:
                break;
        }

        return CallWindowProc((WNDPROC)s_oldWndProc, hwnd, uMsg, wParam, lParam);
    }

    // Custom window procedure for borderless window
    static LRESULT borderlessWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
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

                CallWindowProc((WNDPROC)s_oldWndProc, hwnd, uMsg, wParam, lParam);

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

                return 0;
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

                if (result != 0 && (ImGui::IsAnyItemHovered() || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))) {
                    break;
                }

                std::string_view hoveredWindowName = GImGui->HoveredWindow == nullptr ? "" : GImGui->HoveredWindow->Name;

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


    void Window::initNative() {
        if (ImHexApi::System::isDebugBuild()) {
            // If the application is running in debug mode, ImHex runs under the CONSOLE subsystem,
            // so we don't need to do anything besides enabling ANSI colors
            log::impl::enableColorPrinting();
        } else if (hex::getEnvironmentVariable("__IMHEX_FORWARD_CONSOLE__") == "1") {
            // Check for the __IMHEX_FORWARD_CONSOLE__ environment variable that was set by the forwarder application

            // If it's present, attach to its console window
            ::AttachConsole(ATTACH_PARENT_PROCESS);

            // Reopen stdin, stdout and stderr to the console if not in debug mode
            reopenConsoleHandle(STD_INPUT_HANDLE,  STDIN_FILENO,  stdin);
            reopenConsoleHandle(STD_OUTPUT_HANDLE, STDOUT_FILENO, stdout);

            // Explicitly don't forward stderr because some libraries like to write to it
            // with no way to disable it (e.g., libmagic)
            /*
                reopenConsoleHandle(STD_ERROR_HANDLE,  STDERR_FILENO, stderr);
            */

            // Enable ANSI colors in the console
            log::impl::enableColorPrinting();
        } else {
            log::impl::redirectToFile();
        }

        ImHexApi::System::impl::setBorderlessWindowMode(true);

        // Add plugin library folders to dll search path
        for (const auto &path : hex::fs::getDefaultPaths(fs::ImHexPath::Libraries))  {
            if (std::fs::exists(path))
                AddDllDirectory(path.c_str());
        }
    }

    class DropManager : public IDropTarget {
    public:
        DropManager() = default;
        virtual ~DropManager() = default;

        ULONG AddRef()  override { return 1; }
        ULONG Release() override { return 0; }

        HRESULT QueryInterface(REFIID riid, void **ppvObject) override {
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

            *pdwEffect = DROPEFFECT_NONE;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE DragOver(
            DWORD,
            POINTL,
            DWORD *pdwEffect) override
        {
            *pdwEffect = DROPEFFECT_NONE;
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

            *pdwEffect &= DROPEFFECT_NONE;
            return S_OK;

            return S_OK;
        }
    };

    void Window::setupNativeWindow() {
        // Setup borderless window
        auto hwnd = glfwGetWin32Window(m_window);

        OleInitialize(nullptr);

        static DropManager dm;
        RegisterDragDrop(hwnd, &dm);

        bool borderlessWindowMode = ImHexApi::System::isBorderlessWindowModeEnabled();

        // Set up the correct window procedure based on the borderless window mode state
        if (borderlessWindowMode) {
            s_oldWndProc = ::SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)borderlessWindowProc);

            MARGINS borderless = { 1, 1, 1, 1 };
            ::DwmExtendFrameIntoClientArea(hwnd, &borderless);

            DWORD attribute = DWMNCRP_ENABLED;
            ::DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &attribute, sizeof(attribute));

            ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE);
        } else {
            s_oldWndProc = ::SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)commonWindowProc);
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

            static auto user32Dll = WinUniquePtr<HMODULE>(LoadLibraryA("user32.dll"), FreeLibrary);
            if (user32Dll != nullptr) {
                using SetWindowCompositionAttributeFunc = BOOL(WINAPI*)(HWND, WINCOMPATTRDATA*);

                const auto SetWindowCompositionAttribute =
                        (SetWindowCompositionAttributeFunc)
                        (void*)
                        GetProcAddress(user32Dll.get(), "SetWindowCompositionAttribute");

                if (SetWindowCompositionAttribute != nullptr) {
                    ACCENTPOLICY policy = { ImGuiExt::GetCustomStyle().WindowBlur > 0.5F ? 4U : 0U, 0, ImGuiExt::GetCustomColorU32(ImGuiCustomCol_BlurBackground), 0 };
                    WINCOMPATTRDATA data = { 19, &policy, sizeof(ACCENTPOLICY) };
                    SetWindowCompositionAttribute(hwnd, &data);
                }
            }
        });

    }

    void Window::beginNativeWindowFrame() {
        s_titleBarHeight = ImGui::GetCurrentWindowRead()->MenuBarHeight();

        // Remove WS_POPUP style from the window to make various window management tools work
        auto hwnd = glfwGetWin32Window(m_window);
        ::SetWindowLong(hwnd, GWL_STYLE, (GetWindowLong(hwnd, GWL_STYLE) | WS_OVERLAPPEDWINDOW) & ~WS_POPUP);

        if (!ImHexApi::System::impl::isWindowResizable()) {
            if (glfwGetWindowAttrib(m_window, GLFW_MAXIMIZED)) {
                glfwRestoreWindow(m_window);
            }
        }

    }

    void Window::endNativeWindowFrame() {
        if (!ImHexApi::System::isBorderlessWindowModeEnabled())
            return;
    }

}

#endif