#include "window.hpp"

#include <hex.hpp>

#include <hex/api/plugin_manager.hpp>
#include <hex/api/content_registry/views.hpp>
#include <hex/api/imhex_api/fonts.hpp>
#include <hex/api/layout_manager.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/workspace_manager.hpp>
#include <hex/api/tutorial_manager.hpp>
#include <hex/api/events/requests_lifecycle.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/api/events/events_gui.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/default_paths.hpp>

#include <hex/providers/provider.hpp>

#include <hex/ui/view.hpp>
#include <hex/ui/popup.hpp>
#include <hex/ui/banner.hpp>

#include <cmath>
#include <numbers>

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

#if defined(IMGUI_TEST_ENGINE)
    #include <imgui_te_engine.h>
    #include <imgui_te_ui.h>
#endif

#include <wolv/utils/string.hpp>

#include <GLFW/glfw3.h>
#include <hex/ui/toast.hpp>
#include <wolv/utils/guards.hpp>
#include <fmt/printf.h>
#include <hex/helpers/opengl.hpp>

namespace hex {

    Window::Window() {
        this->initGLFW();
        this->initImGui();
        this->setupNativeWindow();
        this->registerEventHandlers();
        this->setupEmergencyPopups();
    }

    Window::~Window() {
        RequestCloseImHex::unsubscribe(this);
        EventDPIChanged::unsubscribe(this);
        RequestSetPostProcessingShader::unsubscribe(this);

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

        EventDPIChanged::subscribe(this, [this](float oldScaling, float newScaling) {
            if (oldScaling == newScaling || oldScaling == 0 || newScaling == 0)
                return;

            int width, height;
            glfwGetWindowSize(m_window, &width, &height);

            width = float(width) * newScaling / oldScaling;
            height = float(height) * newScaling / oldScaling;

            ImHexApi::System::impl::setMainWindowSize(width, height);
            glfwSetWindowSize(m_window, width, height);
        });

        RequestSetPostProcessingShader::subscribe(this, [this](const std::string &vertexShader, const std::string &fragmentShader) {
            TaskManager::doLater([this, vertexShader, fragmentShader] {
                this->loadPostProcessingShader(vertexShader, fragmentShader);
            });
        });


        LayoutManager::registerLoadCallback([this](std::string_view line) {
            int width = 0, height = 0;
            sscanf(std::string(line).data(), "MainWindowSize=%d,%d", &width, &height);

            if (width > 0 && height > 0) {
                TaskManager::doLater([width, height, this]{
                    glfwSetWindowSize(m_window, width, height);
                });
            }
        });
    }

    void Window::setupEmergencyPopups() {
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
    }


    void Window::loadPostProcessingShader(const std::string &vertexShader, const std::string &fragmentShader) {
        m_postProcessingShader = gl::Shader(vertexShader, fragmentShader);
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

    void Window::unlockFrameRate()  {
        glfwPostEmptyEvent();
        m_shouldUnlockFrameRate = true;
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
        using namespace std::literals::chrono_literals;

        glfwShowWindow(m_window);

        double returnToIdleTime = 5.0;

        constexpr static auto IdleFPS = 5.0;
        constexpr static auto FrameRateUnlockDuration = 1;

        double idleFrameTime = 1.0 / IdleFPS;
        double targetFrameTime = -1.0;
        double longestExceededFrameTime = 0.0;
        while (!glfwWindowShouldClose(m_window)) {
            const auto maxFPS = ImHexApi::System::getTargetFPS();

            auto maxFrameTime = [&]() {
                if (maxFPS < 15) {
                    // Use the monitor's refresh rate
                    auto monitor = glfwGetPrimaryMonitor();
                    if (monitor != nullptr) {
                        auto videoMode = glfwGetVideoMode(monitor);
                        if (videoMode != nullptr) {
                            return 1.0 / videoMode->refreshRate;
                        }
                    }

                    // Fallback to 60 FPS if real monitor refresh rate cannot be determined
                    return 1.0 / 60.0;
                } else if (maxFPS > 200) {
                    // Don't limit the frame rate at all
                    return 0.0;
                } else {
                    // Do regular frame rate limiting
                    return 1.0 / maxFPS;
                }
            }();

            if (targetFrameTime < 0) {
                targetFrameTime = maxFrameTime;
            }

            auto frameTimeStart = glfwGetTime();

            glfwPollEvents();

            {
                int x = 0, y = 0;
                int width = 0, height = 0;
                glfwGetWindowPos(m_window, &x, &y);
                glfwGetWindowSize(m_window, &width, &height);

                ImHexApi::System::impl::setMainWindowPosition(x, y);
                ImHexApi::System::impl::setMainWindowSize(width, height);
            }

            while (!glfwGetWindowAttrib(m_window, GLFW_VISIBLE) || glfwGetWindowAttrib(m_window, GLFW_ICONIFIED)) {
                // If the application is minimized or not visible, don't render anything
                // glfwWaitEvents() is supposed to block the thread, but it does pretty often spuriously wake up anyway
                // so we need to keep looping here until the window is visible again, adding a short sleep to avoid busy-waiting
                glfwWaitEvents();
                std::this_thread::sleep_for(100ms);
            }

            static ImVec2 lastWindowSize = ImHexApi::System::getMainWindowSize();
            if (ImHexApi::System::impl::isWindowResizable()) {
                glfwSetWindowSizeLimits(m_window, 480_scaled, 360_scaled, GLFW_DONT_CARE, GLFW_DONT_CARE);
                lastWindowSize = ImHexApi::System::getMainWindowSize();
            } else {
                glfwSetWindowSizeLimits(m_window, lastWindowSize.x, lastWindowSize.y, lastWindowSize.x, lastWindowSize.y);
            }

            this->fullFrame();

            // Unlock frame rate if any mouse button is being held down to allow drag scrolling to be smooth
            if (ImGui::IsAnyMouseDown())
                unlockFrameRate();

            // Unlock frame rate if any modifier key is held down since they don't generate key repeat events
            if (
                ImGui::IsKeyPressed(ImGuiKey_LeftCtrl) || ImGui::IsKeyPressed(ImGuiKey_RightCtrl) ||
                ImGui::IsKeyPressed(ImGuiKey_LeftShift) || ImGui::IsKeyPressed(ImGuiKey_RightShift) ||
                ImGui::IsKeyPressed(ImGuiKey_LeftSuper) || ImGui::IsKeyPressed(ImGuiKey_RightSuper) ||
                ImGui::IsKeyPressed(ImGuiKey_LeftAlt) || ImGui::IsKeyPressed(ImGuiKey_RightAlt)
            ) {
                unlockFrameRate();
            }

            // Unlock frame rate if there's more than one viewport since these don't call the glfw callbacks registered here
            if (ImGui::GetPlatformIO().Viewports.size() > 1)
                unlockFrameRate();

            // Unlock frame rate if the frame rate was requested to be unlocked
            if (ImHexApi::System::impl::frameRateUnlockRequested()) {
                ImHexApi::System::impl::resetFrameRateUnlockRequested();

                glfwPostEmptyEvent();
                unlockFrameRate();
            }

            auto frameTime = glfwGetTime() - frameTimeStart;

            if (glfwGetTime() > returnToIdleTime) {
                targetFrameTime = idleFrameTime;
            }

            while (frameTime < targetFrameTime - longestExceededFrameTime) {
                auto remainingFrameTime = targetFrameTime - frameTime;
                glfwWaitEventsTimeout(std::min(remainingFrameTime, 1000.0));

                auto newFrameTime = glfwGetTime() - frameTimeStart;

                auto elapsedWaitTime = newFrameTime - frameTime;

                // Returned early; did not time out.
                if (elapsedWaitTime < remainingFrameTime && glfwGetTime() > returnToIdleTime && m_shouldUnlockFrameRate) {
                    returnToIdleTime = glfwGetTime() + FrameRateUnlockDuration;
                    targetFrameTime = maxFrameTime;
                }
                m_shouldUnlockFrameRate = false;

                frameTime = newFrameTime;
            }

            auto exceedTime = frameTime - targetFrameTime;
            if (!m_waitEventsBlocked)
                longestExceededFrameTime = std::max(exceedTime, longestExceededFrameTime);
            m_waitEventsBlocked = false;

            if (std::fmod(frameTimeStart, 5.0) < 0.01) {
                // Reset the longest exceeded frame time every 5 seconds
                longestExceededFrameTime = 0.0;
            }

            while (frameTime < maxFrameTime) {
                frameTime = glfwGetTime() - frameTimeStart;
                std::this_thread::sleep_for(100us);
            }

            ImHexApi::System::impl::setLastFrameTime(glfwGetTime() - frameTimeStart);
        }

        // Hide the window as soon as the render loop exits to make the window
        // disappear as soon as it's closed
        glfwHideWindow(m_window);
    }

    void Window::frameBegin() {
        auto &io = ImGui::GetIO();
        ImHexApi::Fonts::getDefaultFont().push();
        io.FontDefault = ImHexApi::Fonts::getDefaultFont();

        #if !defined(OS_WEB)
            {
                static bool lastAnyWindowFocused = false;
                bool anyWindowFocused = glfwGetWindowAttrib(m_window, GLFW_FOCUSED);

                if (!anyWindowFocused) {
                    const auto platformIo = ImGui::GetPlatformIO();
                    for (auto *viewport : platformIo.Viewports) {
                        if (platformIo.Platform_GetWindowFocus != nullptr && platformIo.Platform_GetWindowFocus(viewport)) {
                            anyWindowFocused = true;
                            break;
                        }
                    }
                }

                if (lastAnyWindowFocused != anyWindowFocused)
                    EventWindowFocused::post(anyWindowFocused);

                lastAnyWindowFocused = anyWindowFocused;
            }
        #endif

        // Start new ImGui Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        #if defined(IMGUI_TEST_ENGINE)
            if (ImGuiExt::ImGuiTestEngine::isEnabled())
                ImGuiTestEngine_ShowTestEngineWindows(m_testEngine, nullptr);
        #endif

        // Run all deferred calls
        TaskManager::runDeferredCalls();

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
                    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 0.2F);
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

        // Draw popup stack
        {
            static bool positionSet = false;
            static bool sizeSet = false;
            static double popupDelay = -2.0;
            static u32 displayFrameCount = 0;
            static bool popupClosed = true;

            static AutoReset<std::unique_ptr<impl::PopupBase>> currPopupStorage;
            static Lang name("");

            auto &currPopup = *currPopupStorage;

            if (auto &popups = impl::PopupBase::getOpenPopups(); !popups.empty()) {
                if (popupClosed) {
                    if (popupDelay <= -1.0) {
                        popupDelay = 0.2;
                    } else {
                        popupDelay -= io.DeltaTime;
                        if (popupDelay < 0 || popups.size() == 1) {
                            popupDelay = -2.0;
                            currPopup = std::move(popups.back());
                            name = Lang(currPopup->getUnlocalizedName());
                            displayFrameCount = 0;

                            ImGui::OpenPopup(name);
                            popupClosed = false;

                            popups.pop_back();
                        }
                    }
                }
            } else {
                popupClosed = true;
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
                                ImGui::GetCurrentContext()->MovingWindow = nullptr;
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
                    popupClosed = true;
                }
            }
        }

        TutorialManager::drawTutorial();

        // Draw Toasts
        {
            u32 index = 0;
            float yOffset = 0;
            for (const auto &toast : impl::ToastBase::getQueuedToasts() | std::views::take(4)) {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5_scaled);
                ImGui::SetNextWindowSize(ImVec2(350_scaled, 0));
                ImGui::SetNextWindowPos((ImHexApi::System::getMainWindowPosition() + ImHexApi::System::getMainWindowSize()) - scaled({ 10, 10 }) - scaled({ 0, yOffset }), ImGuiCond_Always, ImVec2(1, 1));
                ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, 100_scaled));
                if (ImGui::Begin(fmt::format("##Toast_{}", index).c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing)) {
                    auto drawList = ImGui::GetWindowDrawList();

                    const auto min = ImGui::GetWindowPos();

                    ImGui::Indent(5_scaled);
                    toast->draw();
                    ImGui::Unindent();

                    if (ImGui::IsWindowHovered() || toast->getAppearTime() <= 0)
                        toast->setAppearTime(ImGui::GetTime());

                    const auto max = min + ImGui::GetWindowSize();

                    drawList->PushClipRect(min, min + scaled({ 5, max.y - min.y }));
                    drawList->AddRectFilled(min, max, toast->getColor(), 5_scaled);
                    drawList->PopClipRect();

                    yOffset += ImGui::GetWindowSize().y + 10_scaled;
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
            const auto currentProvider = ImHexApi::Provider::get();
            const bool onWelcomeScreen = currentProvider == nullptr || !currentProvider->isAvailable();

            const auto windowPos = ImHexApi::System::getMainWindowPosition();
            float startY = windowPos.y + ImGui::GetTextLineHeight() + ((ImGui::GetTextLineHeight() + (ImGui::GetStyle().FramePadding.y * 2.0F)) * (onWelcomeScreen ? 1 : 2));
            const auto height = ImGui::GetTextLineHeightWithSpacing() * 1.5F;

            // Offset banner based on the size of the title bar. On macOS, it's slightly taller
            #if defined(OS_MACOS)
                startY += 2 * 8_scaled;
            #else
                startY += 2 * ImGui::GetStyle().FramePadding.y;
            #endif

            for (const auto &banner : impl::BannerBase::getOpenBanners() | std::views::take(3)) {
                auto &style = ImGui::GetStyle();
                ImGui::SetNextWindowPos(ImVec2(windowPos.x + 1_scaled, startY));
                ImGui::SetNextWindowSize(ImVec2(ImHexApi::System::getMainWindowSize().x - 2_scaled, height));
                ImGui::SetNextWindowViewport(viewport->ID);
                const auto backgroundColor = banner->getColor().Value;
                ImGui::PushStyleColor(ImGuiCol_WindowBg, backgroundColor);
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiExt::IsDarkBackground(backgroundColor) ? 0xFFFFFFFF : 0xFF000000);
                auto prevShadowOffset = style.WindowShadowOffsetDist;
                auto prevShadowAngle = style.WindowShadowOffsetAngle;
                style.WindowShadowOffsetDist = 12_scaled;
                style.WindowShadowOffsetAngle =  0.5F * std::numbers::pi_v<float>;
                ON_SCOPE_EXIT {
                    style.WindowShadowOffsetDist = prevShadowOffset;
                    style.WindowShadowOffsetAngle = prevShadowAngle;
                };
                if (ImGui::Begin(fmt::format("##Banner{}", static_cast<void*>(banner.get())).c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing)) {
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
                ImGui::PopStyleColor(2);

                startY += height;
            }

            std::erase_if(impl::BannerBase::getOpenBanners(), [](const auto &banner) {
                return banner->shouldClose();
            });
        }
    }

    void Window::frame() {
        auto &io = ImGui::GetIO();

        ShortcutManager::resetLastActivatedMenu();

        if (const auto &fullScreenView = ContentRegistry::Views::impl::getFullScreenView(); fullScreenView == nullptr) {

            // Loop through all views and draw them
            static ImGuiWindow *nextFocusWindow = nullptr;

            for (auto &[name, view] : ContentRegistry::Views::impl::getEntries() | std::views::reverse) {
                ImGui::GetCurrentContext()->NextWindowData.ClearFlags();

                // Draw always visible views
                view->drawAlwaysVisibleContent();
                view->trackViewState();

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

                auto window = ImGui::FindWindowByName(view->getName().c_str());
                if (window != nullptr && window->DockNode == nullptr)
                    ImGui::SetNextWindowBgAlpha(1.0F);

                if (nextFocusWindow == window && !view->didWindowJustOpen() && !ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopup)) {
                    ImGui::SetNextWindowFocus();
                    nextFocusWindow = nullptr;
                }

                // Draw view
                view->draw();

                // If the window was just opened, it wasn't found above, so try to find it again
                if (window == nullptr)
                    window = ImGui::FindWindowByName(view->getName().c_str());

                if (window != nullptr) {
                    if (window->Appearing) {
                        if (view->shouldDefaultFocus()) {
                            nextFocusWindow = window;
                        }
                    }

                    if (view->getWindowOpenState()) {
                        // Get the currently focused view
                        auto windowName = View::toWindowName(name);
                        bool focused = false;

                        const bool windowIsPopup = (window->Flags & ImGuiWindowFlags_Popup) == ImGuiWindowFlags_Popup;
                        if (!windowIsPopup) {
                            ImGui::Begin(windowName.c_str());

                            // Detect if the window is focused
                            focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_NoPopupHierarchy);
                            view->setFocused(focused);
                        }

                        if (view->didWindowJustOpen()) {
                            // Dock the window if it's not already docked
                            if (!windowIsPopup && !ImGui::IsWindowDocked())
                                ImGui::DockBuilderDockWindow(windowName.c_str(), ImHexApi::System::getMainDockSpaceId());

                            EventViewOpened::post(view.get());
                        }

                        // Pass on currently pressed keys to the shortcut handler
                        if (!windowIsPopup) {
                            for (const auto &key : m_pressedKeys) {
                                ShortcutManager::process(view.get(), io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl, io.KeyAlt, io.KeyShift, io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeySuper, focused, key);
                            }

                            ImGui::End();
                        }
                    } else if (view->didWindowJustClose()) {
                        EventViewClosed::post(view.get());
                    }
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

        ImHexApi::Fonts::getDefaultFont().pop();

        // Finalize ImGui frame
        ImGui::Render();

        // Compare the previous frame buffer to the current one to determine if the window content has changed
        // If not, there's no point in sending the draw data off to the GPU and swapping buffers
        // NOTE: For anybody looking at this code and thinking "why not just hash the buffer and compare the hashes",
        // the reason is that hashing the buffer is significantly slower than just comparing the buffers directly.
        // The buffer might become quite large if there's a lot of vertices on the screen, but it's still usually less than
        // 10MB (out of which only the active portion needs to actually be compared) which is worth the ~60x speedup.
        bool shouldRender = [this] {
            if (m_postProcessingShader.isValid() && m_postProcessingShader.hasUniform("Time"))
                return true;

            static std::vector<u8> previousVtxData;
            static size_t previousVtxDataSize = 0;

            size_t totalVtxDataSize = 0;

            for (const auto *viewport : ImGui::GetPlatformIO().Viewports) {
                const auto drawData = viewport->DrawData;
                for (int n = 0; n < drawData->CmdListsCount; n++) {
                    totalVtxDataSize += drawData->CmdLists[n]->VtxBuffer.size() * sizeof(ImDrawVert);
                }
            }

            if (totalVtxDataSize != previousVtxDataSize) {
                previousVtxDataSize = totalVtxDataSize;
                previousVtxData.resize(totalVtxDataSize);
                return true;
            }

            size_t offset = 0;
            for (const auto *viewport : ImGui::GetPlatformIO().Viewports) {
                const auto drawData = viewport->DrawData;
                for (int n = 0; n < drawData->CmdListsCount; n++) {
                    const auto& vtxBuffer = drawData->CmdLists[n]->VtxBuffer;
                    const std::size_t bufSize = vtxBuffer.size() * sizeof(ImDrawVert);

                    if (std::memcmp(previousVtxData.data() + offset, vtxBuffer.Data, bufSize) != 0) {
                        std::memcpy(previousVtxData.data() + offset, vtxBuffer.Data, bufSize);
                        return true;
                    }

                    offset += bufSize;
                }
            }

            return false;
        }();


        GLFWwindow *backupContext = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backupContext);

        if (shouldRender) {
            #if !defined(OS_WEB)
                if (m_postProcessingShader.isValid())
                    drawWithShader();
                else
                    drawImGui();
            #else
                drawImGui();
            #endif

            glfwSwapBuffers(m_window);
        }

        #if defined(IMGUI_TEST_ENGINE)
            ImGuiTestEngine_PostSwap(m_testEngine);
        #endif

        // Process layout load requests
        // NOTE: This needs to be done before a new frame is started, otherwise ImGui won't handle docking correctly
        LayoutManager::process();
        WorkspaceManager::process();
    }

    void Window::drawImGui() {
        auto* drawData = ImGui::GetDrawData();

        // Avoid accidentally clearing the viewport when the application is minimized,
        // otherwise the OS will display an empty frame during window restore on macOS
        if (drawData->DisplaySize.x != 0 && drawData->DisplaySize.y != 0) {
            int displayWidth, displayHeight;
            glfwGetFramebufferSize(m_window, &displayWidth, &displayHeight);
            glViewport(0, 0, displayWidth, displayHeight);
            glClearColor(0.00F, 0.00F, 0.00F, 0.00F);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
    }

    void Window::drawWithShader() {
        #if !defined(OS_WEB)
            int displayWidth, displayHeight;
            glfwGetFramebufferSize(m_window, &displayWidth, &displayHeight);

            GLuint fbo, texture;
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);

            // Create a texture to render into
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, displayWidth, displayHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Attach the texture to the framebuffer
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

            // Check if framebuffer is complete
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                log::error("Framebuffer is not complete!");
            }

            glBindFramebuffer(GL_FRAMEBUFFER, fbo);

            drawImGui();

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            GLuint quadVAO, quadVBO;
            constexpr static std::array QuadVertices = {
                // positions   // texCoords
                -1.0F,  1.0F,  0.0F, 1.0F,
                -1.0F, -1.0F,  0.0F, 0.0F,
                 1.0F, -1.0F,  1.0F, 0.0F,

                -1.0F,  1.0F,  0.0F, 1.0F,
                 1.0F, -1.0F,  1.0F, 0.0F,
                 1.0F,  1.0F,  1.0F, 1.0F
            };

            glGenVertexArrays(1, &quadVAO);
            glGenBuffers(1, &quadVBO);
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(QuadVertices), QuadVertices.data(), GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void *>(2 * sizeof(float)));
            glBindVertexArray(0);

            m_postProcessingShader.bind();

            m_postProcessingShader.setUniform("Time", static_cast<float>(glfwGetTime()));
            m_postProcessingShader.setUniform("Resolution", gl::Vector<float, 2>{{ float(displayWidth), float(displayHeight) }});

            glBindVertexArray(quadVAO);
            glBindTexture(GL_TEXTURE_2D, texture);
            glClearColor(0.00F, 0.00F, 0.00F, 0.00F);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            m_postProcessingShader.unbind();

            glDeleteVertexArrays(1, &quadVAO);
            glDeleteBuffers(1, &quadVBO);
            glDeleteTextures(1, &texture);
            glDeleteFramebuffers(1, &fbo);
        #endif
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
            } catch (const std::system_error &) { //NOLINT(bugprone-empty-catch): we can't log it
                // Catch and ignore system error that might be thrown when too many errors are being logged to a file
            }
        });

        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);

        // Don't hide the window on the web build, otherwise the mouse cursor offset will not
        // be calculated correctly if the canvas is not filling the entire screen
        #if !defined(OS_WEB)
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        #endif

        configureGLFW();

        if (initialWindowProperties.has_value()) {
            glfwWindowHint(GLFW_MAXIMIZED, initialWindowProperties->maximized);
        }

        int monitorX = 0, monitorY = 0;
        int monitorWidth = std::numeric_limits<int>::max(), monitorHeight = std::numeric_limits<int>::max();
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        if (monitor != nullptr) {
            const GLFWvidmode *mode = glfwGetVideoMode(monitor);
            if (mode != nullptr) {
                glfwGetMonitorPos(monitor, &monitorX, &monitorY);

                monitorWidth = mode->width;
                monitorHeight = mode->height;
            }
        }

        float maxWindowCreationWidth = monitorWidth / 1_scaled;
        float maxWindowCreationHeight = monitorHeight / 1_scaled;

        // Wayland auto-maximizes windows that take up 80% or more of the monitor size
        // Limit the size to take up slightly less than that at max
        // glfwGetPlatform() is only available since GLFW 3.4
        #if GLFW_VERSION_MAJOR > 4 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4)
            if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
                const static auto SizeMultiplier = sqrt(0.79);
                maxWindowCreationWidth  *= SizeMultiplier;
                maxWindowCreationHeight *= SizeMultiplier;
            }
        #endif

        maxWindowCreationWidth -= 50_scaled;
        maxWindowCreationHeight -= 50_scaled;

        // Create window
        m_windowTitle = "ImHex";
        m_window = glfwCreateWindow(
            std::min(1280_scaled, maxWindowCreationWidth),
            std::min(720_scaled, maxWindowCreationHeight),
            m_windowTitle.c_str(),
            nullptr, nullptr
        );

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
        if (monitorWidth != std::numeric_limits<int>::max() && monitorHeight != std::numeric_limits<int>::max()) {
            int windowWidth, windowHeight;
            glfwGetWindowSize(m_window, &windowWidth, &windowHeight);

            glfwSetWindowPos(m_window, monitorX + (monitorWidth - windowWidth) / 2, monitorY + (monitorHeight - windowHeight) / 2);
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

            width  = std::min(width,  monitorWidth  - int(50_scaled));
            height = std::min(height, monitorHeight - int(100_scaled));

            if (initialWindowProperties.has_value()) {
                width  = initialWindowProperties->width;
                height = initialWindowProperties->height;
            }

            ImHexApi::System::impl::setMainWindowSize(width, height);
            glfwSetWindowSize(m_window, width, height);
        }

        static const auto unlockFrameRate = [](GLFWwindow *, auto ...) {
            auto win = static_cast<Window *>(glfwGetWindowUserPointer(ImHexApi::System::getMainWindowHandle()));
            if (win == nullptr)
                return;

            win->unlockFrameRate();
        };

        static const auto markWaitEventsBlocked = [](GLFWwindow *, auto ...) {
            auto win = static_cast<Window *>(glfwGetWindowUserPointer(ImHexApi::System::getMainWindowHandle()));
            if (win == nullptr)
                return;

            win->m_waitEventsBlocked = true;
        };

        static const auto isMainWindow = [](const GLFWwindow *window) {
            return window == ImHexApi::System::getMainWindowHandle();
        };

        // Register window move callback
        glfwSetWindowPosCallback(m_window, [](GLFWwindow *window, int x, int y) {
            unlockFrameRate(window);
            markWaitEventsBlocked(window);

            if (!isMainWindow(window)) return;

            ImHexApi::System::impl::setMainWindowPosition(x, y);

            int width = 0, height = 0;
            glfwGetWindowSize(window, &width, &height);
            ImHexApi::System::impl::setMainWindowPosition(x, y);
            ImHexApi::System::impl::setMainWindowSize(width, height);

        });

        // Register window resize callback
        glfwSetWindowSizeCallback(m_window, [](GLFWwindow *window, [[maybe_unused]] int width, [[maybe_unused]] int height) {
            unlockFrameRate(window);
            markWaitEventsBlocked(window);

            if (!isMainWindow(window)) return;

            #if !defined(OS_WINDOWS)
                if (!glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
                    int x = 0, y = 0;
                    glfwGetWindowPos(window, &x, &y);
                    ImHexApi::System::impl::setMainWindowPosition(x, y);
                    ImHexApi::System::impl::setMainWindowSize(width, height);
                }
            #endif

            #if defined(OS_MACOS)
                // Stop widgets registering hover effects while the window is being resized
                if (macosIsWindowBeingResizedByUser(window)) {
                    ImGui::GetIO().MousePos = ImVec2();
                }
            #elif defined(OS_WEB)
                auto win = static_cast<Window *>(glfwGetWindowUserPointer(ImHexApi::System::getMainWindowHandle()));
                win->fullFrame();
            #endif
        });

        glfwSetCursorPosCallback(m_window, unlockFrameRate);
        glfwSetMouseButtonCallback(m_window, unlockFrameRate);
        glfwSetScrollCallback(m_window, unlockFrameRate);
        glfwSetWindowFocusCallback(m_window, [](GLFWwindow *window, int focused) {
            unlockFrameRate(window);
            ImHexApi::System::impl::setMainWindowFocusState(focused);
        });

        glfwSetWindowMaximizeCallback(m_window, [](GLFWwindow *window, int) {
            glfwShowWindow(window);
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

                    auto win = static_cast<Window *>(glfwGetWindowUserPointer(ImHexApi::System::getMainWindowHandle()));
                    win->m_pressedKeys.insert(key);
                }
            }
        });

        // Register window close callback
        glfwSetWindowCloseCallback(m_window, [](GLFWwindow *window) {
            unlockFrameRate(window);

            if (!isMainWindow(window)) return;

            EventWindowClosing::post(window);
        });

        glfwSetWindowSizeLimits(m_window, 480_scaled, 360_scaled, GLFW_DONT_CARE, GLFW_DONT_CARE);
    }

    void Window::resize(i32 width, i32 height) {
        glfwSetWindowSize(m_window, width, height);
    }

    void Window::initImGui() {
        IMGUI_CHECKVERSION();

        // Initialize ImGui and all other ImGui extensions
        GImGui              = ImGui::CreateContext();
        GImPlot             = ImPlot::CreateContext();
        ImPlot3D::GImPlot3D = ImPlot3D::CreateContext();
        GImNodes            = ImNodes::CreateContext();

        #if defined(IMGUI_TEST_ENGINE)
            m_testEngine = ImGuiTestEngine_CreateContext();
            auto &testEngineIo = ImGuiTestEngine_GetIO(m_testEngine);
            testEngineIo.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info;
            testEngineIo.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;

            EventRegisterImGuiTests::post(m_testEngine);

            ImGuiTestEngine_Start(m_testEngine, ImGui::GetCurrentContext());
        #endif

        ImGuiIO &io       = ImGui::GetIO();
        ImGuiStyle &style = ImGui::GetStyle();

        ImNodes::GetStyle().Flags = ImNodesStyleFlags_NodeOutline | ImNodesStyleFlags_GridLines;

        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.ConfigDragClickToInputText = true;

        if (glfwGetPrimaryMonitor() != nullptr) {
            if (ImHexApi::System::isMultiWindowModeEnabled()) {
                io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

                // Enable viewport window OS decorations on Linux so that the window can be moved around on Wayland
                #if defined (OS_LINUX)
                    io.ConfigViewportsNoDecoration = false;
                #endif
            }
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

        style.ScaleAllSizes(ImHexApi::System::getGlobalScale());
        auto scale = ImHexApi::System::getNativeScale();
        io.DisplayFramebufferScale = ImVec2(scale, scale);

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
            if (ImHexApi::System::getGLVersion() >= SemanticVersion(4,1,0)) {
                ImGui_ImplOpenGL3_Init("#version 410");
            } else {
                ImGui_ImplOpenGL3_Init("#version 130");
            }
        #endif

        ImGui_ImplGlfw_SetCallbacksChainForAllWindows(true);

        for (const auto &plugin : PluginManager::getPlugins())
            plugin.setImGuiContext(ImGui::GetCurrentContext());

        RequestInitThemeHandlers::post();
    }

    void Window::exitGLFW() {
        glfwDestroyWindow(m_window);

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
