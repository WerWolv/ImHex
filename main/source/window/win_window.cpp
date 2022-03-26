#include "window.hpp"

#include <hex/api/content_registry.hpp>

#if defined(OS_WINDOWS)

    #include <hex/helpers/utils.hpp>
    #include <hex/helpers/logger.hpp>

    #include <imgui.h>
    #include <imgui_internal.h>
    #include <codicons_font.h>

    #include <nlohmann/json.hpp>

    #include <GLFW/glfw3.h>
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
    #undef GLFW_EXPOSE_NATIVE_WIN32

    #include <winuser.h>
    #include <dwmapi.h>
    #include <windowsx.h>

namespace hex {

    static LONG_PTR g_oldWndProc;
    static float g_titleBarHeight;
    static ImGuiMouseCursor g_mouseCursorIcon;

    static LRESULT windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_NCACTIVATE:
            case WM_NCPAINT:
                return DefWindowProcW(hwnd, uMsg, wParam, lParam);
            case WM_NCCALCSIZE:
                {
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
            case WM_SETCURSOR:
                {
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
            case WM_NCHITTEST:
                {
                    POINT cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

                    const POINT border {
                        static_cast<LONG>((::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)) * ImHexApi::System::getGlobalScale() / 1.5F),
                        static_cast<LONG>((::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)) * ImHexApi::System::getGlobalScale() / 1.5F)
                    };

                    RECT window;
                    if (!::GetWindowRect(hwnd, &window)) {
                        return HTNOWHERE;
                    }

                    constexpr auto RegionClient = 0b0000;
                    constexpr auto RegionLeft   = 0b0001;
                    constexpr auto RegionRight  = 0b0010;
                    constexpr auto RegionTop    = 0b0100;
                    constexpr auto RegionBottom = 0b1000;

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
            case WM_SETTINGCHANGE:
                {
                    if (lParam == 0) break;

                    if (LPCTSTR(lParam) == std::string_view("ImmersiveColorSet")) {
                        EventManager::post<EventOSThemeChanged>();
                    }

                    break;
                }
            case WM_COPYDATA:
                {
                    auto message = reinterpret_cast<COPYDATASTRUCT *>(lParam);
                    if (message == nullptr) break;

                    auto path = reinterpret_cast<const char *>(message->lpData);
                    if (path == nullptr) break;

                    log::info("Opening file in existing instance: {}", path);
                    EventManager::post<RequestOpenFile>(path);
                    break;
                }

            default:
                break;
        }

        return CallWindowProc((WNDPROC)g_oldWndProc, hwnd, uMsg, wParam, lParam);
    }


    void Window::initNative() {
        // Attach to parent console if one exists
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {

            // Redirect cin, cout and cerr to that console
            freopen("CONIN$", "r", stdin);
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
            setvbuf(stdin, nullptr, _IONBF, 0);
            setvbuf(stdout, nullptr, _IONBF, 0);
            setvbuf(stderr, nullptr, _IONBF, 0);

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
            globalMutex = CreateMutex(nullptr, FALSE, UniqueMutexId);
        } else {
            if (ImHexApi::System::getProgramArguments().argc > 1) {
                ::EnumWindows([](HWND hWnd, LPARAM) -> BOOL {
                    auto &programArgs = ImHexApi::System::getProgramArguments();

                    auto length = ::GetWindowTextLength(hWnd);
                    std::string windowName(length + 1, '\x00');
                    ::GetWindowText(hWnd, windowName.data(), windowName.size());

                    if (::IsWindowVisible(hWnd) && length != 0) {
                        if (windowName.starts_with("ImHex")) {
                            COPYDATASTRUCT message = {
                                .dwData = 0,
                                .cbData = static_cast<DWORD>(std::strlen(programArgs.argv[1])) + 1,
                                .lpData = programArgs.argv[1]
                            };

                            SendMessage(hWnd, WM_COPYDATA, reinterpret_cast<WPARAM>(hWnd), reinterpret_cast<LPARAM>(&message));

                            return FALSE;
                        }
                    }

                    return TRUE;
                },
                    0);

                std::exit(0);
            }
        }
    }

    void Window::setupNativeWindow() {
        // Setup borderless window
        auto hwnd = glfwGetWin32Window(this->m_window);

        if (ImHexApi::System::isBorderlessWindowModeEnabled()) {
            g_oldWndProc = ::SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)windowProc);

            MARGINS borderless = { 1, 1, 1, 1 };
            ::DwmExtendFrameIntoClientArea(hwnd, &borderless);

            DWORD attribute = DWMNCRP_ENABLED;
            ::DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &attribute, sizeof(attribute));

            ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE);
            ::SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) | WS_OVERLAPPEDWINDOW);
        }
    }

    void Window::beginNativeWindowFrame() {
        g_titleBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight();
    }

    void Window::endNativeWindowFrame() {
        if (g_mouseCursorIcon != ImGuiMouseCursor_None) {
            ImGui::SetMouseCursor(g_mouseCursorIcon);
        }

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
        if (!ImHexApi::System::isBorderlessWindowModeEnabled()) return;

        auto buttonSize = ImVec2(g_titleBarHeight * 1.5F, g_titleBarHeight - 1);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabHovered));

        auto &titleBarButtons = ContentRegistry::Interface::getTitleBarButtons();

        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - buttonSize.x * (4 + titleBarButtons.size()));
        for (const auto &[icon, tooltip, callback] : titleBarButtons) {
            if (ImGui::TitleBarButton(icon.c_str(), buttonSize)) {
                callback();
            }
            ImGui::InfoTooltip(LangEntry(tooltip));
        }

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


        if (ImGui::TitleBarButton(ICON_VS_CHROME_CLOSE, buttonSize)) {
            ImHexApi::Common::closeImHex();
        }

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar();

        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(this->m_windowTitle.c_str()).x) / 2);
        ImGui::TextUnformatted(this->m_windowTitle.c_str());
    }

}

#endif