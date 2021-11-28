#include "window.hpp"


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

        static LONG_PTR oldWndProc;
        static float titleBarHeight;
        static ImGuiMouseCursor mouseCursorIcon;
        static BOOL compositionEnabled = false;

        static bool isTaskbarAutoHideEnabled(UINT edge, RECT monitor) {
            APPBARDATA data = { .cbSize = sizeof(APPBARDATA), .uEdge = edge, .rc = monitor };
            return ::SHAppBarMessage(ABM_GETAUTOHIDEBAR, &data);
        }

        static LRESULT windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
            switch (uMsg) {
                case WM_NCCALCSIZE: {
                    RECT &rect = *reinterpret_cast<RECT*>(lParam);
                    RECT client = rect;

                    DefWindowProcW(hwnd, WM_NCCALCSIZE, wParam, lParam);

                    if (IsMaximized(hwnd)) {
                        WINDOWINFO windowInfo = { .cbSize = sizeof(WINDOWINFO) };
                        GetWindowInfo(hwnd, &windowInfo);
                        rect = RECT {
                            .left = static_cast<LONG>(client.left + windowInfo.cyWindowBorders),
                            .top = static_cast<LONG>(client.top + windowInfo.cyWindowBorders),
                            .right = static_cast<LONG>(client.right - windowInfo.cyWindowBorders),
                            .bottom = static_cast<LONG>(client.bottom - windowInfo.cyWindowBorders)
                        };

                        HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
                        MONITORINFO monitorInfo = { .cbSize = sizeof(MONITORINFO) };
                        GetMonitorInfoW(hMonitor, &monitorInfo);

                        if (EqualRect(&rect, &monitorInfo.rcMonitor)) {
                            if (isTaskbarAutoHideEnabled(ABE_BOTTOM, monitorInfo.rcMonitor))
                                rect.bottom--;
                            else if (isTaskbarAutoHideEnabled(ABE_LEFT, monitorInfo.rcMonitor))
                                rect.left++;
                            else if (isTaskbarAutoHideEnabled(ABE_TOP, monitorInfo.rcMonitor))
                                rect.top++;
                            else if (isTaskbarAutoHideEnabled(ABE_RIGHT, monitorInfo.rcMonitor))
                                rect.right--;
                        }
                    } else {
                        rect = client;
                    }

                    return 0;
                }
                case WM_SETCURSOR: {
                    auto cursorPos = LOWORD(lParam);

                    switch (cursorPos) {
                        case HTRIGHT:
                        case HTLEFT:
                            mouseCursorIcon = ImGuiMouseCursor_ResizeEW;
                            return TRUE;
                        case HTTOP:
                        case HTBOTTOM:
                            mouseCursorIcon = ImGuiMouseCursor_ResizeNS;
                            return TRUE;
                        case HTTOPLEFT:
                        case HTBOTTOMRIGHT:
                            mouseCursorIcon = ImGuiMouseCursor_ResizeNWSE;
                            return TRUE;
                        case HTTOPRIGHT:
                        case HTBOTTOMLEFT:
                            mouseCursorIcon = ImGuiMouseCursor_ResizeNESW;
                            return TRUE;
                        default:
                            mouseCursorIcon = ImGuiMouseCursor_None;
                            return TRUE;
                    }
                }
                case WM_NCHITTEST: {
                    POINT cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

                    const POINT border{
                        static_cast<LONG>((::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)) * SharedData::globalScale / 1.5F),
                        static_cast<LONG>((::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)) * SharedData::globalScale / 1.5F)
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
                            RegionLeft   * (cursor.x <  (window.left   + border.x)) |
                            RegionRight  * (cursor.x >= (window.right  - border.x)) |
                            RegionTop    * (cursor.y <  (window.top    + border.y)) |
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
                            if ((cursor.y < (window.top + titleBarHeight * 2)) && !(ImGui::IsAnyItemHovered() || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)))
                                return HTCAPTION;
                            else break;
                    }
                    break;
                }
                case WM_SETTINGCHANGE:
                {
                    if (LPCTSTR(lParam) == std::string_view("ImmersiveColorSet")) {
                        EventManager::post<EventOSThemeChanged>();
                    }

                    break;
                }
                case WM_NCACTIVATE:
                case WM_NCPAINT:
                {
                    return DefWindowProc(hwnd, uMsg, wParam, lParam);
                }
                default: break;
            }

            return CallWindowProc((WNDPROC)oldWndProc, hwnd, uMsg, wParam, lParam);
        }


        void Window::initNative() {
            // Attach to parent console if one exists
            if (AttachConsole(ATTACH_PARENT_PROCESS)) {

                // Redirect cin, cout and cerr to that console
                freopen("CONIN$", "w", stdin);
                freopen("CONOUT$", "w", stdout);
                freopen("CONERR$", "w", stderr);
                setvbuf(stdin,  nullptr, _IONBF, 0);
                setvbuf(stdout, nullptr, _IONBF, 0);
                setvbuf(stderr, nullptr, _IONBF, 0);

                fmt::print("\n");
            }


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
        }

        void Window::setupNativeWindow() {
            auto hwnd = glfwGetWin32Window(this->m_window);

            oldWndProc = ::SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)windowProc);

            MARGINS borderless = { 1, 1, 1, 1 };
            ::DwmExtendFrameIntoClientArea(hwnd, &borderless);

            DWORD attribute = DWMNCRP_ENABLED;
            ::DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &attribute, sizeof(attribute));

            ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_ASYNCWINDOWPOS | SWP_NOSIZE | SWP_NOMOVE);
            ::SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) | WS_OVERLAPPEDWINDOW);

            bool themeFollowSystem = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.color") == 0;
            EventManager::subscribe<EventOSThemeChanged>(this, [themeFollowSystem]{
                if (!themeFollowSystem) return;

                HKEY hkey;
                if (RegOpenKey(HKEY_CURRENT_USER, R"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)", &hkey) == ERROR_SUCCESS) {
                    DWORD value = 0;
                    DWORD size = sizeof(DWORD);

                    auto error = RegQueryValueEx(hkey, "AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &size);
                    if (error == ERROR_SUCCESS) {
                        EventManager::post<RequestChangeTheme>(value == 0 ? 1 : 2);
                    }
                }
            });

            if (themeFollowSystem)
                EventManager::post<EventOSThemeChanged>();
        }

        void Window::updateNativeWindow() {
            titleBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight();

            if (mouseCursorIcon != ImGuiMouseCursor_None)
                ImGui::SetMouseCursor(mouseCursorIcon);
        }

        void Window::drawTitleBar() {
            auto buttonSize = ImVec2(titleBarHeight * 1.5F, titleBarHeight - 1);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabHovered));

            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - buttonSize.x * 6);
        #if defined(DEBUG)
            if (ImGui::TitleBarButton(ICON_VS_DEBUG, buttonSize)) {
                if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift) {
                    // Explicitly trigger a segfault by writing to an invalid memory location
                    // Used for debugging crashes
                    *reinterpret_cast<u8*>(0x10) = 0x10;
                } else {
                    hex::openWebpage("https://imhex.werwolv.net/debug");
                }
            }
            ImGui::InfoTooltip("hex.menu.debug_build"_lang);
        #endif
            if (ImGui::TitleBarButton(ICON_VS_SMILEY, buttonSize))
                hex::openWebpage("mailto://hey@werwolv.net");
            ImGui::InfoTooltip("hex.menu.feedback"_lang);

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