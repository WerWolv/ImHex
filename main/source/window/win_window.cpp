#include "window.hpp"

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

    static LONG_PTR g_oldWndProc;
    static float g_titleBarHeight;
    static ImGuiMouseCursor g_mouseCursorIcon;
    static Microsoft::WRL::ComPtr<ITaskbarList4> g_taskbarList;

    // Custom Window procedure for receiving OS events
    static LRESULT commonWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_COPYDATA: {
                // Handle opening files in existing instance

                auto message = reinterpret_cast<COPYDATASTRUCT *>(lParam);
                if (message == nullptr) break;

                auto data = reinterpret_cast<const char8_t *>(message->lpData);
                if (data == nullptr) break;

                std::fs::path path = data;
                log::info("Opening file in existing instance: {}", wolv::util::toUTF8String(path));
                EventManager::post<RequestOpenFile>(path);
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
            default:
                break;
        }

        return CallWindowProc((WNDPROC)g_oldWndProc, hwnd, uMsg, wParam, lParam);
    }

    // Custom window procedure for borderless window
    static LRESULT borderlessWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
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
            case WM_SETCURSOR: {
                // Handle mouse cursor icon
                auto cursorPos = LOWORD(lParam);

                switch (cursorPos) {
                    case HTRIGHT:
                    case HTLEFT:
                        g_mouseCursorIcon = ImGuiMouseCursor_ResizeEW;
                        break;
                    case HTTOP:
                    case HTBOTTOM:
                        g_mouseCursorIcon = ImGuiMouseCursor_ResizeNS;
                        break;
                    case HTTOPLEFT:
                    case HTBOTTOMRIGHT:
                        g_mouseCursorIcon = ImGuiMouseCursor_ResizeNWSE;
                        break;
                    case HTTOPRIGHT:
                    case HTBOTTOMLEFT:
                        g_mouseCursorIcon = ImGuiMouseCursor_ResizeNESW;
                        break;
                    case HTCAPTION:
                    case HTCLIENT:
                        g_mouseCursorIcon = ImGuiMouseCursor_None;
                        break;
                    default:
                        break;
                }

                return TRUE;
            }
            case WM_NCHITTEST: {
                // Handle window resizing and moving

                POINT cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

                const POINT border {
                    static_cast<LONG>((::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)) * ImHexApi::System::getGlobalScale() / 1.5F),
                    static_cast<LONG>((::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)) * ImHexApi::System::getGlobalScale() / 1.5F)
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

                if (result != 0 && (ImGui::IsItemHovered() || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)))
                    break;

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
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
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
                    if (::GetConsoleMode(hConsole, &mode)) {
                        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT;
                        ::SetConsoleMode(hConsole, mode);
                    }
                }
            }
        } else {
            log::redirectToFile();
        }

        // Open new files in already existing ImHex instance
        constexpr static auto UniqueMutexId = "ImHex/a477ea68-e334-4d07-a439-4f159c683763";

        HANDLE globalMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, UniqueMutexId);
        if (!globalMutex) {
            // If no ImHex instance is running, create a new global mutex
            globalMutex = CreateMutex(nullptr, FALSE, UniqueMutexId);
        } else {
            // If an ImHex instance is already running, send the file path to it and exit

            if (ImHexApi::System::getProgramArguments().argc > 1) {
                // Find the ImHex Window and send the file path as a message to it
                ::EnumWindows([](HWND hWnd, LPARAM) -> BOOL {
                    auto &programArgs = ImHexApi::System::getProgramArguments();

                    // Get the window name
                    auto length = ::GetWindowTextLength(hWnd);
                    std::string windowName(length + 1, '\x00');
                    ::GetWindowText(hWnd, windowName.data(), windowName.size());

                    // Check if the window is visible and if it's an ImHex window
                    if (::IsWindowVisible(hWnd) && length != 0) {
                        if (windowName.starts_with("ImHex")) {
                            // Create the message
                            COPYDATASTRUCT message = {
                                .dwData = 0,
                                .cbData = static_cast<DWORD>(std::strlen(programArgs.argv[1])) + 1,
                                .lpData = programArgs.argv[1]
                            };

                            // Send the message
                            SendMessage(hWnd, WM_COPYDATA, reinterpret_cast<WPARAM>(hWnd), reinterpret_cast<LPARAM>(&message));

                            return FALSE;
                        }
                    }

                    return TRUE;
                }, 0);

                std::exit(0);
            }
        }
    }

    void Window::setupNativeWindow() {
        // Setup borderless window
        auto hwnd = glfwGetWin32Window(this->m_window);

        bool borderlessWindowMode = ImHexApi::System::isBorderlessWindowModeEnabled();

        ImGui_ImplGlfw_SetBorderlessWindowMode(borderlessWindowMode);

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
                        std::raise(SIGABRT);
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
    }

    void Window::beginNativeWindowFrame() {
        g_titleBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight();
    }

    void Window::endNativeWindowFrame() {
        if (!ImHexApi::System::isBorderlessWindowModeEnabled())
            return;

        if (g_mouseCursorIcon != ImGuiMouseCursor_None) {
            ImGui::SetMouseCursor(g_mouseCursorIcon);
        }

        // Translate ImGui mouse cursors to Win32 mouse cursors
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
            case ImGuiMouseCursor_None:
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
                break;
        }
    }

    void Window::drawTitleBar() {
        // In borderless window mode, we draw our own title bar

        if (!ImHexApi::System::isBorderlessWindowModeEnabled()) return;

        auto startX = ImGui::GetCursorPosX();

        auto buttonSize = ImVec2(g_titleBarHeight * 1.5F, g_titleBarHeight - 1);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabHovered));

        auto &titleBarButtons = ContentRegistry::Interface::impl::getTitleBarButtons();

        // Draw custom title bar buttons
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - buttonSize.x * (4 + titleBarButtons.size()));
        for (const auto &[icon, tooltip, callback] : titleBarButtons) {
            if (ImGui::TitleBarButton(icon.c_str(), buttonSize)) {
                callback();
            }
            ImGui::InfoTooltip(LangEntry(tooltip));
        }

        // Draw minimize, restore and maximize buttons
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - buttonSize.x * 3);
        if (ImGui::TitleBarButton(ICON_VS_CHROME_MINIMIZE, buttonSize))
            glfwIconifyWindow(this->m_window);
        if (glfwGetWindowAttrib(this->m_window, GLFW_MAXIMIZED)) {
            if (ImGui::TitleBarButton(ICON_VS_CHROME_RESTORE, buttonSize))
                glfwRestoreWindow(this->m_window);
        } else {
            if (ImGui::TitleBarButton(ICON_VS_CHROME_MAXIMIZE, buttonSize))
                glfwMaximizeWindow(this->m_window);
        }

        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0xFF7A70F1);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0xFF2311E8);

        // Draw close button
        if (ImGui::TitleBarButton(ICON_VS_CHROME_CLOSE, buttonSize)) {
            ImHexApi::System::closeImHex();
        }

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar();

        ImGui::SetCursorPosX(std::max(startX, (ImGui::GetWindowWidth() - ImGui::CalcTextSize(this->m_windowTitle.c_str()).x) / 2));
        ImGui::TextUnformatted(this->m_windowTitle.c_str());
    }

}

#endif