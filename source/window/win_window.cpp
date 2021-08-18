#include "window.hpp"

#if defined(OS_WINDOWS)

    #include <imgui.h>

    #include <GLFW/glfw3.h>
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
    #include <winuser.h>
    #include <dwmapi.h>
    #include <windowsx.h>
    #include <imgui_internal.h>

    #include <codicons_font.h>

    namespace hex {
        static LONG_PTR oldWndProc;
        static float titleBarHeight;
        static ImGuiMouseCursor mouseCursorIcon;

        LRESULT wndProcImHex(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
            switch (uMsg) {
                case WM_NCCALCSIZE: {
                    auto& params = *reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                    RECT &rect = params.rgrc[0];

                    WINDOWPLACEMENT placement;
                    if (!::GetWindowPlacement(hwnd, &placement) || placement.showCmd != SW_MAXIMIZE)
                        return 0;

                    auto monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
                    if (monitor == nullptr) {
                        return 0;
                    }

                    MONITORINFO monitor_info{};
                    monitor_info.cbSize = sizeof(monitor_info);
                    if (!::GetMonitorInfoW(monitor, &monitor_info))
                        return 0;

                    rect = monitor_info.rcWork;

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
                        ::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER),
                        ::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)
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
                            if ((cursor.y < (window.top + titleBarHeight)) && !ImGui::IsAnyItemHovered())
                                return HTCAPTION;
                            else break;
                    }
                }
            }

            return CallWindowProc((WNDPROC)oldWndProc, hwnd, uMsg, wParam, lParam);
        }

        void Window::setupNativeWindow() {
            auto hwnd = glfwGetWin32Window(this->m_window);
            oldWndProc = SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)wndProcImHex);
            MARGINS borderless = {1,1,1,1};
            DwmExtendFrameIntoClientArea(hwnd, &borderless);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_ASYNCWINDOWPOS | SWP_NOSIZE | SWP_NOMOVE);
            SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) | WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CAPTION | WS_SYSMENU);
        }

        void Window::updateNativeWindow() {
            titleBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight();

            if (mouseCursorIcon != ImGuiMouseCursor_None)
                ImGui::SetMouseCursor(mouseCursorIcon);
        }

        void Window::drawTitleBar() {
            auto buttonSize = ImVec2(titleBarHeight * 1.5F, titleBarHeight);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabHovered));

            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - buttonSize.x * 6);
        #if defined(DEBUG)
            if (ImGui::Button(ICON_VS_DEBUG, buttonSize))
                hex::openWebpage("https://imhex.werwolv.net/debug");
            ImGui::InfoTooltip("hex.menu.debug_build"_lang);
        #endif
            if (ImGui::Button(ICON_VS_SMILEY, buttonSize))
                hex::openWebpage("mailto://hey@werwolv.net");
            ImGui::InfoTooltip("hex.menu.feedback"_lang);

            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - buttonSize.x * 3);
            if (ImGui::Button(ICON_VS_CHROME_MINIMIZE, buttonSize))
                glfwIconifyWindow(this->m_window);
            if (glfwGetWindowAttrib(this->m_window, GLFW_MAXIMIZED)) {
                if (ImGui::Button(ICON_VS_CHROME_RESTORE, buttonSize))
                    glfwRestoreWindow(this->m_window);
            } else {
                if (ImGui::Button(ICON_VS_CHROME_MAXIMIZE, buttonSize))
                    glfwMaximizeWindow(this->m_window);
            }

            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0xFF7A70F1);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0xFF2311E8);


            if (ImGui::Button(ICON_VS_CHROME_CLOSE, buttonSize)) {
                EventManager::post<RequestCloseImHex>();
                EventManager::post<EventWindowClosing>(this->m_window);
            }

            ImGui::PopStyleColor(5);
            ImGui::PopStyleVar();

            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(this->m_windowTitle.c_str()).x) / 2);
            ImGui::TextUnformatted(this->m_windowTitle.c_str());
        }

    }

#endif