#include "window.hpp"

#include <hex.hpp>

#include <hex/api/plugin_manager.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/layout_manager.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/workspace_manager.hpp>
#include <hex/api/tutorial_manager.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>

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

#include <GLFW/glfw3.h>
#include <hex/ui/toast.hpp>
#include <wolv/utils/guards.hpp>

namespace hex {

    using namespace std::literals::chrono_literals;

    Window::Window() {
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
                } else if (argument == "duplicate-plugins") {
                    openEmergencyPopup("Duplicate Plugins loaded");
                }
            }
        }

        // Initialize the window
        this->initGLFW();
        this->initImGui();
        this->setupNativeWindow();
        this->registerEventHandlers();

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

        EventWindowDeinitializing::post(m_window);

        this->exitImGui();
        this->exitGLFW();
    }

    void Window::registerEventHandlers() {
        // Initialize default theme
        RequestChangeTheme::post("Dark");

        // Handle the close window request by telling GLFW to shut down
        RequestCloseImHex::subscribe(this, [this](bool noQuestions) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);

            if (!noQuestions)
                EventWindowClosing::post(m_window);
        });

        // Handle opening popups
        RequestOpenPopup::subscribe(this, [this](auto name) {
            std::scoped_lock lock(m_popupMutex);

            m_popupsToOpen.push_back(name);
        });
    }

    void Window::fullFrame() {
        m_lastStartFrameTime = glfwGetTime();

        glfwPollEvents();

        static ImVec2 lastWindowSize = ImHexApi::System::getMainWindowSize();
        if (ImHexApi::System::impl::isWindowResizable()) {
            glfwSetWindowSizeLimits(m_window, 480_scaled, 360_scaled, GLFW_DONT_CARE, GLFW_DONT_CARE);
            lastWindowSize = ImHexApi::System::getMainWindowSize();
        } else {
            glfwSetWindowSizeLimits(m_window, lastWindowSize.x, lastWindowSize.y, lastWindowSize.x, lastWindowSize.y);
        }

        // Render frame
        this->frameBegin();
        this->frame();
        this->frameEnd();
    }

    void Window::loop() {
        while (!glfwWindowShouldClose(m_window)) {
            m_lastStartFrameTime = glfwGetTime();

            // Determine if the application should be in long sleep mode
            bool shouldLongSleep = !m_unlockFrameRate;

            // Wait 5 frames before actually enabling the long sleep mode to make animations not stutter
            constexpr static auto LongSleepTimeout = 5;
            static i32 lockTimeout = 0;
            if (!shouldLongSleep) {
                lockTimeout = LongSleepTimeout;
            } else if (lockTimeout > 0) {
                lockTimeout -= 1;
            }

            if (shouldLongSleep && lockTimeout > 0)
                shouldLongSleep = false;

            m_unlockFrameRate = false;

            if (!glfwGetWindowAttrib(m_window, GLFW_VISIBLE) || glfwGetWindowAttrib(m_window, GLFW_ICONIFIED)) {
                // If the application is minimized or not visible, don't render anything
                glfwWaitEvents();
            } else {
                // If the application is visible, render a frame

                // If the application is in long sleep mode, only render a frame every 200ms
                // Long sleep mode is enabled automatically after a few frames if the window content hasn't changed
                // and no events have been received
                if (shouldLongSleep) {
                    // Calculate the time until the next frame
                    constexpr static auto LongSleepFPS = 5.0;
                    const double timeout = std::max(0.0, (1.0 / LongSleepFPS) - (glfwGetTime() - m_lastStartFrameTime));

                    glfwWaitEventsTimeout(timeout);
                }
            }

            this->fullFrame();

            ImHexApi::System::impl::setLastFrameTime(glfwGetTime() - m_lastStartFrameTime);

            // Limit frame rate
            // If the target FPS are below 15, use the monitor refresh rate, if it's above 200, don't limit the frame rate
            const auto targetFPS = ImHexApi::System::getTargetFPS();
            if (targetFPS < 15) {
                glfwSwapInterval(1);
            } else if (targetFPS > 200) {
                glfwSwapInterval(0);
            } else {
                if (!shouldLongSleep) {
                    glfwSwapInterval(0);
                    const auto frameTime = glfwGetTime() - m_lastStartFrameTime;
                    const auto targetFrameTime = 1.0 / targetFPS;
                    if (frameTime < targetFrameTime) {
                        glfwWaitEventsTimeout(targetFrameTime - frameTime);
                    }
                }
            }

            m_lastFrameTime = glfwGetTime() - m_lastStartFrameTime;
        }

        // Hide the window as soon as the render loop exits to make the window
        // disappear as soon as it's closed
        glfwHideWindow(m_window);
    }

    void Window::frameBegin() {
        // Start new ImGui Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        EventFrameBegin::post();

        // Handle all undocked floating windows
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(ImHexApi::System::getMainWindowSize() - ImVec2(0, ImGui::GetTextLineHeightWithSpacing()));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        // Render main dock space
        if (ImGui::Begin("ImHexDockSpace", nullptr, windowFlags)) {
            ImGui::PopStyleVar();

            this->beginNativeWindowFrame();
        } else {
            ImGui::PopStyleVar();
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
                        ImGui::TextUnformatted(wolv::io::fs::exists(filePath) ? "Yes" : "No");
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

            // Duplicate plugins error popup
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
            if (ImGui::BeginPopupModal("Duplicate Plugins loaded", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
                ImGui::TextUnformatted("ImHex found and attempted to load multiple plugins with the same name!");
                ImGui::TextUnformatted("Make sure you installed ImHex correctly and, if needed,");
                ImGui::TextUnformatted("cleaned up older installations correctly.");
                ImGui::TextUnformatted("Each plugin should only ever be loaded once.");

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
            std::scoped_lock lock(m_popupMutex);
            m_popupsToOpen.remove_if([](const auto &name) {
                if (ImGui::IsPopupOpen(name.c_str()))
                    return true;
                else
                    ImGui::OpenPopup(name.c_str());

                return false;
            });
        }

        // Draw popup stack
        {
            static bool positionSet = false;
            static bool sizeSet = false;
            static double popupDelay = -2.0;
            static u32 displayFrameCount = 0;

            static std::unique_ptr<impl::PopupBase> currPopup;
            static Lang name("");

            AT_FIRST_TIME {
                EventImHexClosing::subscribe([] {
                    currPopup.reset();
                });
            };

            if (auto &popups = impl::PopupBase::getOpenPopups(); !popups.empty()) {
                if (!ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopupId)) {
                    if (popupDelay <= -1.0) {
                        popupDelay = 0.2;
                    } else {
                        popupDelay -= m_lastFrameTime;
                        if (popupDelay < 0 || popups.size() == 1) {
                            popupDelay = -2.0;
                            currPopup = std::move(popups.back());
                            name = Lang(currPopup->getUnlocalizedName());
                            displayFrameCount = 0;

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
                        displayFrameCount += 1;
                        currPopup->drawContent();

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
                    }
                };

                if (currPopup->isModal())
                    createPopup(ImGui::BeginPopupModal(name, closeButton, flags));
                else
                    createPopup(ImGui::BeginPopup(name, flags));

                if (!ImGui::IsPopupOpen(name) && displayFrameCount < 100) {
                    ImGui::OpenPopup(name);
                }

                if (currPopup->shouldClose()) {
                    log::debug("Closing popup '{}'", name);
                    positionSet = sizeSet = false;

                    currPopup = nullptr;
                }
            }
        }

        // Draw Toasts
        {
            u32 index = 0;
            for (const auto &toast : impl::ToastBase::getQueuedToasts() | std::views::take(4)) {
                const auto toastHeight = 60_scaled;
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5_scaled);
                ImGui::SetNextWindowSize(ImVec2(280_scaled, toastHeight));
                ImGui::SetNextWindowPos((ImHexApi::System::getMainWindowPosition() + ImHexApi::System::getMainWindowSize()) - scaled({ 10, 10 }) - scaled({ 0, (10 + toastHeight) * index }), ImGuiCond_Always, ImVec2(1, 1));
                if (ImGui::Begin(hex::format("##Toast_{}", index).c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing)) {
                    auto drawList = ImGui::GetWindowDrawList();

                    const auto min = ImGui::GetWindowPos();
                    const auto max = min + ImGui::GetWindowSize();

                    drawList->PushClipRect(min, min + scaled({ 5, 60 }));
                    drawList->AddRectFilled(min, max, toast->getColor(), 5_scaled);
                    drawList->PopClipRect();

                    ImGui::Indent();
                    toast->draw();
                    ImGui::Unindent();

                    if (ImGui::IsWindowHovered() || toast->getAppearTime() <= 0)
                        toast->setAppearTime(ImGui::GetTime());
                }
                ImGui::End();
                ImGui::PopStyleVar();

                index += 1;
            }

            std::erase_if(impl::ToastBase::getQueuedToasts(), [](const auto &toast){
                return toast->getAppearTime() > 0 && (toast->getAppearTime() + impl::ToastBase::VisibilityTime) < ImGui::GetTime();
            });
        }

        // Run all deferred calls
        TaskManager::runDeferredCalls();
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
                const auto &[unlocalizedName, openView] = entry;

                return openView->hasViewMenuItemEntry() && openView->shouldProcess();
            });

            ImGuiWindowClass windowClass = {};

            windowClass.DockNodeFlagsOverrideSet |= ImGuiDockNodeFlags_NoCloseButton;

            if (openViewCount <= 1 || LayoutManager::isLayoutLocked())
                windowClass.DockNodeFlagsOverrideSet |= ImGuiDockNodeFlags_NoTabBar;

            ImGui::SetNextWindowClass(&windowClass);

            // Draw view
            view->draw();
            view->trackViewOpenState();

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
                        EventViewOpened::post(view.get());
                    }

                    ImGui::End();
                }

                // Pass on currently pressed keys to the shortcut handler
                for (const auto &key : m_pressedKeys) {
                    ShortcutManager::process(view, io.KeyCtrl, io.KeyAlt, io.KeyShift, io.KeySuper, focused, key);
                }
            }
        }

        // Handle global shortcuts
        for (const auto &key : m_pressedKeys) {
            ShortcutManager::processGlobals(io.KeyCtrl, io.KeyAlt, io.KeyShift, io.KeySuper, key);
        }

        m_pressedKeys.clear();
    }

    void Window::frameEnd() {
        EventFrameEnd::post();

        TutorialManager::drawTutorial();

        // Clean up all tasks that are done
        TaskManager::collectGarbage();

        this->endNativeWindowFrame();

        // Finalize ImGui frame
        ImGui::Render();

        // Hash the draw data to determine if anything changed on the screen
        // If not, there's no point in sending the draw data off to the GPU and swapping buffers
        bool shouldRender = false;
        {
            u32 drawDataHash = 0;
            static u32 previousDrawDataHash = 0;

            for (const auto viewPort : ImGui::GetPlatformIO().Viewports) {
                auto drawData = viewPort->DrawData;
                for (int n = 0; n < drawData->CmdListsCount; n++) {
                    const ImDrawList *cmdList = drawData->CmdLists[n];
                    drawDataHash = ImHashData(cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert), drawDataHash);
                }
                for (int n = 0; n < drawData->CmdListsCount; n++) {
                    const ImDrawList *cmdList = drawData->CmdLists[n];
                    drawDataHash = ImHashData(cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx), drawDataHash);
                }
            }

            shouldRender = drawDataHash != previousDrawDataHash;
            previousDrawDataHash = drawDataHash;
        }

        if (shouldRender) {
            int displayWidth, displayHeight;
            glfwGetFramebufferSize(m_window, &displayWidth, &displayHeight);
            glViewport(0, 0, displayWidth, displayHeight);
            glClearColor(0.00F, 0.00F, 0.00F, 0.00F);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(m_window);

            m_unlockFrameRate = true;
        }

        GLFWwindow *backupContext = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backupContext);

        // Process layout load requests
        // NOTE: This needs to be done before a new frame is started, otherwise ImGui won't handle docking correctly
        LayoutManager::process();
        WorkspaceManager::process();
    }

    void Window::initGLFW() {
        auto initialWindowProperties = ImHexApi::System::getInitialWindowProperties();
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
            glfwWindowHint(GLFW_DECORATED, ImHexApi::System::isBorderlessWindowModeEnabled() ? GL_FALSE : GL_TRUE);
        #endif

        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);

        if (initialWindowProperties.has_value()) {
            glfwWindowHint(GLFW_MAXIMIZED, initialWindowProperties->maximized);
        }

        // Create window
        m_windowTitle = "ImHex";
        m_window      = glfwCreateWindow(1280_scaled, 720_scaled, m_windowTitle.c_str(), nullptr, nullptr);

        ImHexApi::System::impl::setMainWindowHandle(m_window);

        glfwSetWindowUserPointer(m_window, this);

        if (m_window == nullptr) {
            log::fatal("Failed to create window!");
            std::abort();
        }

        // Force window to be fully opaque by default
        glfwSetWindowOpacity(m_window, 1.0F);

        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1);

        // Center window
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        if (monitor != nullptr) {
            const GLFWvidmode *mode = glfwGetVideoMode(monitor);
            if (mode != nullptr) {
                int monitorX, monitorY;
                glfwGetMonitorPos(monitor, &monitorX, &monitorY);

                int windowWidth, windowHeight;
                glfwGetWindowSize(m_window, &windowWidth, &windowHeight);

                glfwSetWindowPos(m_window, monitorX + (mode->width - windowWidth) / 2, monitorY + (mode->height - windowHeight) / 2);
            }
        }

        // Set up initial window position
        {
            int x = 0, y = 0;
            glfwGetWindowPos(m_window, &x, &y);

            if (initialWindowProperties.has_value()) {
                x = initialWindowProperties->x;
                y = initialWindowProperties->y;
            }

            ImHexApi::System::impl::setMainWindowPosition(x, y);
            glfwSetWindowPos(m_window, x, y);
        }

        // Set up initial window size
        {
            int width = 0, height = 0;
            glfwGetWindowSize(m_window, &width, &height);
            glfwSetWindowSize(m_window, width, height);

            if (initialWindowProperties.has_value()) {
                width  = initialWindowProperties->width;
                height = initialWindowProperties->height;
            }

            ImHexApi::System::impl::setMainWindowSize(width, height);
            glfwSetWindowSize(m_window, width, height);
        }

        // Register window move callback
        glfwSetWindowPosCallback(m_window, [](GLFWwindow *window, int x, int y) {
            ImHexApi::System::impl::setMainWindowPosition(x, y);

            if (auto g = ImGui::GetCurrentContext(); g == nullptr || g->WithinFrameScope) return;

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->m_unlockFrameRate = true;

            win->frameBegin();
            win->frame();
            win->frameEnd();
        });

        // Register window resize callback
        glfwSetWindowSizeCallback(m_window, [](GLFWwindow *window, int width, int height) {
            if (!glfwGetWindowAttrib(window, GLFW_ICONIFIED))
                ImHexApi::System::impl::setMainWindowSize(width, height);

            if (auto g = ImGui::GetCurrentContext(); g == nullptr || g->WithinFrameScope) return;

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->m_unlockFrameRate = true;

            win->frameBegin();
            win->frame();
            win->frameEnd();
        });

        glfwSetCursorPosCallback(m_window, [](GLFWwindow *window, double, double) {
            if (auto g = ImGui::GetCurrentContext(); g == nullptr || g->WithinFrameScope) return;

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->m_unlockFrameRate = true;
        });

        glfwSetWindowFocusCallback(m_window, [](GLFWwindow *, int focused) {
            EventWindowFocused::post(focused == GLFW_TRUE);
        });

        #if !defined(OS_WEB)
            // Register key press callback
            glfwSetInputMode(m_window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
            glfwSetKeyCallback(m_window, [](GLFWwindow *window, int key, int scanCode, int action, int mods) {
                hex::unused(mods);


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
                        auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
                        win->m_unlockFrameRate = true;

                        if (!(mods & GLFW_MOD_NUM_LOCK)) {
                            if (key == GLFW_KEY_KP_0) key = GLFW_KEY_INSERT;
                            else if (key == GLFW_KEY_KP_1) key = GLFW_KEY_END;
                            else if (key == GLFW_KEY_KP_2) key = GLFW_KEY_DOWN;
                            else if (key == GLFW_KEY_KP_3) key = GLFW_KEY_PAGE_DOWN;
                            else if (key == GLFW_KEY_KP_4) key = GLFW_KEY_LEFT;
                            else if (key == GLFW_KEY_KP_6) key = GLFW_KEY_RIGHT;
                            else if (key == GLFW_KEY_KP_7) key = GLFW_KEY_HOME;
                            else if (key == GLFW_KEY_KP_8) key = GLFW_KEY_UP;
                            else if (key == GLFW_KEY_KP_9) key = GLFW_KEY_PAGE_UP;
                        }

                        win->m_pressedKeys.push_back(key);
                    }
                }
            });
        #endif

        // Register window close callback
        glfwSetWindowCloseCallback(m_window, [](GLFWwindow *window) {
            EventWindowClosing::post(window);
        });

        glfwSetWindowSizeLimits(m_window, 480_scaled, 360_scaled, GLFW_DONT_CARE, GLFW_DONT_CARE);

        glfwShowWindow(m_window);
    }

    void Window::resize(i32 width, i32 height) {
        glfwSetWindowSize(m_window, width, height);
    }

    void Window::initImGui() {
        IMGUI_CHECKVERSION();

        auto fonts = ImHexApi::Fonts::getFontAtlas();

        if (fonts == nullptr) {
            fonts = IM_NEW(ImFontAtlas)();

            fonts->AddFontDefault();
            fonts->Build();
        }

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
            if (ImHexApi::System::isMutliWindowModeEnabled())
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

        io.UserData = &m_imguiCustomData;

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
                auto window = static_cast<Window*>(handler->UserData);

                for (auto &[name, view] : ContentRegistry::Views::impl::getEntries()) {
                    std::string format = view->getUnlocalizedName().get() + "=%d";
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

            auto context = ImGui::GetCurrentContext();
            context->SettingsHandlers.push_back(handler);
            context->TestEngineHookItems = true;

            io.IniFilename = nullptr;
        }


        ImGui_ImplGlfw_InitForOpenGL(m_window, true);

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
        glfwDestroyWindow(m_window);
        glfwTerminate();

        m_window = nullptr;
    }

    void Window::exitImGui() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

}
