#include "window.hpp"
#include "hex/ui/banner.hpp"

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
#include <hex/helpers/default_paths.hpp>

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
#include <implot3d.h>
#include <implot3d_internal.h>
#include <imnodes.h>
#include <imnodes_internal.h>

#include <wolv/utils/string.hpp>

#include <GLFW/glfw3.h>
#include <hex/ui/toast.hpp>
#include <wolv/utils/guards.hpp>
#include <fmt/printf.h>
#include <fmt/chrono.h>

namespace hex {

    using namespace std::literals::chrono_literals;

    Window::Window() {
        const static auto openEmergencyPopup = [this](const std::string &title){
            TaskManager::doLater([this, title] {
                for (const auto &provider : ImHexApi::Provider::getProviders())
                    ImHexApi::Provider::remove(provider, false);

                ImGui::OpenPopup(title.c_str());
                m_emergencyPopupOpen = true;
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
        ContentRegistry::Settings::impl::load();

        EventWindowInitialized::post();
        EventImHexStartupFinished::post();
        RequestStartMigration::post();

        TutorialManager::init();

        #if defined(OS_MACOS)
            ShortcutManager::enableMacOSMode();
        #endif
    }

    Window::~Window() {
        EventProviderDeleted::unsubscribe(this);
        RequestCloseImHex::unsubscribe(this);
        RequestUpdateWindowTitle::unsubscribe(this);
        EventAbnormalTermination::unsubscribe(this);
        RequestOpenPopup::unsubscribe(this);

        EventWindowDeinitializing::post(m_window);

        ContentRegistry::Settings::impl::store();

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

        EventDPIChanged::subscribe([this](float oldScaling, float newScaling) {
            if (oldScaling == newScaling || oldScaling == 0 || newScaling == 0)
                return;

            int width, height;
            glfwGetWindowSize(m_window, &width, &height);

            width = float(width) * newScaling / oldScaling;
            height = float(height) * newScaling / oldScaling;

            ImHexApi::System::impl::setMainWindowSize(width, height);
            glfwSetWindowSize(m_window, width, height);
        });


        LayoutManager::registerLoadCallback([this](std::string_view line) {
            int width = 0, height = 0;
                sscanf(line.data(), "MainWindowSize=%d,%d", &width, &height);

                if (width > 0 && height > 0) {
                    TaskManager::doLater([width, height, this]{
                        glfwSetWindowSize(m_window, width, height);
                    });
                }
        });
    }

    void handleException() {
        try {
            throw;
        } catch (const std::exception &e) {
            log::fatal("Unhandled exception: {}", e.what());
            EventCrashRecovered::post(e);
        } catch (...) {
            log::fatal("Unhandled exception: Unknown exception");
        }
    }

    void errorRecoverLogCallback(void*, const char* fmt, ...) {
        va_list args;

        std::string message;

        va_start(args, fmt);
        message.resize(std::vsnprintf(nullptr, 0, fmt, args));
        va_end(args);

        va_start(args, fmt);
        std::vsnprintf(message.data(), message.size(), fmt, args);
        va_end(args);

        message.resize(message.size() - 1);

        log::error("{}", message);
    }

    void Window::fullFrame() {
        [[maybe_unused]] static u32 crashWatchdog = 0;

        if (auto g = ImGui::GetCurrentContext(); g == nullptr || g->WithinFrameScope) {
            return;
        }

        #if !defined(DEBUG)
        try {
        #endif

            // Render an entire frame
            this->frameBegin();
            this->frame();
            this->frameEnd();

        #if !defined(DEBUG)
            // Feed the watchdog
            crashWatchdog = 0;
        } catch (...) {
            // If an exception keeps being thrown, abort the application after 10 frames
            // This is done to avoid the application getting stuck in an infinite loop of exceptions
            crashWatchdog += 1;
            if (crashWatchdog > 10) {
                log::fatal("Crash watchdog triggered, aborting");
                std::abort();
            }

            // Try to recover from the exception by bringing ImGui back into a working state
            ImGui::EndFrame();
            ImGui::UpdatePlatformWindows();

            // Handle the exception
            handleException();
        }
        #endif
    }

    void Window::loop() {
        glfwShowWindow(m_window);
        while (!glfwWindowShouldClose(m_window)) {
            m_lastStartFrameTime = glfwGetTime();

            {
                int x = 0, y = 0;
                int width = 0, height = 0;
                glfwGetWindowPos(m_window, &x, &y);
                glfwGetWindowSize(m_window, &width, &height);

                ImHexApi::System::impl::setMainWindowPosition(x, y);
                ImHexApi::System::impl::setMainWindowSize(width, height);
            }

            if (!glfwGetWindowAttrib(m_window, GLFW_VISIBLE) || glfwGetWindowAttrib(m_window, GLFW_ICONIFIED)) {
                // If the application is minimized or not visible, don't render anything
                glfwWaitEvents();
            }

            m_lastStartFrameTime = glfwGetTime();

            static ImVec2 lastWindowSize = ImHexApi::System::getMainWindowSize();
            if (ImHexApi::System::impl::isWindowResizable()) {
                glfwSetWindowSizeLimits(m_window, 480_scaled, 360_scaled, GLFW_DONT_CARE, GLFW_DONT_CARE);
                lastWindowSize = ImHexApi::System::getMainWindowSize();
            } else {
                glfwSetWindowSizeLimits(m_window, lastWindowSize.x, lastWindowSize.y, lastWindowSize.x, lastWindowSize.y);
            }

            this->fullFrame();

            ImHexApi::System::impl::setLastFrameTime(glfwGetTime() - m_lastStartFrameTime);

            {
                while (true) {
                    glfwPollEvents();

                    if (ImHexApi::System::getTargetFPS() >= 200)
                        break;

                    {
                        std::unique_lock lock(m_sleepMutex);
                        m_sleepCondVar.wait_for(lock, std::chrono::microseconds(100));
                        if (m_sleepFlag.exchange(false))
                            break;
                    }
                }
            }

            m_lastFrameTime = glfwGetTime() - m_lastStartFrameTime;

            // Unlock frame rate if any mouse button is being held down to allow drag scrolling to be smooth
            if (ImGui::IsAnyMouseDown())
                m_unlockFrameRate = true;

            // Unlock frame rate if any modifier key is held down since they don't generate key repeat events
            if (
                ImGui::IsKeyPressed(ImGuiKey_LeftCtrl) || ImGui::IsKeyPressed(ImGuiKey_RightCtrl) ||
                ImGui::IsKeyPressed(ImGuiKey_LeftShift) || ImGui::IsKeyPressed(ImGuiKey_RightShift) ||
                ImGui::IsKeyPressed(ImGuiKey_LeftSuper) || ImGui::IsKeyPressed(ImGuiKey_RightSuper) ||
                ImGui::IsKeyPressed(ImGuiKey_LeftAlt) || ImGui::IsKeyPressed(ImGuiKey_RightAlt)
            ) {
                m_unlockFrameRate = true;
            }

            // Unlock frame rate if there's more than one viewport since these don't call the glfw callbacks registered here
            if (ImGui::GetPlatformIO().Viewports.size() > 1)
                m_unlockFrameRate = true;
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

        TutorialManager::drawTutorial();

        EventFrameBegin::post();

        // Handle all undocked floating windows
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(ImHexApi::System::getMainWindowSize() - ImVec2(0, ImGui::GetTextLineHeightWithSpacing()));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        if (!m_emergencyPopupOpen)
            windowFlags |= ImGuiWindowFlags_MenuBar;

        // Render main dock space
        if (ImGui::Begin("ImHexDockSpace", nullptr, windowFlags)) {
            ImGui::PopStyleVar();

            this->beginNativeWindowFrame();
        } else {
            ImGui::PopStyleVar();
        }
        ImGui::End();
        ImGui::PopStyleVar(2);

        // Plugin load error popups
        // These are not translated because they should always be readable, no matter if any localization could be loaded or not
        {
            const static auto drawPluginFolderTable = [] {
                ImGuiExt::UnderlinedText("Plugin folders");
                if (ImGui::BeginTable("plugins", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit, ImVec2(0, 100_scaled))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 0.2);
                    ImGui::TableSetupColumn("Exists", ImGuiTableColumnFlags_WidthFixed, ImGui::GetTextLineHeight() * 3);

                    ImGui::TableHeadersRow();

                    for (const auto &path : paths::Plugins.all()) {
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

            if (m_emergencyPopupOpen) {
                const auto pos = ImHexApi::System::getMainWindowPosition();
                const auto size = ImHexApi::System::getMainWindowSize();
                ImGui::GetBackgroundDrawList()->AddRectFilled(pos, pos + size, ImGui::GetColorU32(ImGuiCol_WindowBg) | 0xFF000000);
            }

            ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, 0x00);
            ON_SCOPE_EXIT { ImGui::PopStyleColor(); };

            // No plugins error popup
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
            if (ImGui::BeginPopupModal("No Plugins", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar)) {
                ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindowRead());
                ImGui::TextUnformatted("No ImHex plugins loaded (including the built-in plugin)!");
                ImGui::TextUnformatted("Make sure you installed ImHex correctly.");
                ImGui::TextUnformatted("There should be at least a 'builtin.hexplug' file in your plugins folder.");

                ImGui::NewLine();

                drawPluginFolderTable();

                ImGui::NewLine();
                if (ImGuiExt::DimmedButton("Close ImHex", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                    ImHexApi::System::closeImHex(true);

                ImGui::EndPopup();
            }

            // Duplicate plugins error popup
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
            if (ImGui::BeginPopupModal("Duplicate Plugins loaded", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar)) {
                ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindowRead());
                ImGui::TextUnformatted("ImHex found and attempted to load multiple plugins with the same name!");
                ImGui::TextUnformatted("Make sure you installed ImHex correctly and, if needed,");
                ImGui::TextUnformatted("cleaned up older installations correctly.");
                ImGui::TextUnformatted("Each plugin should only ever be loaded once.");

                ImGui::NewLine();

                drawPluginFolderTable();

                ImGui::NewLine();
                if (ImGuiExt::DimmedButton("Close ImHex", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                    ImHexApi::System::closeImHex(true);

                ImGui::EndPopup();
            }
        }

        // Open popups when plugins requested it
        // We retry every frame until the popup actually opens
        // It might not open the first time because another popup is already open
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

                std::string localizedName = name.get();
                if (currPopup->isModal())
                    createPopup(ImGui::BeginPopupModal(localizedName.c_str(), closeButton, flags));
                else
                    createPopup(ImGui::BeginPopup(localizedName.c_str(), flags));

                if (!ImGui::IsPopupOpen(localizedName.c_str()) && displayFrameCount < 5) {
                    ImGui::OpenPopup(localizedName.c_str());
                }

                if (currPopup->shouldClose() || !open) {
                    log::debug("Closing popup '{}'", localizedName);
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

        // Draw Banners
        {
            const bool onWelcomeScreen = !ImHexApi::Provider::isValid();

            const auto windowPos = ImHexApi::System::getMainWindowPosition();
            float startY = windowPos.y + ((ImGui::GetTextLineHeight() + (ImGui::GetStyle().FramePadding.y * 2.0F)) * (onWelcomeScreen ? 2 : 3));
            const auto height = 30_scaled;

            for (const auto &banner : impl::BannerBase::getOpenBanners() | std::views::take(5)) {
                ImGui::PushID(banner.get());
                {
                    ImGui::SetNextWindowPos(ImVec2(windowPos.x + 1_scaled, startY));
                    ImGui::SetNextWindowSize(ImVec2(ImHexApi::System::getMainWindowSize().x - 2_scaled, height));
                    ImGui::PushStyleColor(ImGuiCol_WindowBg, banner->getColor().Value);
                    if (ImGui::Begin("##Banner", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing)) {
                        if (ImGui::BeginChild("##Content", ImGui::GetContentRegionAvail() - ImVec2(20_scaled, 0))) {
                            banner->draw();
                        }
                        ImGui::EndChild();

                        ImGui::SameLine();

                        if (ImGui::CloseButton(ImGui::GetID("BannerCloseButton"), ImGui::GetCursorScreenPos())) {
                            banner->close();
                        }
                    }
                    ImGui::End();
                    ImGui::PopStyleColor();
                }
                ImGui::PopID();

                startY += height;
            }

            std::erase_if(impl::BannerBase::getOpenBanners(), [](const auto &banner) {
                return banner->shouldClose();
            });
        }

        // Run all deferred calls
        TaskManager::runDeferredCalls();
    }

    void Window::frame() {
        auto &io = ImGui::GetIO();

        ShortcutManager::resetLastActivatedMenu();

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

            auto window    = ImGui::FindWindowByName(view->getName().c_str());
            if (window != nullptr && window->DockNode == nullptr)
                ImGui::SetNextWindowBgAlpha(1.0F);

            // Draw view
            view->draw();
            view->trackViewOpenState();

            if (view->getWindowOpenState()) {
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

                    // Pass on currently pressed keys to the shortcut handler
                    for (const auto &key : m_pressedKeys) {
                        ShortcutManager::process(view.get(), io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl, io.KeyAlt, io.KeyShift, io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeySuper, focused, key);
                    }

                    ImGui::End();
                }
            }
        }

        // Handle global shortcuts
        for (const auto &key : m_pressedKeys) {
            ShortcutManager::processGlobals(io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl, io.KeyAlt, io.KeyShift, io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeySuper, key);
        }

        m_pressedKeys.clear();
    }

    void Window::frameEnd() {
        EventFrameEnd::post();

        // Clean up all tasks that are done
        TaskManager::collectGarbage();

        this->endNativeWindowFrame();

        // Finalize ImGui frame
        ImGui::Render();

        // Compare the previous frame buffer to the current one to determine if the window content has changed
        // If not, there's no point in sending the draw data off to the GPU and swapping buffers
        // NOTE: For anybody looking at this code and thinking "why not just hash the buffer and compare the hashes",
        // the reason is that hashing the buffer is significantly slower than just comparing the buffers directly.
        // The buffer might become quite large if there's a lot of vertices on the screen but it's still usually less than
        // 10MB (out of which only the active portion needs to actually be compared) which is worth the ~60x speedup.
        bool shouldRender = false;
        {
            static std::vector<u8> previousVtxData;
            static size_t previousVtxDataSize = 0;

            size_t offset = 0;
            size_t vtxDataSize = 0;

            for (const auto viewPort : ImGui::GetPlatformIO().Viewports) {
                auto drawData = viewPort->DrawData;
                for (int n = 0; n < drawData->CmdListsCount; n++) {
                    vtxDataSize += drawData->CmdLists[n]->VtxBuffer.size() * sizeof(ImDrawVert);
                }
            }
            for (const auto viewPort : ImGui::GetPlatformIO().Viewports) {
                auto drawData = viewPort->DrawData;
                for (int n = 0; n < drawData->CmdListsCount; n++) {
                    const ImDrawList *cmdList = drawData->CmdLists[n];
                    std::string ownerName = cmdList->_OwnerName;

                    if (vtxDataSize == previousVtxDataSize && (!ownerName.contains("##Popup") || !ownerName.contains("##image"))) {
                        shouldRender = shouldRender || std::memcmp(previousVtxData.data() + offset, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.size() * sizeof(ImDrawVert)) != 0;
                    } else {
                        shouldRender = true;
                    }

                    if (previousVtxData.size() < offset + cmdList->VtxBuffer.size() * sizeof(ImDrawVert)) {
                        previousVtxData.resize(offset + cmdList->VtxBuffer.size() * sizeof(ImDrawVert));
                    }

                    std::memcpy(previousVtxData.data() + offset, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.size() * sizeof(ImDrawVert));
                    offset += cmdList->VtxBuffer.size() * sizeof(ImDrawVert);
                }
            }

            previousVtxDataSize = vtxDataSize;
        }

        GLFWwindow *backupContext = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backupContext);

        if (shouldRender) {
            auto* drawData = ImGui::GetDrawData();
            
            // Avoid accidentally clearing the viewport when the application is minimized,
            // otherwise the OS will display an empty frame during deminimization on macOS
            if (drawData->DisplaySize.x != 0 && drawData->DisplaySize.y != 0) {
                int displayWidth, displayHeight;
                glfwGetFramebufferSize(m_window, &displayWidth, &displayHeight);
                glViewport(0, 0, displayWidth, displayHeight);
                glClearColor(0.00F, 0.00F, 0.00F, 0.00F);
                glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

                glfwSwapBuffers(m_window);
            }
        }

        // Process layout load requests
        // NOTE: This needs to be done before a new frame is started, otherwise ImGui won't handle docking correctly
        LayoutManager::process();
        WorkspaceManager::process();

        ImGui::GetIO().FontGlobalScale = 1.0F / ImHexApi::System::getBackingScaleFactor();
    }

    void Window::initGLFW() {
        auto initialWindowProperties = ImHexApi::System::getInitialWindowProperties();
        glfwSetErrorCallback([](int error, const char *desc) {
            bool isWaylandError = error == GLFW_PLATFORM_ERROR;
            #if defined(GLFW_FEATURE_UNAVAILABLE)
                isWaylandError = isWaylandError || (error == GLFW_FEATURE_UNAVAILABLE);
            #endif
            isWaylandError = isWaylandError && std::string_view(desc).contains("Wayland");

            if (isWaylandError) {
                // Ignore error spam caused by Wayland not supporting moving or resizing
                // windows or querying their position and size.
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

        configureGLFW();
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
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

        // Disable VSync. Not like any graphics driver actually cares
        glfwSwapInterval(0);

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

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->m_unlockFrameRate = true;
            win->fullFrame();
        });

        // Register window resize callback
        glfwSetWindowSizeCallback(m_window, [](GLFWwindow *window, [[maybe_unused]] int width, [[maybe_unused]] int height) {
            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->m_unlockFrameRate = true;

            #if !defined(OS_WINDOWS)
                if (!glfwGetWindowAttrib(window, GLFW_ICONIFIED))
                    ImHexApi::System::impl::setMainWindowSize(width, height);
            #endif

            #if defined(OS_MACOS)
                // Stop widgets registering hover effects while the window is being resized
                if (macosIsWindowBeingResizedByUser(window)) {
                    ImGui::GetIO().MousePos = ImVec2();
                }
            #elif defined(OS_WEB)
                win->fullFrame();
            #endif
        });

        static const auto unlockFrameRate = [](GLFWwindow *window, auto ...) {
            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->m_unlockFrameRate = true;
        };

        glfwSetCursorPosCallback(m_window, unlockFrameRate);
        glfwSetMouseButtonCallback(m_window, unlockFrameRate);
        glfwSetScrollCallback(m_window, unlockFrameRate);
        glfwSetWindowFocusCallback(m_window, unlockFrameRate);

        glfwSetWindowFocusCallback(m_window, [](GLFWwindow *, int focused) {
            EventWindowFocused::post(focused == GLFW_TRUE);
        });

        // Register key press callback
        glfwSetInputMode(m_window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
        glfwSetKeyCallback(m_window, [](GLFWwindow *window, int key, int scanCode, int action, int mods) {
            std::ignore = mods;

            #if !defined(OS_WEB)
                // Handle A-Z keys using their ASCII value instead of the keycode
                if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
                    std::string_view name = glfwGetKeyName(key, scanCode);

                    // If the key name is only one character long, use the ASCII value instead
                    // Otherwise the keyboard was set to a non-English layout and the key name
                    // is not the same as the ASCII value
                    if (!name.empty()) {
                        const std::uint8_t byte = name[0];
                        if (name.length() == 1 && byte <= 0x7F) {
                            key = std::toupper(byte);
                        }
                    }
                }
            #else
                std::ignore = scanCode;
                // Emscripten doesn't support glfwGetKeyName. Just pass the value through.
            #endif

            if (key == GLFW_KEY_UNKNOWN) return;

            if (action == GLFW_PRESS || action == GLFW_REPEAT) {
                if (key != GLFW_KEY_LEFT_CONTROL && key != GLFW_KEY_RIGHT_CONTROL &&
                    key != GLFW_KEY_LEFT_ALT && key != GLFW_KEY_RIGHT_ALT &&
                    key != GLFW_KEY_LEFT_SHIFT && key != GLFW_KEY_RIGHT_SHIFT &&
                    key != GLFW_KEY_LEFT_SUPER && key != GLFW_KEY_RIGHT_SUPER
                ) {
                    unlockFrameRate(window);

                    // Windows and Linux use the numpad for special actions when NumLock is disabled such as arrow keys or
                    // the insert, home and end keys. GLFW however still returns the original numpad keys that are being pressed.
                    // Translate them here to the desired keys.
                    // macOS doesn't seem to have the concept of NumLock at all. They repurposed it as the "Clear" key so this
                    // conversion makes no sense there.
                    #if !defined(OS_MACOS)
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
                    #endif

                    auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
                    win->m_pressedKeys.push_back(key);
                }
            }
        });

        // Register window close callback
        glfwSetWindowCloseCallback(m_window, [](GLFWwindow *window) {
            EventWindowClosing::post(window);
        });

        glfwSetWindowSizeLimits(m_window, 480_scaled, 360_scaled, GLFW_DONT_CARE, GLFW_DONT_CARE);

        m_frameRateThread = std::jthread([this](const std::stop_token &stopToken) {
            using Duration = std::chrono::duration<double, std::nano>;
            Duration passedTime = {};

            std::chrono::steady_clock::time_point startTime = {}, endTime = {};
            Duration requestedFrameTime = {}, remainingUnlockedTime = {};
            float targetFps = 0;

            const auto nativeFps = [] -> float {
                if (const auto monitor = glfwGetPrimaryMonitor(); monitor != nullptr) {
                    if (const auto videoMode = glfwGetVideoMode(monitor); videoMode != nullptr) {
                        return videoMode->refreshRate;
                    }
                }

                return 60;
            }();

            while (!stopToken.stop_requested()) {
                const auto iterationTime = endTime - startTime;
                startTime = std::chrono::steady_clock::now();

                targetFps = ImHexApi::System::getTargetFPS();

                if (m_unlockFrameRate.exchange(false)) {
                    remainingUnlockedTime = std::chrono::seconds(2);
                }

                // If the target frame rate is below 15, use the current monitor's refresh rate
                if (targetFps < 15) {
                    targetFps = nativeFps;
                }

                passedTime += iterationTime;
                if (remainingUnlockedTime > std::chrono::nanoseconds(0)) {
                    remainingUnlockedTime -= iterationTime;
                } else {
                    targetFps = 5;
                }

                requestedFrameTime = (Duration(1.0E9) / targetFps) / 1.3;
                if (passedTime >= requestedFrameTime) {
                    std::scoped_lock lock(m_sleepMutex);
                    m_sleepFlag = true;
                    m_sleepCondVar.notify_all();

                    passedTime = {};
                }

                std::this_thread::sleep_for(std::chrono::microseconds(100));

                endTime = std::chrono::steady_clock::now();
            }
        });
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
        GImGui              = ImGui::CreateContext(fonts);
        GImPlot             = ImPlot::CreateContext();
        ImPlot3D::GImPlot3D = ImPlot3D::CreateContext();
        GImNodes            = ImNodes::CreateContext();

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

        style.Colors[ImGuiCol_TabSelectedOverline]          = ImVec4(0.0F, 0.0F, 0.0F, 0.0F);
        style.Colors[ImGuiCol_TabDimmedSelectedOverline]    = ImVec4(0.0F, 0.0F, 0.0F, 0.0F);

        // Install custom settings handler
        {
            ImGuiSettingsHandler handler;
            handler.TypeName   = "ImHex";
            handler.TypeHash   = ImHashStr("ImHex");

            handler.ReadOpenFn = [](ImGuiContext *ctx, ImGuiSettingsHandler *, const char *) -> void* { return ctx; };

            handler.ReadLineFn = [](ImGuiContext *, ImGuiSettingsHandler *, void *, const char *line) {
                LayoutManager::onLoad(line);
            };

            handler.WriteAllFn = [](ImGuiContext *, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buffer) {
                buffer->appendf("[%s][General]\n", handler->TypeName);
                LayoutManager::onStore(buffer);
                buffer->append("\n");
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
            ImGui_ImplGlfw_InstallEmscriptenCallbacks(m_window, "#canvas");
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

        ImNodes::DestroyContext();
        ImPlot3D::DestroyContext();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

}
