#include <algorithm>
#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/content_registry/views.hpp>
#include <hex/api/content_registry/settings.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/events/events_gui.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/api/events/events_interaction.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/menu_items.hpp>
#include <hex/helpers/auto_reset.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <fonts/fonts.hpp>
#include <fonts/vscode_icons.hpp>

#include <romfs/romfs.hpp>
#include <wolv/utils/guards.hpp>

#include <GLFW/glfw3.h>

#include <random>
#include <unordered_set>

namespace hex::plugin::builtin {

    // Function that draws the provider popup, defined in the ui_items.cpp file
    void drawProviderTooltip(const prv::Provider *provider);

    namespace {

        std::string s_windowTitle, s_windowTitleFull;
        u32 s_searchBarPosition = 0;
        AutoReset<ImGuiExt::Texture> s_logoTexture;

        ContentRegistry::Settings::SettingsVariable<bool, "hex.builtin.setting.interface", "hex.builtin.setting.interface.show_header_command_palette"> s_showSearchBar = true;
        ContentRegistry::Settings::SettingsVariable<bool, "hex.builtin.setting.interface", "hex.builtin.setting.interface.display_shortcut_highlights"> s_displayShortcutHighlights = true;
        ContentRegistry::Settings::SettingsVariable<bool, "hex.builtin.setting.interface", "hex.builtin.setting.interface.use_native_menu_bar"> s_useNativeMenuBar = true;
        ContentRegistry::Settings::SettingsVariable<bool, "hex.builtin.setting.interface", "hex.builtin.setting.interface.show_titlebar_backdrop"> s_showTitlebarBackDrop = true;

        void drawTitleBarBackDrop() {
            if (!s_showTitlebarBackDrop)
                return;

            const auto diameter = 800_scaled;
            const auto pos = ImHexApi::System::getMainWindowPosition() - ImVec2(0, diameter / 2);
            ImGui::GetWindowDrawList()->AddShadowCircle(pos, diameter / 2, ImGui::GetColorU32(ImGuiCol_ButtonActive, 0.8F), diameter / 4, ImVec2());
        }

        bool allowSeparator = false;
        void createNestedMenu(std::span<const UnlocalizedString> menuItems, const char *icon, const Shortcut &shortcut, View *view, const ContentRegistry::UserInterface::impl::MenuCallback &callback, const ContentRegistry::UserInterface::impl::EnabledCallback &enabledCallback, const ContentRegistry::UserInterface::impl::SelectedCallback &selectedCallback) {
            const auto &name = menuItems.front();

            if (name.get() == ContentRegistry::UserInterface::impl::SeparatorValue) {
                if (allowSeparator)
                    menu::menuSeparator();
                allowSeparator = false;
                return;
            }

            if (name.get() == ContentRegistry::UserInterface::impl::SubMenuValue) {
                if (enabledCallback()) {
                    callback();
                }
            } else if (menuItems.size() == 1) {
                bool enabled = enabledCallback();
                if (!shortcut.has(AllowWhileTyping) && ImGui::GetIO().WantTextInput)
                    enabled = false;

                if (menu::menuItemEx(Lang(name), icon, shortcut, selectedCallback(), enabled)) {
                    if (shortcut == Shortcut::None)
                        callback();
                    else {
                        if (!ShortcutManager::runShortcut(shortcut, View::getLastFocusedView())) {
                            if (!ShortcutManager::runShortcut(shortcut)) {
                                callback();
                            }
                        }
                    }
                }
            } else {
                bool isSubmenu = (menuItems.begin() + 1)->get() == ContentRegistry::UserInterface::impl::SubMenuValue;

                if (menu::beginMenuEx(Lang(name), std::next(menuItems.begin())->get() == ContentRegistry::UserInterface::impl::SubMenuValue ? icon : nullptr, isSubmenu ? enabledCallback() : true)) {
                    allowSeparator = true;
                    createNestedMenu({ std::next(menuItems.begin()), menuItems.end() }, icon, shortcut, view, callback, enabledCallback, selectedCallback);
                    menu::endMenu();
                }
            }

            allowSeparator = true;
        }

        void drawFooter(ImDrawList *drawList, ImVec2 dockSpaceSize) {
            auto dockId = ImGui::DockSpace(ImGui::GetID("ImHexMainDock"), dockSpaceSize);
            ImHexApi::System::impl::setMainDockSpaceId(dockId);

            ImGui::Separator();
            ImGui::SetCursorPosX(8);
            for (const auto &callback : ContentRegistry::UserInterface::impl::getFooterItems()) {
                const auto y = ImGui::GetCursorPosY();
                const auto prevIdx = drawList->_VtxCurrentIdx;
                callback();
                const auto currIdx = drawList->_VtxCurrentIdx;
                ImGui::SetCursorPosY(y);

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
            u32 drawIndex = 1;
            ImGui::PushID("SideBarWindows");
            for (const auto &[icon, callback, enabledCallback] : ContentRegistry::UserInterface::impl::getSidebarItems()) {
                ImGui::SetCursorPosY(sidebarPos.y + sidebarWidth * drawIndex);

                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabActive));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabHovered));


                if (enabledCallback()) {
                    drawIndex += 1;
                    ImGui::BeginDisabled(!ImHexApi::Provider::isValid());
                    {
                        if (ImGui::Button(icon.c_str(), ImVec2(sidebarWidth, sidebarWidth))) {
                            if (static_cast<u32>(openWindow) == index)
                                openWindow = -1;
                            else
                                openWindow = index;
                        }
                    }
                    ImGui::EndDisabled();
                }

                ImGui::PopStyleColor(3);

                auto sideBarFocused = ImGui::IsWindowFocused();

                bool open = static_cast<u32>(openWindow) == index;
                if (open) {
                    ImGui::SetNextWindowPos(ImGui::GetWindowPos() + sidebarPos + ImVec2(sidebarWidth - 1_scaled, -1_scaled));
                    ImGui::SetNextWindowSizeConstraints(ImVec2(0, dockSpaceSize.y + 5_scaled), ImVec2(FLT_MAX, dockSpaceSize.y + 5_scaled));

                    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1);
                    ImGui::PushStyleColor(ImGuiCol_WindowShadow, 0x00000000);
                    if (ImGui::Begin("SideBarWindow", &open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                        if (ImGui::BeginChild("##Content", ImGui::GetContentRegionAvail())) {
                            callback();
                        }
                        ImGui::EndChild();

                        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !sideBarFocused) {
                            openWindow = -1;
                        }

                        ImGuiExt::DisableWindowResize(ImGuiDir_Up);
                        ImGuiExt::DisableWindowResize(ImGuiDir_Down);
                        ImGuiExt::DisableWindowResize(ImGuiDir_Left);

                    }
                    ImGui::End();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                }

                ImGui::NewLine();
                index += 1;
            }
            ImGui::PopID();
        }

        void drawTitleBar() {
            auto titleBarHeight = ImGui::GetCurrentWindowRead()->MenuBarHeight;

            #if defined (OS_MACOS)
                titleBarHeight *= 0.7F;
            #endif

            auto buttonSize = ImVec2(titleBarHeight * 1.5F, titleBarHeight - 1);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabHovered));

            const auto windowSize = ImGui::GetWindowSize();
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

            const auto &titleBarButtons = ContentRegistry::UserInterface::impl::getTitlebarButtons();

            // Draw custom title bar buttons
            if (!titleBarButtons.empty()) {
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 7_scaled - (buttonSize.x + ImGui::GetStyle().ItemSpacing.x) * float((titleBarButtonsVisible ? 4 : 0) + titleBarButtons.size()));

                if (ImGui::GetCursorPosX() > (searchBoxPos.x + searchBoxSize.x)) {
                    for (const auto &[icon, color, tooltip, callback] : titleBarButtons) {
                        ImGui::SetCursorPosY(titleBarButtonPosY);
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiExt::GetCustomColorVec4(color));
                        if (ImGuiExt::TitleBarButton(icon.c_str(), buttonSize)) {
                            callback();
                        }
                        ImGui::PopStyleColor();
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

                    const auto style = ImGui::GetStyle();
                    fonts::Default().pushBold(0.8);
                    ImGui::GetWindowDrawList()->AddText(
                        ImGui::GetCursorScreenPos() + ImVec2(style.FramePadding.x, style.ChildBorderSize + (searchBoxSize.y - ImGui::CalcTextSize(ICON_VS_SEARCH).y) / 2),
                        ImGui::GetColorU32(ImGuiCol_Text),
                        ICON_VS_SEARCH
                    );
                    fonts::Default().pop();

                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, scaled({ 1, 1 }));

                    if (ImGui::Button(s_windowTitle.c_str(), searchBoxSize)) {
                        EventSearchBoxClicked::post(ImGuiMouseButton_Left);
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                        EventSearchBoxClicked::post(ImGuiMouseButton_Right);

                    ImGui::PushTextWrapPos(300_scaled);

                    if (auto provider = ImHexApi::Provider::get(); provider != nullptr) {
                        drawProviderTooltip(ImHexApi::Provider::get());
                    } else if (!s_windowTitleFull.empty()) {
                        if (ImGuiExt::InfoTooltip()) {
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted(s_windowTitleFull.c_str());
                            ImGui::EndTooltip();
                        }
                    }
                    ImGui::PopTextWrapPos();

                    ImGui::PopStyleVar(3);
                    ImGui::PopStyleColor(3);
                } else {
                    ImGui::TextUnformatted(s_windowTitle.c_str());
                }
            }
        }

        bool isMenuItemVisible(const View *lastFocusedView, const ContentRegistry::UserInterface::impl::MenuItem &menuItem) {
            if (!menuItem.view)
                return true;
            if (!lastFocusedView)
                return menuItem.shortcut.has(ShowOnWelcomeScreen);
            return menuItem.view == lastFocusedView || menuItem.view == lastFocusedView->getMenuItemInheritView();
        }

        std::set<UnlocalizedString> getVisibleMainMenus() {
            static std::set<UnlocalizedString> result;
            const auto lastFocusedView = View::getLastFocusedView();
            static std::optional<const View*> prevLastFocusedView;

            if (lastFocusedView == prevLastFocusedView)
                return result;

            prevLastFocusedView = lastFocusedView;
            result.clear();

            for (const auto &[priority, menuItem] : ContentRegistry::UserInterface::impl::getMenuItems()) {
                if (result.contains(menuItem.unlocalizedNames.front())) [[likely]]
                    continue;

                if (menuItem.unlocalizedNames.front().get().starts_with('$'))
                    continue;

                if (isMenuItemVisible(lastFocusedView, menuItem))
                    result.insert(menuItem.unlocalizedNames.front());
            }
            return result;
        }

        void populateMenu(const UnlocalizedString &menuName) {
            allowSeparator = false;

            const auto lastFocusedView = View::getLastFocusedView();
            for (auto &[priority, menuItem] : ContentRegistry::UserInterface::impl::getMenuItems()) {
                if (!menuName.empty()) {
                    if (menuItem.unlocalizedNames[0] != menuName)
                        continue;
                }

                if (!isMenuItemVisible(lastFocusedView, menuItem)) {
                    continue;
                }

                const auto &[
                    unlocalizedNames,
                    icon,
                    shortcut,
                    view,
                    callback,
                    enabledCallback,
                    selectedCallback,
                    toolbarIndex
                ] = menuItem;

                createNestedMenu(unlocalizedNames | std::views::drop(1), icon.glyph.c_str(), shortcut, view, callback, enabledCallback, selectedCallback);
            }
        }

        void defineMenu(const UnlocalizedString &menuName) {
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetColorU32(ImGuiCol_HeaderHovered, 0.75F));
            ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 2_scaled);
            const auto menuOpen = menu::beginMenu(Lang(menuName));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            if (menuOpen) {
                populateMenu(menuName);
                menu::endMenu();
            } else {
                if (s_displayShortcutHighlights) {
                    if (const auto lastShortcutMenu = ShortcutManager::getLastActivatedMenu(); lastShortcutMenu.has_value()) {
                        if (menuName == *lastShortcutMenu) {
                            ImGui::NavHighlightActivated(ImGui::GetItemID());
                        }
                    }
                }
            }

        }

        void drawMenu() {
            const auto &menuItems = ContentRegistry::UserInterface::impl::getMainMenuItems();
            const auto visibleMainMenus = getVisibleMainMenus();

            if (menu::isNativeMenuBarUsed()) {
                for (const auto &[priority, menuItem] : menuItems) {
                    if (visibleMainMenus.contains(menuItem.unlocalizedName))
                        defineMenu(menuItem.unlocalizedName);
                }

                if (menu::beginTaskBarMenu()) {
                    populateMenu(ContentRegistry::UserInterface::impl::TaskBarMenuValue);
                    menu::endTaskBarMenu();
                }
            } else {
                auto cursorPos = ImGui::GetCursorPosX();
                u32 fittingItems = 0;

                for (const auto &[priority, menuItem] : menuItems) {
                    if (!visibleMainMenus.contains(menuItem.unlocalizedName))
                        continue;

                    auto menuName = Lang(menuItem.unlocalizedName);

                    const auto padding = ImGui::GetStyle().FramePadding.x;
                    bool lastItem = (fittingItems + 1) == visibleMainMenus.size();
                    auto width = ImGui::CalcTextSize(menuName).x + padding * (lastItem ? -3.0F : 4.0F);

                    if ((cursorPos + width) > (s_searchBarPosition - ImGui::CalcTextSize(ICON_VS_ELLIPSIS).x - padding * 2))
                        break;

                    cursorPos += width;
                    fittingItems += 1;
                }

                if (fittingItems <= 2)
                    fittingItems = 0;

                {
                    u32 count = 0;
                    for (const auto &[priority, menuItem] : menuItems) {
                        if (count >= fittingItems)
                            break;
                        if (!visibleMainMenus.contains(menuItem.unlocalizedName))
                            continue;

                        defineMenu(menuItem.unlocalizedName);

                        count += 1;
                    }
                }

                if (fittingItems == 0) {
                    if (ImGui::BeginMenu(ICON_VS_MENU)) {
                        for (const auto &[priority, menuItem] : menuItems) {
                            if (visibleMainMenus.contains(menuItem.unlocalizedName))
                                defineMenu(menuItem.unlocalizedName);
                        }
                        ImGui::EndMenu();
                    }
                } else if (fittingItems < visibleMainMenus.size()) {
                    u32 count = 0;
                    if (ImGui::BeginMenu(ICON_VS_ELLIPSIS)) {
                        for (const auto &[priority, menuItem] : menuItems) {
                            ON_SCOPE_EXIT { count += 1; };
                            if (count <= fittingItems)
                                continue;
                            if (!visibleMainMenus.contains(menuItem.unlocalizedName))
                                continue;

                            defineMenu(menuItem.unlocalizedName);
                        }
                        ImGui::EndMenu();
                    }
                }
            }
        }

        void drawMainMenu([[maybe_unused]] float menuBarHeight) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0F);
            ImGui::SetNextWindowScroll(ImVec2(0, 0));

            #if defined(OS_MACOS)
                ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 8_scaled);
                ON_SCOPE_EXIT { ImGui::PopStyleVar(); };
            #endif

            if (ImGui::BeginMainMenuBar()) {
                drawTitleBarBackDrop();
                ImGui::Dummy({});
                ImGui::EndMainMenuBar();
            }

            auto window = ImHexApi::System::getMainWindowHandle();
            menu::enableNativeMenuBar(s_useNativeMenuBar);
            if (menu::beginMainMenuBar()) {
                if (ImHexApi::System::isBorderlessWindowModeEnabled()) {
                    #if defined(OS_WINDOWS)
                        ImGui::SetCursorPosX(5_scaled);
                        ImGui::Image(*s_logoTexture, s_logoTexture->getSize() * 0.1_scaled);
                        ImGui::SetCursorPosX(5_scaled);
                        ImGui::InvisibleButton("##logo", ImVec2(menuBarHeight, menuBarHeight));
                        if (ImGui::IsItemHovered() && ImGui::IsAnyMouseDown())
                            ImGui::OpenPopup("WindowingMenu");
                    #elif defined(OS_MACOS)
                        if (!isMacosFullScreenModeEnabled(window))
                            ImGui::SetCursorPosX(80_scaled);
                    #endif
                }

                if (ImGui::BeginPopup("WindowingMenu")) {
                    bool maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED);

                    ImGui::BeginDisabled(!maximized);
                    if (ImGui::MenuItemEx("Restore", ICON_VS_CHROME_RESTORE)) glfwRestoreWindow(window);
                    ImGui::EndDisabled();

                    if (ImGui::MenuItemEx("Minimize", ICON_VS_CHROME_MINIMIZE)) glfwIconifyWindow(window);

                    ImGui::BeginDisabled(maximized);
                    if (ImGui::MenuItemEx("Maximize", ICON_VS_CHROME_MAXIMIZE)) glfwMaximizeWindow(window);
                    ImGui::EndDisabled();

                    ImGui::Separator();

                    if (ImGui::MenuItemEx("Close", ICON_VS_CHROME_CLOSE)) ImHexApi::System::closeImHex();

                    ImGui::EndPopup();
                }

                {
                    ImGui::BeginDisabled(ContentRegistry::Views::impl::getFullScreenView() != nullptr);
                    drawMenu();
                    ImGui::EndDisabled();
                }
                menu::endMainMenuBar();
            }
            menu::enableNativeMenuBar(false);

            if (ImGui::BeginMainMenuBar()) {
                ImGui::Dummy({});

                ImGui::PopStyleVar(2);

                drawTitleBar();

                #if defined(OS_MACOS)
                    if (ImHexApi::System::isBorderlessWindowModeEnabled()) {
                        const auto windowSize = ImHexApi::System::getMainWindowSize();
                        const auto menuUnderlaySize = ImVec2(windowSize.x, ImGui::GetCurrentWindowRead()->MenuBarHeight);
                        
                        ImGui::SetCursorPos(ImVec2());

                        // Prevent window from being moved unless title bar is hovered

                        if (!ImGui::IsAnyItemHovered()) {
                            const auto cursorPos = ImGui::GetCursorScreenPos();
                            if (ImGui::IsMouseHoveringRect(cursorPos, cursorPos + menuUnderlaySize) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                macosHandleTitlebarDoubleClickGesture(window);
                            }

                            macosSetWindowMovable(window, true);
                        } else {
                            macosSetWindowMovable(window, false);

                        }
                    }
                #endif
                
                ImGui::EndMainMenuBar();
            } else {
                ImGui::PopStyleVar(2);
            }
        }

        void drawToolbar() {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0F);
            if (ImGui::BeginMenuBar()) {
                drawTitleBarBackDrop();

                ImGui::BeginDisabled(ContentRegistry::Views::impl::getFullScreenView() != nullptr);
                {
                    for (const auto &callback : ContentRegistry::UserInterface::impl::getToolbarItems()) {
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
                }
                ImGui::EndDisabled();

                ImGui::EndMenuBar();
            }
            ImGui::PopStyleVar(2);
        }

        bool anySidebarItemsAvailable() {
            if (const auto &items = ContentRegistry::UserInterface::impl::getSidebarItems(); items.empty()) {
                return false;
            } else {
                return std::ranges::any_of(items, [](const auto &item) {
                    return item.enabledCallback();
                });
            }
        }

        bool isAnyViewOpen() {
            const auto &views = ContentRegistry::Views::impl::getEntries();
            return std::ranges::any_of(views,
                               [](const auto &entry) {
                                   return entry.second->getWindowOpenState();
                               });
        }

    }

    void addWindowDecoration() {
        EventFrameBegin::subscribe([]{
            AT_FIRST_TIME {
                s_logoTexture = ImGuiExt::Texture::fromImage(romfs::get("assets/common/icon.png").span(), ImGuiExt::Texture::Filter::Linear);
            };

            constexpr static ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

            ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(ImHexApi::System::getMainWindowSize() - ImVec2(0, ImGui::GetTextLineHeightWithSpacing()));
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);

            // Draw main window decoration
            if (ImGui::Begin("ImHexDockSpace", nullptr, windowFlags)) {
                ImGui::PopStyleVar(2);

                const auto drawList = ImGui::GetWindowDrawList();
                const auto shouldDrawSidebar = anySidebarItemsAvailable();

                const auto menuBarHeight = ImGui::GetCurrentWindowRead()->MenuBarHeight;
                const auto sidebarPos   = ImGui::GetCursorPos();
                const auto sidebarWidth = shouldDrawSidebar ? 20_scaled : 0;

                auto footerHeight  = ImGui::GetTextLineHeightWithSpacing() + 1_scaled;
                #if defined(OS_MACOS)
                    footerHeight += ImGui::GetStyle().WindowPadding.y * 2;
                #else
                    footerHeight += ImGui::GetStyle().FramePadding.y * 2;
                #endif

                const auto dockSpaceSize = ImHexApi::System::getMainWindowSize() - ImVec2(sidebarWidth, menuBarHeight * 2 + footerHeight);

                ImGui::SetCursorPosX(sidebarWidth);
                drawFooter(drawList, dockSpaceSize);

                if (shouldDrawSidebar) {
                    // Draw sidebar background and separator
                    drawList->AddRectFilled(
                        ImGui::GetWindowPos() - ImVec2(0, ImGui::GetStyle().FramePadding.y + 1_scaled),
                        ImGui::GetWindowPos() + ImGui::GetWindowSize() - ImVec2(dockSpaceSize.x, footerHeight - ImGui::GetStyle().FramePadding.y + 1_scaled),
                        ImGui::GetColorU32(ImGuiCol_MenuBarBg)
                    );

                    ImGui::SetCursorPos(sidebarPos);
                    drawSidebar(dockSpaceSize, sidebarPos, sidebarWidth);

                    if (ImHexApi::Provider::isValid() && isAnyViewOpen()) {
                        drawList->AddLine(
                                ImGui::GetWindowPos() + sidebarPos + ImVec2(sidebarWidth - 1_scaled, menuBarHeight - 2_scaled),
                                ImGui::GetWindowPos() + sidebarPos + ImGui::GetWindowSize() - ImVec2(dockSpaceSize.x + 1_scaled, footerHeight - ImGui::GetStyle().FramePadding.y + 2_scaled + menuBarHeight),
                                ImGui::GetColorU32(ImGuiCol_Separator));
                    }
                }

                drawMainMenu(menuBarHeight);
                drawToolbar();
            } else {
                ImGui::PopStyleVar(2);
            }
            ImGui::End();

            ImGui::PopStyleVar(2);

            // Draw main menu popups
            for (auto &[priority, menuItem] : ContentRegistry::UserInterface::impl::getMainMenuItems()) {
                const auto &unlocalizedNames = menuItem.unlocalizedName;
                if (ImGui::BeginPopup(unlocalizedNames.get().c_str())) {
                    populateMenu(unlocalizedNames);
                    ImGui::EndPopup();
                }
            }
        });

        constexpr static auto DefaultImHexTitle = "ImHex";
        static std::string s_applicationName = DefaultImHexTitle;

        s_windowTitle = DefaultImHexTitle;

        // Handle updating the window title
        RequestUpdateWindowTitle::subscribe([] {
            std::string prefix, postfix;
            std::string title = DefaultImHexTitle;

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

                    if (!provider->isWritable())
                        postfix += " (Read Only)";
                }
            }

            s_windowTitle     = prefix + hex::limitStringLength(title, 32) + postfix;
            s_windowTitleFull = prefix + title + postfix;

            auto window = ImHexApi::System::getMainWindowHandle();
            if (window != nullptr) {
                if (title != DefaultImHexTitle)
                    title = std::string(DefaultImHexTitle) + " - " + title;

                title = wolv::util::replaceStrings(title, DefaultImHexTitle, s_applicationName);

                TaskManager::doLater([window, title] {
                    glfwSetWindowTitle(window, title.c_str());
                });
            }
        });

        EventProviderDirtied::subscribe([] {
            RequestUpdateWindowTitle::post();
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.interface", "hex.builtin.setting.interface.randomize_window_title", [](const ContentRegistry::Settings::SettingsValue &value) {
            const bool randomTitle = value.get<bool>(false);
            if (randomTitle) {
                s_applicationName.clear();
                std::mt19937_64 rng(std::random_device{}());
                for (u32 i = 0; i < 24; i += 1) {
                    s_applicationName += char(rng() % ('Z' - 'A' + 1)) + 'A';
                }
            } else {
                s_applicationName = DefaultImHexTitle;
            }
            RequestUpdateWindowTitle::post();
        });
    }


}
