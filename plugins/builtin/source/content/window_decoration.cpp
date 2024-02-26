#include <hex/api/content_registry.hpp>
#include <hex/api/event_manager.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/project_file_manager.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/utils.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <fonts/codicons_font.h>
#include <romfs/romfs.hpp>
#include <wolv/utils/guards.hpp>

namespace hex::plugin::builtin {

    namespace {

        std::string s_windowTitle, s_windowTitleFull;
        u32 s_searchBarPosition = 0;
        ImGuiExt::Texture s_logoTexture;
        bool s_showSearchBar = true;

        void createNestedMenu(std::span<const UnlocalizedString> menuItems, const char *icon, const Shortcut &shortcut, const ContentRegistry::Interface::impl::MenuCallback &callback, const ContentRegistry::Interface::impl::EnabledCallback &enabledCallback, const ContentRegistry::Interface::impl::SelectedCallback &selectedCallback) {
            const auto &name = menuItems.front();

            if (name.get() == ContentRegistry::Interface::impl::SeparatorValue) {
                ImGui::Separator();
                return;
            }

            if (name.get() == ContentRegistry::Interface::impl::SubMenuValue) {
                callback();
            } else if (menuItems.size() == 1) {
                if (ImGui::MenuItemEx(Lang(name), icon, shortcut.toString().c_str(), selectedCallback(), enabledCallback()))
                    callback();
            } else {
                bool isSubmenu = (menuItems.begin() + 1)->get() == ContentRegistry::Interface::impl::SubMenuValue;

                if (ImGui::BeginMenuEx(Lang(name), std::next(menuItems.begin())->get() == ContentRegistry::Interface::impl::SubMenuValue ? icon : nullptr, isSubmenu ? enabledCallback() : true)) {
                    createNestedMenu({ std::next(menuItems.begin()), menuItems.end() }, icon, shortcut, callback, enabledCallback, selectedCallback);
                    ImGui::EndMenu();
                }
            }
        }

        void drawFooter(ImDrawList *drawList, ImVec2 dockSpaceSize, float footerHeight) {
            auto dockId = ImGui::DockSpace(ImGui::GetID("ImHexMainDock"), dockSpaceSize);
            ImHexApi::System::impl::setMainDockSpaceId(dockId);

            drawList->AddRectFilled(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize() - ImVec2(dockSpaceSize.x, footerHeight - ImGui::GetStyle().FramePadding.y - 1_scaled), ImGui::GetColorU32(ImGuiCol_MenuBarBg));

            ImGui::Separator();
            ImGui::SetCursorPosX(8);
            for (const auto &callback : ContentRegistry::Interface::impl::getFooterItems()) {
                auto prevIdx = drawList->_VtxCurrentIdx;
                callback();
                auto currIdx = drawList->_VtxCurrentIdx;

                // Only draw separator if something was actually drawn
                if (prevIdx != currIdx) {
                    ImGui::SameLine();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine();
                }
            }
        }

        void drawSidebar(ImVec2 dockSpaceSize, ImVec2 sidebarPos, float sidebarWidth) {
                static i32 openWindow = -1;
                u32 index = 0;
                ImGui::PushID("SideBarWindows");
                for (const auto &[icon, callback, enabledCallback] : ContentRegistry::Interface::impl::getSidebarItems()) {
                    ImGui::SetCursorPosY(sidebarPos.y + sidebarWidth * index);

                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabActive));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabHovered));

                    ImGui::BeginDisabled(!(ImHexApi::Provider::isValid() && enabledCallback()));
                    {
                        if (ImGui::Button(icon.c_str(), ImVec2(sidebarWidth, sidebarWidth))) {
                            if (static_cast<u32>(openWindow) == index)
                                openWindow = -1;
                            else
                                openWindow = index;
                        }
                    }
                    ImGui::EndDisabled();

                    ImGui::PopStyleColor(3);

                    auto sideBarFocused = ImGui::IsWindowFocused();

                    bool open = static_cast<u32>(openWindow) == index;
                    if (open) {

                        ImGui::SetNextWindowPos(ImGui::GetWindowPos() + sidebarPos + ImVec2(sidebarWidth - 1_scaled, -1_scaled));
                        ImGui::SetNextWindowSize(ImVec2(0, dockSpaceSize.y + 5_scaled));

                        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1);
                        ImGui::PushStyleColor(ImGuiCol_WindowShadow, 0x00000000);
                        if (ImGui::Begin("SideBarWindow", &open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                            if (ImGui::BeginChild("##Content", ImVec2(), ImGuiChildFlags_ResizeX)) {
                                callback();
                            }
                            ImGui::EndChild();

                            if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !sideBarFocused) {
                                openWindow = -1;
                            }
                        }
                        ImGui::End();
                        ImGui::PopStyleVar();
                        ImGui::PopStyleColor();
                    }

                    ImGui::NewLine();
                    index++;
                }
                ImGui::PopID();
        }

        void drawTitleBar() {
            auto titleBarHeight = ImGui::GetCurrentWindowRead()->MenuBarHeight();
            auto buttonSize = ImVec2(titleBarHeight * 1.5F, titleBarHeight - 1);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabHovered));

            const auto windowSize = ImHexApi::System::getMainWindowSize();
            auto searchBoxSize = ImVec2(s_showSearchBar ? windowSize.x / 2.5 : ImGui::CalcTextSize(s_windowTitle.c_str()).x, titleBarHeight);
            auto searchBoxPos = ImVec2((windowSize / 2 - searchBoxSize / 2).x, 0);
            auto titleBarButtonPosY = 0.0F;

            #if defined(OS_MACOS)
                searchBoxPos.y = ImGui::GetStyle().FramePadding.y * 2;
                titleBarButtonPosY = searchBoxPos.y;
            #else
                titleBarButtonPosY = 0;

                if (s_showSearchBar) {
                    searchBoxPos.y = 3_scaled;
                    searchBoxSize.y -= 3_scaled;
                }
            #endif

            s_searchBarPosition = searchBoxPos.x;

            // Custom titlebar buttons implementation for borderless window mode
            auto window = ImHexApi::System::getMainWindowHandle();
            bool titleBarButtonsVisible = false;
            if (ImHexApi::System::isBorderlessWindowModeEnabled() && glfwGetWindowMonitor(window) == nullptr) {
                #if defined(OS_WINDOWS)
                    titleBarButtonsVisible = true;

                    // Draw minimize, restore and maximize buttons
                    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - buttonSize.x * 3);
                    if (ImGuiExt::TitleBarButton(ICON_VS_CHROME_MINIMIZE, buttonSize))
                        glfwIconifyWindow(window);
                    if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED)) {
                        if (ImGuiExt::TitleBarButton(ICON_VS_CHROME_RESTORE, buttonSize))
                            glfwRestoreWindow(window);
                    } else {
                        if (ImGuiExt::TitleBarButton(ICON_VS_CHROME_MAXIMIZE, buttonSize))
                            glfwMaximizeWindow(window);
                    }

                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0xFF7A70F1);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0xFF2311E8);

                    // Draw close button
                    if (ImGuiExt::TitleBarButton(ICON_VS_CHROME_CLOSE, buttonSize)) {
                        ImHexApi::System::closeImHex();
                    }

                    ImGui::PopStyleColor(2);

                #endif
            }

            auto &titleBarButtons = ContentRegistry::Interface::impl::getTitlebarButtons();

            // Draw custom title bar buttons
            if (!titleBarButtons.empty()) {
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 7_scaled - buttonSize.x * float((titleBarButtonsVisible ? 4 : 0) + titleBarButtons.size()));

                if (ImGui::GetCursorPosX() > (searchBoxPos.x + searchBoxSize.x)) {
                    for (const auto &[icon, tooltip, callback] : titleBarButtons) {
                        ImGui::SetCursorPosY(titleBarButtonPosY);
                        if (ImGuiExt::TitleBarButton(icon.c_str(), buttonSize)) {
                            callback();
                        }
                        ImGuiExt::InfoTooltip(Lang(tooltip));
                    }
                }
            }

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();

            {
                ImGui::SetCursorPos(searchBoxPos);

                if (s_showSearchBar) {
                    const auto buttonColor = [](float alpha) {
                        return ImU32(ImColor(ImGui::GetStyleColorVec4(ImGuiCol_DockingEmptyBg) * ImVec4(1, 1, 1, alpha)));
                    };

                    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor(0.5F));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonColor(0.7F));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonColor(0.9F));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0_scaled);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4_scaled);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, scaled({ 1, 1 }));


                    if (ImGui::Button(s_windowTitle.c_str(), searchBoxSize)) {
                        EventSearchBoxClicked::post(ImGuiMouseButton_Left);
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                        EventSearchBoxClicked::post(ImGuiMouseButton_Right);

                    ImGui::PushTextWrapPos(300_scaled);
                    if (!s_windowTitleFull.empty())
                        ImGui::SetItemTooltip("%s", s_windowTitleFull.c_str());
                    ImGui::PopTextWrapPos();

                    ImGui::PopStyleVar(3);
                    ImGui::PopStyleColor(3);
                } else {
                    ImGui::TextUnformatted(s_windowTitle.c_str());
                }
            }
        }

        void drawMainMenu([[maybe_unused]] float menuBarHeight) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
            ImGui::SetNextWindowScroll(ImVec2(0, 0));

            #if defined(OS_MACOS)
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 8_scaled));
                ON_SCOPE_EXIT { ImGui::PopStyleVar(); };
            #endif

            if (ImGui::BeginMainMenuBar()) {
                auto window = ImHexApi::System::getMainWindowHandle();

                ImGui::PopStyleVar();

                if (ImHexApi::System::isBorderlessWindowModeEnabled()) {
                    #if defined(OS_WINDOWS)
                        ImGui::SetCursorPosX(5_scaled);
                        ImGui::Image(s_logoTexture, ImVec2(menuBarHeight, menuBarHeight));
                        ImGui::SetCursorPosX(5_scaled);
                        ImGui::InvisibleButton("##logo", ImVec2(menuBarHeight, menuBarHeight));
                        ImGui::OpenPopupOnItemClick("WindowingMenu", ImGuiPopupFlags_MouseButtonLeft);
                    #elif defined(OS_MACOS)
                        if (!isMacosFullScreenModeEnabled(window))
                            ImGui::SetCursorPosX(68_scaled);
                    #endif
                }

                if (ImGui::BeginPopup("WindowingMenu")) {
                    bool maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED);

                    ImGui::BeginDisabled(!maximized);
                    if (ImGui::MenuItem(ICON_VS_CHROME_RESTORE " Restore"))  glfwRestoreWindow(window);
                    ImGui::EndDisabled();

                    if (ImGui::MenuItem(ICON_VS_CHROME_MINIMIZE " Minimize")) glfwIconifyWindow(window);

                    ImGui::BeginDisabled(maximized);
                    if (ImGui::MenuItem(ICON_VS_CHROME_MAXIMIZE " Maximize")) glfwMaximizeWindow(window);
                    ImGui::EndDisabled();

                    ImGui::Separator();

                    if (ImGui::MenuItem(ICON_VS_CHROME_CLOSE " Close"))    ImHexApi::System::closeImHex();

                    ImGui::EndPopup();
                }

                const static auto drawMenu = [] {
                    for (const auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMainMenuItems()) {
                        ImGui::GetStyle().TouchExtraPadding = scaled(ImVec2(0, 2));
                        if (ImGui::BeginMenu(Lang(menuItem.unlocalizedName))) {
                            ImGui::EndMenu();
                        }
                        ImGui::GetStyle().TouchExtraPadding = ImVec2(0, 0);
                    }

                    for (auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMenuItems()) {
                        const auto &[
                            unlocalizedNames,
                            icon,
                            shortcut,
                            view,
                            callback,
                            enabledCallback,
                            selectedCallack,
                            toolbarIndex
                        ] = menuItem;

                        createNestedMenu(unlocalizedNames, icon.glyph.c_str(), *shortcut, callback, enabledCallback, selectedCallack);
                    }
                };

                static u32 menuEndPos = 0;
                if (menuEndPos <= s_searchBarPosition) {
                    drawMenu();

                    // Only initialize the menu end position if the language is already loaded
                    if (!LocalizationManager::getSelectedLanguage().empty())
                        menuEndPos = ImGui::GetCursorPosX();
                } else {
                    if (ImGui::BeginMenu(ICON_VS_MENU)) {
                        drawMenu();
                        ImGui::EndMenu();
                    }
                }

                drawTitleBar();

                ImGui::EndMainMenuBar();
            } else {
                ImGui::PopStyleVar();
            }
        }

        void drawToolbar() {
            if (ImGui::BeginMenuBar()) {
                for (const auto &callback : ContentRegistry::Interface::impl::getToolbarItems()) {
                    callback();
                    ImGui::SameLine();
                }

                if (auto provider = ImHexApi::Provider::get(); provider != nullptr) {
                    ImGui::BeginDisabled(TaskManager::getRunningTaskCount() > 0);
                    if (ImGui::CloseButton(ImGui::GetID("ProviderCloseButton"), ImGui::GetCursorScreenPos() + ImVec2(ImGui::GetContentRegionAvail().x - 17_scaled, 3_scaled))) {
                        ImHexApi::Provider::remove(provider);
                    }
                    ImGui::EndDisabled();
                }

                ImGui::EndMenuBar();
            }
        }

        bool anySidebarItemsAvailable() {
            if (const auto &items = ContentRegistry::Interface::impl::getSidebarItems(); items.empty()) {
                return false;
            } else {
                return std::any_of(items.begin(), items.end(), [](const auto &item) {
                    return item.enabledCallback();
                });
            }
        }

        bool isAnyViewOpen() {
            const auto &views = ContentRegistry::Views::impl::getEntries();
            return std::any_of(views.begin(), views.end(),
                               [](const auto &entry) {
                                   return entry.second->getWindowOpenState();
                               });
        }

    }

    void addWindowDecoration() {
        EventFrameBegin::subscribe([]{
            AT_FIRST_TIME {
                s_logoTexture = ImGuiExt::Texture(romfs::get("assets/common/icon.png").span(), ImGuiExt::Texture::Filter::Nearest);
            };

            constexpr static ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

            // Handle all undocked floating windows
            ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(ImHexApi::System::getMainWindowSize() - ImVec2(0, ImGui::GetTextLineHeightWithSpacing()));
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

            // Draw main window decoration
            if (ImGui::Begin("ImHexDockSpace", nullptr, windowFlags)) {
                ImGui::PopStyleVar();

                const auto drawList = ImGui::GetWindowDrawList();
                const auto shouldDrawSidebar = anySidebarItemsAvailable();

                const auto menuBarHeight = ImGui::GetCurrentWindowRead()->MenuBarHeight();
                const auto sidebarPos   = ImGui::GetCursorPos();
                const auto sidebarWidth = shouldDrawSidebar ? 20_scaled : 0;

                auto footerHeight  = ImGui::GetTextLineHeightWithSpacing() + 1_scaled;
                #if defined(OS_MACOS)
                    footerHeight += ImGui::GetStyle().WindowPadding.y * 2;
                #else
                    footerHeight += ImGui::GetStyle().FramePadding.y * 2;
                #endif

                const auto dockSpaceSize = ImVec2(ImHexApi::System::getMainWindowSize().x - sidebarWidth, ImGui::GetContentRegionAvail().y - footerHeight);

                ImGui::SetCursorPosX(sidebarWidth);
                drawFooter(drawList, dockSpaceSize, footerHeight);

                if (shouldDrawSidebar) {
                    ImGui::SetCursorPos(sidebarPos);
                    drawSidebar(dockSpaceSize, sidebarPos, sidebarWidth);
                }

                drawMainMenu(menuBarHeight);
                drawToolbar();

                if (ImHexApi::Provider::isValid() && isAnyViewOpen()) {
                    drawList->AddLine(
                            ImGui::GetWindowPos() + sidebarPos + ImVec2(sidebarWidth - 1_scaled, -2_scaled),
                            ImGui::GetWindowPos() + sidebarPos + ImGui::GetWindowSize() - ImVec2(dockSpaceSize.x + 1_scaled, footerHeight - ImGui::GetStyle().FramePadding.y - 1_scaled + menuBarHeight),
                            ImGui::GetColorU32(ImGuiCol_Separator));
                }
            } else {
                ImGui::PopStyleVar();
            }
            ImGui::End();

            ImGui::PopStyleVar(2);

            // Draw main menu popups
            for (auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMenuItems()) {
                const auto &[unlocalizedNames, icon, shortcut, view, callback, enabledCallback, selectedCallback, toolbarIndex] = menuItem;

                if (ImGui::BeginPopup(unlocalizedNames.front().get().c_str())) {
                    createNestedMenu({ unlocalizedNames.begin() + 1, unlocalizedNames.end() }, icon.glyph.c_str(), *shortcut, callback, enabledCallback, selectedCallback);
                    ImGui::EndPopup();
                }
            }
        });


        constexpr static auto ImHexTitle = "ImHex";

        s_windowTitle = ImHexTitle;

        // Handle updating the window title
        RequestUpdateWindowTitle::subscribe([] {
            std::string prefix, postfix;
            std::string title = ImHexTitle;

            if (ProjectFile::hasPath()) {
                // If a project is open, show the project name instead of the file name

                prefix  = "Project ";
                title   = ProjectFile::getPath().stem().string();

                if (ImHexApi::Provider::isDirty())
                    postfix += " (*)";

            } else if (ImHexApi::Provider::isValid()) {
                auto provider = ImHexApi::Provider::get();
                if (provider != nullptr) {
                    title = provider->getName();

                    if (provider->isDirty())
                        postfix += " (*)";

                    if (!provider->isWritable() && provider->getActualSize() != 0)
                        postfix += " (Read Only)";
                }
            }

            s_windowTitle     = prefix + hex::limitStringLength(title, 32) + postfix;
            s_windowTitleFull = prefix + title + postfix;

            auto window = ImHexApi::System::getMainWindowHandle();
            if (window != nullptr) {
                if (title != ImHexTitle)
                    title = std::string(ImHexTitle) + " - " + title;

                glfwSetWindowTitle(window, title.c_str());
            }
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.interface", "hex.builtin.setting.interface.show_header_command_palette", [](const ContentRegistry::Settings::SettingsValue &value) {
            s_showSearchBar = value.get<bool>(true);
        });
    }


}
