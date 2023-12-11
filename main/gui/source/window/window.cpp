#include "window.hpp"

#include <hex.hpp>

#include <hex/api/plugin_manager.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/layout_manager.hpp>
#include <hex/api/shortcut_manager.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/stacktrace.hpp>

#include <hex/ui/view.hpp>
#include <hex/ui/popup.hpp>

#include <chrono>
#include <csignal>

#include <romfs/romfs.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <implot.h>
#include <implot_internal.h>
#include <imnodes.h>
#include <imnodes_internal.h>

#include <wolv/utils/string.hpp>

#include <fonts/codicons_font.h>

#include <hex/api/project_file_manager.hpp>

#include <GLFW/glfw3.h>

namespace hex {

    using namespace std::literals::chrono_literals;

    static std::fs::path s_imguiSettingsPath;

    /**
     * @brief returns the path to load/save imgui settings to, or an empty path if no location was found
     */
    std::fs::path getImGuiSettingsPath() {
        return s_imguiSettingsPath;
    }

    Window::Window() {
        stacktrace::initialize();

        constexpr static auto openEmergencyPopup = [](const std::string &title){
            TaskManager::doLater([title] {
                for (const auto &provider : ImHexApi::Provider::getProviders())
                    ImHexApi::Provider::remove(provider, false);

                ImGui::OpenPopup(title.c_str());
            });
        };

        // Handle fatal error popups for errors detected during initialization
        {
            for (const auto &[argument, value] : ImHexApi::System::getInitArguments()) {
                if (argument == "no-plugins") {
                    openEmergencyPopup("No Plugins");
                } else if (argument == "no-builtin-plugin") {
                    openEmergencyPopup("No Builtin Plugin");
                } else if (argument == "multiple-builtin-plugins") {
                    openEmergencyPopup("Multiple Builtin Plugins");
                }
            }
        }

        // Initialize the window
        this->initGLFW();
        this->initImGui();
        this->setupNativeWindow();
        this->registerEventHandlers();

        this->m_logoTexture = ImGuiExt::Texture(romfs::get("logo.png").span(), ImGuiExt::Texture::Filter::Linear);

        ContentRegistry::Settings::impl::store();
        EventSettingsChanged::post();
        EventWindowInitialized::post();
        EventImHexStartupFinished::post();
    }

    Window::~Window() {
        EventProviderDeleted::unsubscribe(this);
        RequestCloseImHex::unsubscribe(this);
        RequestUpdateWindowTitle::unsubscribe(this);
        EventAbnormalTermination::unsubscribe(this);
        RequestOpenPopup::unsubscribe(this);

        this->exitImGui();
        this->exitGLFW();
    }

    void Window::registerEventHandlers() {
        // Initialize default theme
        RequestChangeTheme::post("Dark");

        // Handle the close window request by telling GLFW to shut down
        RequestCloseImHex::subscribe(this, [this](bool noQuestions) {
            glfwSetWindowShouldClose(this->m_window, GLFW_TRUE);

            if (!noQuestions)
                EventWindowClosing::post(this->m_window);
        });

        // Handle updating the window title
        RequestUpdateWindowTitle::subscribe(this, [this] {
            std::string title = "ImHex";

            if (ProjectFile::hasPath()) {
                // If a project is open, show the project name instead of the file name

                title = "Project " + hex::limitStringLength(ProjectFile::getPath().stem().string(), 32);

                if (ImHexApi::Provider::isDirty())
                    title += " (*)";

            } else if (ImHexApi::Provider::isValid()) {
                auto provider = ImHexApi::Provider::get();
                if (provider != nullptr) {
                    title = hex::limitStringLength(provider->getName(), 32);

                    if (provider->isDirty())
                        title += " (*)";

                    if (!provider->isWritable())
                        title += " (Read Only)";
                }
            }

            this->m_windowTitle = title;

            if (this->m_window != nullptr) {
                if (title != "ImHex")
                    title = "ImHex - " + title;

                glfwSetWindowTitle(this->m_window, title.c_str());
            }
        });

        // Handle opening popups
        RequestOpenPopup::subscribe(this, [this](auto name) {
            std::scoped_lock lock(this->m_popupMutex);

            this->m_popupsToOpen.push_back(name);
        });
    }

    void Window::fullFrame() {
        this->m_lastStartFrameTime = glfwGetTime();

        glfwPollEvents();

        // Render frame
        this->frameBegin();
        this->frame();
        this->frameEnd();
    }

    void Window::loop() {
        u64 frameCount = 0;
        while (!glfwWindowShouldClose(this->m_window)) {
            this->m_lastStartFrameTime = glfwGetTime();

            if (!glfwGetWindowAttrib(this->m_window, GLFW_VISIBLE) || glfwGetWindowAttrib(this->m_window, GLFW_ICONIFIED)) {
                // If the application is minimized or not visible, don't render anything
                glfwWaitEvents();
            } else {
                // If no events have been received in a while, lower the frame rate
                {
                    // If the mouse is down, the mouse is moving or a popup is open, we don't want to lower the frame rate
                    bool frameRateUnlocked =
                            ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopupId) ||
                            TaskManager::getRunningTaskCount() > 0 ||
                            this->m_buttonDown ||
                            this->m_hadEvent ||
                            !this->m_pressedKeys.empty() ||
                            frameCount < 100;

                    // Calculate the time until the next frame
                    const double timeout = std::max(0.0, (1.0 / 5.0) - (glfwGetTime() - this->m_lastStartFrameTime));

                    // If the frame rate has been unlocked for 5 seconds, lock it again
                    if ((this->m_lastStartFrameTime - this->m_frameRateUnlockTime) > 5 && this->m_frameRateTemporarilyUnlocked && !frameRateUnlocked) {
                        this->m_frameRateTemporarilyUnlocked = false;
                    }

                    // If the frame rate is locked, wait for events with a timeout
                    if (frameRateUnlocked || this->m_frameRateTemporarilyUnlocked) {
                        if (!this->m_frameRateTemporarilyUnlocked) {
                            this->m_frameRateTemporarilyUnlocked = true;
                            this->m_frameRateUnlockTime = this->m_lastStartFrameTime;
                        }
                    } else {
                        glfwWaitEventsTimeout(timeout);
                    }

                    this->m_hadEvent = false;
                }
            }

            this->fullFrame();
            frameCount += 1;

            // Limit frame rate
            // If the target FPS are below 15, use the monitor refresh rate, if it's above 200, don't limit the frame rate
            const auto targetFPS = ImHexApi::System::getTargetFPS();
            if (targetFPS < 15) {
                glfwSwapInterval(1);
            } else if (targetFPS > 200) {
                glfwSwapInterval(0);
            } else {
                glfwSwapInterval(0);
                const auto frameTime = glfwGetTime() - this->m_lastStartFrameTime;
                const auto targetFrameTime = 1.0 / targetFPS;
                if (frameTime < targetFrameTime) {
                    glfwWaitEventsTimeout(targetFrameTime - frameTime);
                }
            }

            this->m_lastFrameTime = glfwGetTime() - this->m_lastStartFrameTime;
        }
    }

    static void createNestedMenu(std::span<const std::string> menuItems, const Shortcut &shortcut, const std::function<void()> &callback, const std::function<bool()> &enabledCallback) {
        const auto &name = menuItems.front();

        if (name == ContentRegistry::Interface::impl::SeparatorValue) {
            ImGui::Separator();
            return;
        }

        if (name == ContentRegistry::Interface::impl::SubMenuValue) {
            callback();
        } else if (menuItems.size() == 1) {
            if (ImGui::MenuItem(Lang(name), shortcut.toString().c_str(), false, enabledCallback()))
                callback();
        } else {
            bool isSubmenu = *(menuItems.begin() + 1) == ContentRegistry::Interface::impl::SubMenuValue;

            if (ImGui::BeginMenu(Lang(name), isSubmenu ? enabledCallback() : true)) {
                createNestedMenu({ menuItems.begin() + 1, menuItems.end() }, shortcut, callback, enabledCallback);
                ImGui::EndMenu();
            }
        }
    }

    void Window::drawTitleBar() const {
        auto titleBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight();
        auto buttonSize = ImVec2(titleBarHeight * 1.5F, titleBarHeight - 1);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabHovered));

        const auto windowSize = ImHexApi::System::getMainWindowSize();
        const auto searchBoxSize = ImVec2(std::sqrt(windowSize.x) * 14_scaled, titleBarHeight - 3_scaled);
        const auto searchBoxPos = ImVec2((windowSize / 2 - searchBoxSize / 2).x, 3_scaled);

        // Custom titlebar buttons implementation for borderless window mode
        auto &titleBarButtons = ContentRegistry::Interface::impl::getTitleBarButtons();

        // Draw custom title bar buttons
        if (!titleBarButtons.empty()) {
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - buttonSize.x * float(4 + titleBarButtons.size()));

            if (ImGui::GetCursorPosX() > (searchBoxPos.x + searchBoxSize.x)) {
                for (const auto &[icon, tooltip, callback]: titleBarButtons) {
                    if (ImGuiExt::TitleBarButton(icon.c_str(), buttonSize)) {
                        callback();
                    }
                    ImGuiExt::InfoTooltip(Lang(tooltip));
                }
            }
        }

        if (ImHexApi::System::isBorderlessWindowModeEnabled()) {
            // Draw minimize, restore and maximize buttons
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - buttonSize.x * 3);
            if (ImGuiExt::TitleBarButton(ICON_VS_CHROME_MINIMIZE, buttonSize))
                glfwIconifyWindow(this->m_window);
            if (glfwGetWindowAttrib(this->m_window, GLFW_MAXIMIZED)) {
                if (ImGuiExt::TitleBarButton(ICON_VS_CHROME_RESTORE, buttonSize))
                    glfwRestoreWindow(this->m_window);
            } else {
                if (ImGuiExt::TitleBarButton(ICON_VS_CHROME_MAXIMIZE, buttonSize))
                    glfwMaximizeWindow(this->m_window);
            }

            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0xFF7A70F1);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0xFF2311E8);

            // Draw close button
            if (ImGuiExt::TitleBarButton(ICON_VS_CHROME_CLOSE, buttonSize)) {
                ImHexApi::System::closeImHex();
            }

            ImGui::PopStyleColor(2);
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        {
            const auto buttonColor = [](float alpha) {
                return ImU32(ImColor(ImGui::GetStyleColorVec4(ImGuiCol_DockingEmptyBg) * ImVec4(1, 1, 1, alpha)));
            };

            ImGui::PushStyleColor(ImGuiCol_Button, buttonColor(0.5F));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonColor(0.7F));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonColor(0.9F));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0_scaled);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4_scaled);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, scaled({ 1, 1 }));

            ImGui::SetCursorPos(searchBoxPos);
            if (ImGui::Button(this->m_windowTitle.c_str(), searchBoxSize)) {
                EventSearchBoxClicked::post();
            }

            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(3);
        }
    }

    static bool isAnyViewOpen() {
        const auto &views = ContentRegistry::Views::impl::getEntries();
        return std::any_of(views.begin(), views.end(),
                           [](const auto &entry) {
                               return entry.second->getWindowOpenState();
                           });
    }

    void Window::frameBegin() {
        // Start new ImGui Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Handle all undocked floating windows
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(ImHexApi::System::getMainWindowSize() - ImVec2(0, ImGui::GetTextLineHeightWithSpacing()));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Render main dock space
        if (ImGui::Begin("ImHexDockSpace", nullptr, windowFlags)) {
            auto drawList = ImGui::GetWindowDrawList();
            ImGui::PopStyleVar();

            bool shouldDrawSidebar = [] {
                if (const auto &items = ContentRegistry::Interface::impl::getSidebarItems(); items.empty())
                    return false;
                else {
                    return std::any_of(items.begin(), items.end(), [](const auto &item) {
                        return item.enabledCallback();
                    });
                }
            }();

            const auto menuBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight();
            auto sidebarPos   = ImGui::GetCursorPos();
            auto sidebarWidth = shouldDrawSidebar ? 20_scaled : 0;

            ImGui::SetCursorPosX(sidebarWidth);

            auto footerHeight  = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2 + 1_scaled;
            auto dockSpaceSize = ImVec2(ImHexApi::System::getMainWindowSize().x - sidebarWidth, ImGui::GetContentRegionAvail().y - footerHeight);

            // Render footer
            {

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

            // Render sidebar
            if (shouldDrawSidebar) {
                ImGui::SetCursorPos(sidebarPos);

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

            // Render main menu
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
            ImGui::SetNextWindowScroll(ImVec2(0, 0));
            if (ImGui::BeginMainMenuBar()) {

                if (ImHexApi::System::isBorderlessWindowModeEnabled()) {
                    ImGui::SetCursorPosX(5);

                    ImGui::Image(this->m_logoTexture, ImVec2(menuBarHeight, menuBarHeight));
                    ImGui::SetCursorPosX(5);
                    ImGui::InvisibleButton("##logo", ImVec2(menuBarHeight, menuBarHeight));
                    ImGui::OpenPopupOnItemClick("WindowingMenu", ImGuiPopupFlags_MouseButtonLeft);
                }

                if (ImGui::BeginPopup("WindowingMenu")) {
                    bool maximized = glfwGetWindowAttrib(this->m_window, GLFW_MAXIMIZED);

                    ImGui::BeginDisabled(!maximized);
                    if (ImGui::MenuItem(ICON_VS_CHROME_RESTORE " Restore"))  glfwRestoreWindow(this->m_window);
                    ImGui::EndDisabled();

                    if (ImGui::MenuItem(ICON_VS_CHROME_MINIMIZE " Minimize")) glfwIconifyWindow(this->m_window);

                    ImGui::BeginDisabled(maximized);
                    if (ImGui::MenuItem(ICON_VS_CHROME_MAXIMIZE " Maximize")) glfwMaximizeWindow(this->m_window);
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
                        const auto &[unlocalizedNames, shortcut, view, callback, enabledCallback] = menuItem;
                        createNestedMenu(unlocalizedNames, *shortcut, callback, enabledCallback);
                    }
                };

                const auto windowWidth = ImHexApi::System::getMainWindowSize().x;
                if (windowWidth > 1200_scaled) {
                    drawMenu();
                } else {
                    if (ImGui::BeginMenu(ICON_VS_MENU)) {
                        drawMenu();
                        ImGui::EndMenu();
                    }
                }

                this->drawTitleBar();

                ImGui::EndMainMenuBar();
            }
            ImGui::PopStyleVar();

            // Render toolbar
            if (ImGui::BeginMenuBar()) {

                for (const auto &callback : ContentRegistry::Interface::impl::getToolbarItems()) {
                    callback();
                    ImGui::SameLine();
                }

                if (auto provider = ImHexApi::Provider::get(); provider != nullptr) {
                    if (ImGui::CloseButton(ImGui::GetID("ProviderCloseButton"), ImGui::GetCursorScreenPos() + ImVec2(ImGui::GetContentRegionAvail().x - 17_scaled, 3_scaled))) {
                        ImHexApi::Provider::remove(provider);
                    }
                }

                ImGui::EndMenuBar();
            }

            this->beginNativeWindowFrame();

            if (ImHexApi::Provider::isValid() && isAnyViewOpen()) {
                drawList->AddLine(
                        ImGui::GetWindowPos() + sidebarPos + ImVec2(sidebarWidth - 1_scaled, -2_scaled),
                        ImGui::GetWindowPos() + sidebarPos + ImGui::GetWindowSize() - ImVec2(dockSpaceSize.x + 1_scaled, footerHeight - ImGui::GetStyle().FramePadding.y - 1_scaled + menuBarHeight),
                        ImGui::GetColorU32(ImGuiCol_Separator));
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);

        // Plugin load error popups. These are not translated because they should always be readable, no matter if any localization could be loaded or not
        {
            auto drawPluginFolderTable = [] {
                ImGuiExt::UnderlinedText("Plugin folders");
                if (ImGui::BeginTable("plugins", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit, ImVec2(0, 100_scaled))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 0.2);
                    ImGui::TableSetupColumn("Exists", ImGuiTableColumnFlags_WidthFixed, ImGui::GetTextLineHeight() * 3);

                    ImGui::TableHeadersRow();

                    for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Plugins, true)) {
                        const auto filePath = path / "builtin.hexplug";
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(wolv::util::toUTF8String(filePath).c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(wolv::io::fs::exists(filePath) ? ICON_VS_CHECK : ICON_VS_CLOSE);
                    }
                    ImGui::EndTable();
                }
            };

            // No plugins error popup
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
            if (ImGui::BeginPopupModal("No Plugins", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
                ImGui::TextUnformatted("No ImHex plugins loaded (including the built-in plugin)!");
                ImGui::TextUnformatted("Make sure you installed ImHex correctly.");
                ImGui::TextUnformatted("There should be at least a 'builtin.hexplug' file in your plugins folder.");

                ImGui::NewLine();

                drawPluginFolderTable();

                ImGui::NewLine();
                if (ImGui::Button("Close ImHex", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                    ImHexApi::System::closeImHex(true);

                ImGui::EndPopup();
            }

            // No built-in plugin error popup
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
            if (ImGui::BeginPopupModal("No Builtin Plugin", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
                ImGui::TextUnformatted("The ImHex built-in plugins could not be loaded!");
                ImGui::TextUnformatted("Make sure you installed ImHex correctly.");
                ImGui::TextUnformatted("There should be at least a 'builtin.hexplug' file in your plugins folder.");

                ImGui::NewLine();

                drawPluginFolderTable();

                ImGui::NewLine();
                if (ImGui::Button("Close ImHex", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                    ImHexApi::System::closeImHex(true);

                ImGui::EndPopup();
            }

            // Multiple built-in plugins error popup
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
            if (ImGui::BeginPopupModal("Multiple Builtin Plugins", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
                ImGui::TextUnformatted("ImHex found and attempted to load multiple built-in plugins!");
                ImGui::TextUnformatted("Make sure you installed ImHex correctly and, if needed,");
                ImGui::TextUnformatted("cleaned up older installations correctly,");
                ImGui::TextUnformatted("There should be exactly one 'builtin.hexplug' file in any one your plugin folders.");

                ImGui::NewLine();

                drawPluginFolderTable();

                ImGui::NewLine();
                if (ImGui::Button("Close ImHex", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                    ImHexApi::System::closeImHex(true);

                ImGui::EndPopup();
            }
        }

        // Open popups when plugins requested it
        {
            std::scoped_lock lock(this->m_popupMutex);
            this->m_popupsToOpen.remove_if([](const auto &name) {
                if (ImGui::IsPopupOpen(name.c_str()))
                    return true;
                else
                    ImGui::OpenPopup(name.c_str());

                return false;
            });
        }

        // Draw popup stack
        {
            static bool popupDisplaying = false;
            static bool positionSet = false;
            static bool sizeSet = false;
            static double popupDelay = -2.0;

            static std::unique_ptr<impl::PopupBase> currPopup;
            static Lang name("");

            if (auto &popups = impl::PopupBase::getOpenPopups(); !popups.empty()) {
                if (!ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopupId)) {
                    if (popupDelay <= -1.0) {
                        popupDelay = 0.2;
                    } else {
                        popupDelay -= this->m_lastFrameTime;
                        if (popupDelay < 0 || popups.size() == 1) {
                            popupDelay = -2.0;
                            currPopup = std::move(popups.back());
                            name = Lang(currPopup->getUnlocalizedName());

                            ImGui::OpenPopup(name);
                            popups.pop_back();
                        }
                    }
                }
            }

            if (currPopup != nullptr) {
                bool open = true;

                const auto &minSize = currPopup->getMinSize();
                const auto &maxSize = currPopup->getMaxSize();
                const bool hasConstraints = minSize.x != 0 && minSize.y != 0 && maxSize.x != 0 && maxSize.y != 0;

                if (hasConstraints)
                    ImGui::SetNextWindowSizeConstraints(minSize, maxSize);
                else
                    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_Appearing);

                auto* closeButton = currPopup->hasCloseButton() ? &open : nullptr;

                const auto flags = currPopup->getFlags() | (!hasConstraints ? (ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize) : ImGuiWindowFlags_None);

                if (!positionSet) {
                    ImGui::SetNextWindowPos(ImHexApi::System::getMainWindowPosition() + (ImHexApi::System::getMainWindowSize() / 2.0F), ImGuiCond_Always, ImVec2(0.5F, 0.5F));

                    if (sizeSet)
                        positionSet = true;
                }

                const auto createPopup = [&](bool displaying) {
                    if (displaying) {
                        currPopup->drawContent();
                        popupDisplaying = true;

                        if (ImGui::GetWindowSize().x > ImGui::GetStyle().FramePadding.x * 10)
                            sizeSet = true;

                        // Reset popup position if it's outside the main window when multi-viewport is not enabled
                        // If not done, the popup will be stuck outside the main window and cannot be accessed anymore
                        if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == ImGuiConfigFlags_None) {
                            const auto currWindowPos = ImGui::GetWindowPos();
                            const auto minWindowPos = ImHexApi::System::getMainWindowPosition() - ImGui::GetWindowSize();
                            const auto maxWindowPos = ImHexApi::System::getMainWindowPosition() + ImHexApi::System::getMainWindowSize();
                            if (currWindowPos.x > maxWindowPos.x || currWindowPos.y > maxWindowPos.y || currWindowPos.x < minWindowPos.x || currWindowPos.y < minWindowPos.y) {
                                positionSet = false;
                                GImGui->MovingWindow = nullptr;
                            }
                        }

                        ImGui::EndPopup();
                    } else {
                        popupDisplaying = false;
                    }
                };

                if (currPopup->isModal())
                    createPopup(ImGui::BeginPopupModal(name, closeButton, flags));
                else
                    createPopup(ImGui::BeginPopup(name, flags));

                if (!popupDisplaying || currPopup->shouldClose()) {
                    log::debug("Closing popup '{}'", name);
                    positionSet = sizeSet = false;

                    currPopup = nullptr;
                }
            }
        }

        // Run all deferred calls
        TaskManager::runDeferredCalls();

        // Draw main menu popups
        for (auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMenuItems()) {
            const auto &[unlocalizedNames, shortcut, view, callback, enabledCallback] = menuItem;

            if (ImGui::BeginPopup(unlocalizedNames.front().c_str())) {
                createNestedMenu({ unlocalizedNames.begin() + 1, unlocalizedNames.end() }, *shortcut, callback, enabledCallback);
                ImGui::EndPopup();
            }
        }

        EventFrameBegin::post();
    }

    void Window::frame() {
        auto &io = ImGui::GetIO();

        // Loop through all views and draw them
        for (auto &[name, view] : ContentRegistry::Views::impl::getEntries()) {
            ImGui::GetCurrentContext()->NextWindowData.ClearFlags();

            // Draw always visible views
            view->drawAlwaysVisibleContent();

            // Skip views that shouldn't be processed currently
            if (!view->shouldProcess())
                continue;

            const auto openViewCount = std::ranges::count_if(ContentRegistry::Views::impl::getEntries(), [](const auto &entry) {
                const auto &[unlocalizedName, view] = entry;

                return view->hasViewMenuItemEntry() && view->shouldProcess();
            });

            ImGuiWindowClass windowClass = {};

            windowClass.DockNodeFlagsOverrideSet |= ImGuiDockNodeFlags_NoCloseButton;

            if (openViewCount <= 1 || LayoutManager::isLayoutLocked())
                windowClass.DockNodeFlagsOverrideSet |= ImGuiDockNodeFlags_NoTabBar;

            ImGui::SetNextWindowClass(&windowClass);

            // Draw view
            view->draw();

            if (view->getWindowOpenState()) {
                auto window    = ImGui::FindWindowByName(view->getName().c_str());
                bool hasWindow = window != nullptr;
                bool focused   = false;

                // Get the currently focused view
                if (hasWindow && (window->Flags & ImGuiWindowFlags_Popup) != ImGuiWindowFlags_Popup) {
                    auto windowName = View::toWindowName(name);
                    ImGui::Begin(windowName.c_str());

                    // Detect if the window is focused
                    focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_NoPopupHierarchy);

                    // Dock the window if it's not already docked
                    if (view->didWindowJustOpen() && !ImGui::IsWindowDocked()) {
                        ImGui::DockBuilderDockWindow(windowName.c_str(), ImHexApi::System::getMainDockSpaceId());
                    }

                    ImGui::End();
                }

                // Pass on currently pressed keys to the shortcut handler
                for (const auto &key : this->m_pressedKeys) {
                    ShortcutManager::process(view, io.KeyCtrl, io.KeyAlt, io.KeyShift, io.KeySuper, focused, key);
                }
            }
        }

        // Handle global shortcuts
        for (const auto &key : this->m_pressedKeys) {
            ShortcutManager::processGlobals(io.KeyCtrl, io.KeyAlt, io.KeyShift, io.KeySuper, key);
        }

        this->m_pressedKeys.clear();
    }

    void Window::frameEnd() {
        EventFrameEnd::post();

        // Clean up all tasks that are done
        TaskManager::collectGarbage();

        this->endNativeWindowFrame();

        // Render UI
        ImGui::Render();

        int displayWidth, displayHeight;
        glfwGetFramebufferSize(this->m_window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(0.00F, 0.00F, 0.00F, 0.00F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        GLFWwindow *backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);

        glfwSwapBuffers(this->m_window);

        // Process layout load requests
        // NOTE: This needs to be done before a new frame is started, otherwise ImGui won't handle docking correctly
        LayoutManager::process();
    }

    void Window::initGLFW() {
        bool restoreWindowPos = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.restore_window_pos", false);

        glfwSetErrorCallback([](int error, const char *desc) {
            if (error == GLFW_PLATFORM_ERROR) {
                // Ignore error spam caused by Wayland not supporting moving or resizing
                // windows or querying their position and size.
                if (std::string_view(desc).contains("Wayland"))
                    return;
            }

            try {
                log::error("GLFW Error [0x{:05X}] : {}", error, desc);
            } catch (const std::system_error &) {
                // Catch and ignore system error that might be thrown when too many errors are being logged to a file
            }
        });

        if (!glfwInit()) {
            log::fatal("Failed to initialize GLFW!");
            std::abort();
        }

        // Set up used OpenGL version
        #if defined(OS_MACOS)
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
            glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, GLFW_TRUE);
        #else
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        #endif

        glfwWindowHint(GLFW_DECORATED, ImHexApi::System::isBorderlessWindowModeEnabled() ? GL_FALSE : GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);

        if (restoreWindowPos) {
            int maximized = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.maximized", GLFW_FALSE);
            glfwWindowHint(GLFW_MAXIMIZED, maximized);
        }

        // Create window
        this->m_windowTitle = "ImHex";
        this->m_window      = glfwCreateWindow(1280_scaled, 720_scaled, this->m_windowTitle.c_str(), nullptr, nullptr);

        glfwSetWindowUserPointer(this->m_window, this);

        if (this->m_window == nullptr) {
            log::fatal("Failed to create window!");
            std::abort();
        }

        // Force window to be fully opaque by default
        glfwSetWindowOpacity(this->m_window, 1.0F);

        glfwMakeContextCurrent(this->m_window);
        glfwSwapInterval(1);

        // Center window
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        if (monitor != nullptr) {
            const GLFWvidmode *mode = glfwGetVideoMode(monitor);
            if (mode != nullptr) {
                int monitorX, monitorY;
                glfwGetMonitorPos(monitor, &monitorX, &monitorY);

                int windowWidth, windowHeight;
                glfwGetWindowSize(this->m_window, &windowWidth, &windowHeight);

                glfwSetWindowPos(this->m_window, monitorX + (mode->width - windowWidth) / 2, monitorY + (mode->height - windowHeight) / 2);
            }
        }

        // Set up initial window position
        {
            int x = 0, y = 0;
            glfwGetWindowPos(this->m_window, &x, &y);

            if (restoreWindowPos) {
                x = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.x", x);
                y = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.y", y);
            }

            ImHexApi::System::impl::setMainWindowPosition(x, y);
            glfwSetWindowPos(this->m_window, x, y);
        }

        // Set up initial window size
        {
            int width = 0, height = 0;
            glfwGetWindowSize(this->m_window, &width, &height);
            glfwSetWindowSize(this->m_window, width, height);

            if (restoreWindowPos) {
                width = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.width", width);
                height = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.height", height);
            }

            ImHexApi::System::impl::setMainWindowSize(width, height);
            glfwSetWindowSize(this->m_window, width, height);
        }

        // Register window move callback
        glfwSetWindowPosCallback(this->m_window, [](GLFWwindow *window, int x, int y) {
            ImHexApi::System::impl::setMainWindowPosition(x, y);

            if (auto g = ImGui::GetCurrentContext(); g == nullptr || g->WithinFrameScope) return;

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->frameBegin();
            win->frame();
            win->frameEnd();
            win->processEvent();
        });

        // Register window resize callback
        glfwSetWindowSizeCallback(this->m_window, [](GLFWwindow *window, int width, int height) {
            if (!glfwGetWindowAttrib(window, GLFW_ICONIFIED))
                ImHexApi::System::impl::setMainWindowSize(width, height);

            if (auto g = ImGui::GetCurrentContext(); g == nullptr || g->WithinFrameScope) return;

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->frameBegin();
            win->frame();
            win->frameEnd();
            win->processEvent();
        });

        // Register mouse handling callback
        glfwSetMouseButtonCallback(this->m_window, [](GLFWwindow *window, int button, int action, int mods) {
            hex::unused(button, mods);

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));

            if (action == GLFW_PRESS)
                win->m_buttonDown = true;
            else if (action == GLFW_RELEASE)
                win->m_buttonDown = false;
            win->processEvent();
        });

        // Register scrolling callback
        glfwSetScrollCallback(this->m_window, [](GLFWwindow *window, double xOffset, double yOffset) {
            hex::unused(xOffset, yOffset);

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->processEvent();
        });

        #if !defined(OS_WEB)
            // Register key press callback
            glfwSetKeyCallback(this->m_window, [](GLFWwindow *window, int key, int scanCode, int action, int mods) {
                hex::unused(mods);

                auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));

                if (action == GLFW_RELEASE) {
                    win->m_buttonDown = false;
                } else {
                    win->m_buttonDown = true;
                }

                // Handle A-Z keys using their ASCII value instead of the keycode
                if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
                    std::string_view name = glfwGetKeyName(key, scanCode);

                    // If the key name is only one character long, use the ASCII value instead
                    // Otherwise the keyboard was set to a non-English layout and the key name
                    // is not the same as the ASCII value
                    if (name.length() == 1) {
                        key = std::toupper(name[0]);
                    }
                }

                if (key == GLFW_KEY_UNKNOWN) return;

                if (action == GLFW_PRESS || action == GLFW_REPEAT) {
                    if (key != GLFW_KEY_LEFT_CONTROL && key != GLFW_KEY_RIGHT_CONTROL &&
                        key != GLFW_KEY_LEFT_ALT && key != GLFW_KEY_RIGHT_ALT &&
                        key != GLFW_KEY_LEFT_SHIFT && key != GLFW_KEY_RIGHT_SHIFT &&
                        key != GLFW_KEY_LEFT_SUPER && key != GLFW_KEY_RIGHT_SUPER
                    ) {
                        win->m_pressedKeys.push_back(key);
                    }
                }

                win->processEvent();
            });
        #endif

        // Register cursor position callback
        glfwSetCursorPosCallback(this->m_window, [](GLFWwindow *window, double x, double y) {
            hex::unused(x, y);

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->processEvent();
        });

        // Register window close callback
        glfwSetWindowCloseCallback(this->m_window, [](GLFWwindow *window) {
            EventWindowClosing::post(window);
        });

        // Register file drop callback
        glfwSetDropCallback(this->m_window, [](GLFWwindow *, int count, const char **paths) {
            // Loop over all dropped files
            for (int i = 0; i < count; i++) {
                auto path = std::fs::path(reinterpret_cast<const char8_t *>(paths[i]));

                // Check if a custom file handler can handle the file
                bool handled = false;
                for (const auto &[extensions, handler] : ContentRegistry::FileHandler::impl::getEntries()) {
                    for (const auto &extension : extensions) {
                        if (path.extension() == extension) {
                            // Pass the file to the handler and check if it was successful
                            if (!handler(path)) {
                                log::error("Handler for extensions '{}' failed to process file!", extension);
                                break;
                            }

                            handled = true;
                        }
                    }
                }

                // If no custom handler was found, just open the file regularly
                if (!handled)
                    RequestOpenFile::post(path);
            }
        });

        glfwSetWindowSizeLimits(this->m_window, 480_scaled, 360_scaled, GLFW_DONT_CARE, GLFW_DONT_CARE);

        glfwShowWindow(this->m_window);
    }

    void Window::resize(i32 width, i32 height) {
        glfwSetWindowSize(this->m_window, width, height);
    }

    void Window::initImGui() {
        IMGUI_CHECKVERSION();

        auto fonts = ImHexApi::Fonts::getFontAtlas();

        // Initialize ImGui and all other ImGui extensions
        GImGui   = ImGui::CreateContext(fonts);
        GImPlot  = ImPlot::CreateContext();
        GImNodes = ImNodes::CreateContext();

        ImGuiIO &io       = ImGui::GetIO();
        ImGuiStyle &style = ImGui::GetStyle();

        ImNodes::GetStyle().Flags = ImNodesStyleFlags_NodeOutline | ImNodesStyleFlags_GridLines;

        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.FontGlobalScale = 1.0F;

        if (glfwGetPrimaryMonitor() != nullptr) {
            bool multiWindowEnabled = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.multi_windows", false);

            if (multiWindowEnabled)
                io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        }

        io.ConfigViewportsNoTaskBarIcon = false;

        ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
        ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkCreationOnSnap);

        // Allow ImNodes links to always be detached without holding down any button
        {
            static bool always = true;
            ImNodes::GetIO().LinkDetachWithModifierClick.Modifier = &always;
        }

        io.UserData = &this->m_imguiCustomData;

        auto scale = ImHexApi::System::getGlobalScale();
        style.ScaleAllSizes(scale);
        io.DisplayFramebufferScale = ImVec2(scale, scale);
        io.Fonts->SetTexID(fonts->TexID);

        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.IndentSpacing            = 10.0F;
        style.DisplaySafeAreaPadding  = ImVec2(0.0F, 0.0F);

        // Install custom settings handler
        {
            ImGuiSettingsHandler handler;
            handler.TypeName   = "ImHex";
            handler.TypeHash   = ImHashStr("ImHex");

            handler.ReadOpenFn = [](ImGuiContext *ctx, ImGuiSettingsHandler *, const char *) -> void* { return ctx; };

            handler.ReadLineFn = [](ImGuiContext *, ImGuiSettingsHandler *handler, void *, const char *line) {
                Window* window = static_cast<Window*>(handler->UserData);

                for (auto &[name, view] : ContentRegistry::Views::impl::getEntries()) {
                    std::string format = view->getUnlocalizedName() + "=%d";
                    sscanf(line, format.c_str(), &view->getWindowOpenState());
                }
                for (auto &[name, function, detached] : ContentRegistry::Tools::impl::getEntries()) {
                    std::string format = name + "=%d";
                    sscanf(line, format.c_str(), &detached);
                }

                int width = 0, height = 0;
                sscanf(line, "MainWindowSize=%d,%d", &width, &height);

                if (width > 0 && height > 0) {
                    TaskManager::doLater([width, height, window]{
                        glfwSetWindowSize(window->m_window, width, height);
                    });
                }
            };

            handler.WriteAllFn = [](ImGuiContext *, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf) {
                buf->appendf("[%s][General]\n", handler->TypeName);

                for (auto &[name, view] : ContentRegistry::Views::impl::getEntries()) {
                    buf->appendf("%s=%d\n", name.c_str(), view->getWindowOpenState());
                }
                for (auto &[name, function, detached] : ContentRegistry::Tools::impl::getEntries()) {
                    buf->appendf("%s=%d\n", name.c_str(), detached);
                }

                buf->append("\n");
            };

            handler.UserData   = this;
            ImGui::GetCurrentContext()->SettingsHandlers.push_back(handler);

            io.IniFilename = nullptr;
            for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Config)) {
                if (std::fs::exists(dir) && (fs::isPathWritable(dir))) {
                    s_imguiSettingsPath = dir / "interface.ini";
                    break;
                }
            }

            if (!s_imguiSettingsPath.empty() && wolv::io::fs::exists(s_imguiSettingsPath)) {
                ImGui::LoadIniSettingsFromDisk(wolv::util::toUTF8String(s_imguiSettingsPath).c_str());
            }
        }


        ImGui_ImplGlfw_InitForOpenGL(this->m_window, true);

        #if defined(OS_MACOS)
            ImGui_ImplOpenGL3_Init("#version 150");
        #elif defined(OS_WEB)
            ImGui_ImplOpenGL3_Init();
        #else
            ImGui_ImplOpenGL3_Init("#version 130");
        #endif

        for (const auto &plugin : PluginManager::getPlugins())
            plugin.setImGuiContext(ImGui::GetCurrentContext());

        RequestInitThemeHandlers::post();
    }

    void Window::exitGLFW() {
        {
            int x = 0, y = 0, width = 0, height = 0, maximized = 0;
            glfwGetWindowPos(this->m_window, &x, &y);
            glfwGetWindowSize(this->m_window, &width, &height);
            maximized = glfwGetWindowAttrib(this->m_window, GLFW_MAXIMIZED);

            ContentRegistry::Settings::write("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.x", x);
            ContentRegistry::Settings::write("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.y", y);
            ContentRegistry::Settings::write("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.width", width);
            ContentRegistry::Settings::write("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.height", height);
            ContentRegistry::Settings::write("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.maximized", maximized);
        }

        glfwDestroyWindow(this->m_window);
        glfwTerminate();

        this->m_window = nullptr;
    }

    void Window::exitImGui() {
        ImGui::SaveIniSettingsToDisk(wolv::util::toUTF8String(s_imguiSettingsPath).c_str());

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

}
