#include "window.hpp"

#include "messaging.hpp"

#include <hex/api/content_registry.hpp>

#if defined(OS_WINDOWS)

    #include <hex/helpers/utils.hpp>
    #include <hex/helpers/logger.hpp>

    #include <imgui.h>
    #include <imgui_internal.h>
    #include <fonts/codicons_font.h>

    #include <GLFW/glfw3.h>
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
    #undef GLFW_EXPOSE_NATIVE_WIN32

    #include <winbase.h>
    #include <winuser.h>
    #include <dwmapi.h>
    #include <windowsx.h>
    #include <shobjidl.h>
    #include <wrl/client.h>

    #include <csignal>
    #include <cstdio>

    #include <imgui_impl_glfw.h>

namespace hex {

    template<typename T>
    using WinUniquePtr = std::unique_ptr<std::remove_pointer_t<T>, BOOL(*)(T)>;

    static LONG_PTR g_oldWndProc;
    static float g_titleBarHeight;
    static Microsoft::WRL::ComPtr<ITaskbarList4> g_taskbarList;

    void nativeErrorMessage(const std::string &message) {
        log::fatal(message);
        MessageBox(NULL, message.c_str(), "Error", MB_ICONERROR | MB_OK);
    }

    // Custom Window procedure for receiving OS events
    static LRESULT commonWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_COPYDATA: {
                // Handle opening files in existing instance

                auto message = reinterpret_cast<COPYDATASTRUCT *>(lParam);
                if (message == nullptr) break;

                int nullIndex = -1;

                char* messageData = reinterpret_cast<char*>(message->lpData);
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

                std::string evtName(messageData, nullIndex);

                std::vector<u8> evtData;
                for (size_t i=0; i<(messageSize-nullIndex)-1; i++) {
                    u8 b = *reinterpret_cast<u8*>(messageData+nullIndex+i+1);
                    evtData.push_back(b);
                }
                
                hex::messaging::messageReceived(evtName, evtData);
                break;
            }
            case WM_SETTINGCHANGE: {
                // Handle Windows theme changes
                if (lParam == 0) break;

                if (LPCTSTR(lParam) == std::string_view("ImmersiveColorSet")) {
                    EventManager::post<EventOSThemeChanged>();
                }

                break;
            }
            case WM_SETCURSOR: {
                if (LOWORD(lParam) != HTCLIENT) {
                    return CallWindowProc((WNDPROC)g_oldWndProc, hwnd, uMsg, wParam, lParam);
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

        return CallWindowProc((WNDPROC)g_oldWndProc, hwnd, uMsg, wParam, lParam);
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

                CallWindowProc((WNDPROC)g_oldWndProc, hwnd, uMsg, wParam, lParam);

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
                        if ((cursor.y < (window.top + g_titleBarHeight * 2)) && !(ImGui::IsAnyItemHovered() || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)))
                            return HTCAPTION;
                        else break;
                }
                break;
            }
            default:
                break;
        }

        return commonWindowProc(hwnd, uMsg, wParam, lParam);
    }


    void Window::initNative() {
        ImHexApi::System::impl::setBorderlessWindowMode(true);

        // Add plugin library folders to dll search path
        for (const auto &path : hex::fs::getDefaultPaths(fs::ImHexPath::Libraries))  {
            if (std::fs::exists(path))
                AddDllDirectory(path.c_str());
        }

        // Various libraries sadly directly print to stderr with no way to disable it
        // We redirect stderr to NUL to prevent this
        freopen("NUL:", "w", stderr);
        setvbuf(stderr, nullptr, _IONBF, 0);

        // Attach to parent console if one exists
        bool result = AttachConsole(ATTACH_PARENT_PROCESS) == TRUE;

        #if defined(DEBUG)
            if (::GetLastError() == ERROR_INVALID_HANDLE) {
                result = AllocConsole() == TRUE;
            }
        #endif

        if (result) {
            // Redirect stdin and stdout to that new console
            freopen("CONIN$", "r", stdin);
            freopen("CONOUT$", "w", stdout);
            setvbuf(stdin, nullptr, _IONBF, 0);
            setvbuf(stdout, nullptr, _IONBF, 0);

            fmt::print("\n");

            // Enable color format specifiers in console
            {
                auto hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
                if (hConsole != INVALID_HANDLE_VALUE) {
                    DWORD mode = 0;
                    if (::GetConsoleMode(hConsole, &mode) == TRUE) {
                        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
                        ::SetConsoleMode(hConsole, mode);
                    }
                }
            }
        } else {
            log::impl::redirectToFile();
        }
    }

    void Window::setupNativeWindow() {
        // Setup borderless window
        auto hwnd = glfwGetWin32Window(this->m_window);

        bool borderlessWindowMode = ImHexApi::System::isBorderlessWindowModeEnabled();

        // Set up the correct window procedure based on the borderless window mode state
        if (borderlessWindowMode) {
            g_oldWndProc = ::SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)borderlessWindowProc);

            MARGINS borderless = { 1, 1, 1, 1 };
            ::DwmExtendFrameIntoClientArea(hwnd, &borderless);

            DWORD attribute = DWMNCRP_ENABLED;
            ::DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &attribute, sizeof(attribute));

            ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE);
            ::SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) | WS_OVERLAPPEDWINDOW);
        } else {
            g_oldWndProc = ::SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)commonWindowProc);
        }

        // Add a custom exception handler to detect heap corruptions
        {
            ::AddVectoredExceptionHandler(TRUE, [](PEXCEPTION_POINTERS exception) -> LONG {
                if ((exception->ExceptionRecord->ExceptionCode & 0xF000'0000) == 0xC000'0000) {
                    log::fatal("Exception raised: 0x{:08X}", exception->ExceptionRecord->ExceptionCode);
                    if (exception->ExceptionRecord->ExceptionCode == STATUS_HEAP_CORRUPTION) {
                        log::fatal("Heap corruption detected!");
                    }
                }

                return EXCEPTION_CONTINUE_SEARCH;
            });
        }

        // Set up a taskbar progress handler
        {
            if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
                CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskbarList4, &g_taskbarList);
            }

            EventManager::subscribe<EventSetTaskBarIconState>([hwnd](u32 state, u32 type, u32 progress){
                using enum ImHexApi::System::TaskProgressState;
                switch (ImHexApi::System::TaskProgressState(state)) {
                    case Reset:
                        g_taskbarList->SetProgressState(hwnd, TBPF_NOPROGRESS);
                        g_taskbarList->SetProgressValue(hwnd, 0, 0);
                        break;
                    case Flash:
                        FlashWindow(hwnd, true);
                        break;
                    case Progress:
                        g_taskbarList->SetProgressState(hwnd, TBPF_INDETERMINATE);
                        g_taskbarList->SetProgressValue(hwnd, progress, 100);
                        break;
                }

                using enum ImHexApi::System::TaskProgressType;
                switch (ImHexApi::System::TaskProgressType(type)) {
                    case Normal:
                        g_taskbarList->SetProgressState(hwnd, TBPF_NORMAL);
                        break;
                    case Warning:
                        g_taskbarList->SetProgressState(hwnd, TBPF_PAUSED);
                        break;
                    case Error:
                        g_taskbarList->SetProgressState(hwnd, TBPF_ERROR);
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

        EventManager::subscribe<EventThemeChanged>([this]{
            auto hwnd = glfwGetWin32Window(this->m_window);

            static auto user32Dll = WinUniquePtr<HMODULE>(LoadLibraryA("user32.dll"), FreeLibrary);
            if (user32Dll != nullptr) {
                using SetWindowCompositionAttributeFunc = BOOL(WINAPI*)(HWND, WINCOMPATTRDATA*);

                const auto SetWindowCompositionAttribute =
                        (SetWindowCompositionAttributeFunc)
                        (void*)
                        GetProcAddress(user32Dll.get(), "SetWindowCompositionAttribute");

                if (SetWindowCompositionAttribute != nullptr) {
                    ACCENTPOLICY policy = { ImGui::GetCustomStyle().WindowBlur > 0.5F ? 4U : 0U, 0, ImGui::GetCustomColorU32(ImGuiCustomCol_BlurBackground), 0 };
                    WINCOMPATTRDATA data = { 19, &policy, sizeof(ACCENTPOLICY) };
                    SetWindowCompositionAttribute(hwnd, &data);
                }
            }
        });

    }

    void Window::beginNativeWindowFrame() {
        g_titleBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight();
    }

    void Window::endNativeWindowFrame() {
        if (!ImHexApi::System::isBorderlessWindowModeEnabled())
            return;
    }

}

#endif