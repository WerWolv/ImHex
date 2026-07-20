#include "window_backend.hpp"
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
#include <hex/api/events/events_interaction.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/default_paths.hpp>

#if defined(OS_MACOS)
    #include <hex/helpers/utils_macos.hpp>
#endif

#include <hex/providers/provider.hpp>

#include <hex/ui/view.hpp>
#include <hex/ui/popup.hpp>
#include <hex/ui/banner.hpp>

#include <cmath>
#include <limits>
#include <numbers>
#include <utility>

#include <romfs/romfs.hpp>

#include <imgui.h>
#include <imgui_internal.h>
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

#include <hex/ui/toast.hpp>
#include <wolv/utils/guards.hpp>
#include <fmt/printf.h>
#include <hex/helpers/opengl.hpp>

namespace hex {

    static double getTime() {
        static const auto Start = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
    }

    Window *Window::s_mainWindow = nullptr;

    Window::Window() {
        s_mainWindow = this;
        this->initWindow();
        this->initImGui();
        this->setupNativeWindow();
        this->registerEventHandlers();
        this->setupEmergencyPopups();
    }

    Window::~Window() {
        RequestCloseImHex::unsubscribe(this);
        EventDPIChanged::unsubscribe(this);
        RequestSetPostProcessingShader::unsubscribe(this);

        EventWindowDeinitializing::post();

        this->exitImGui();
        this->exitWindow();

        ImHexApi::System::impl::setWindowBackend(nullptr);
        s_mainWindow = nullptr;
    }

    Window* Window::getMainWindow() {
        return s_mainWindow;
    }

    ImHexApi::System::WindowBackend& Window::getBackend() {
        return *m_backend;
    }

    void Window::registerEventHandlers() {
        // Initialize default theme
        RequestChangeTheme::post("Dark");

        // Handle close requests through the selected window backend.
        RequestCloseImHex::subscribe(this, [this](bool noQuestions) {
            m_backend->setShouldClose(true);

            if (!noQuestions)
                EventWindowClosing::post();
        });

        EventDPIChanged::subscribe(this, [this](float oldScaling, float newScaling) {
            if (oldScaling == newScaling || oldScaling == 0 || newScaling == 0)
                return;

            const auto properties = m_backend->getWindowProperties();
            auto width = i32(properties.width);
            auto height = i32(properties.height);

            width = float(width) * newScaling / oldScaling;
            height = float(height) * newScaling / oldScaling;

            ImHexApi::System::impl::setMainWindowSize(width, height);
            m_backend->setSize(width, height);
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
                    m_backend->setSize(width, height);
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
        m_backend->wakeEventLoop();
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

        m_backend->show();

        double returnToIdleTime = 5.0;

        constexpr static auto IdleFPS = 5.0;
        constexpr static auto FrameRateUnlockDuration = 1;

        double idleFrameTime = 1.0 / IdleFPS;
        double targetFrameTime = -1.0;
        double longestExceededFrameTime = 0.0;
        while (!m_backend->shouldClose()) {
            const auto maxFPS = ImHexApi::System::getTargetFPS();

            auto maxFrameTime = [&]() {
                if (maxFPS < 15) {
                    // Use the monitor's refresh rate
                    if (const auto monitor = m_backend->getPrimaryMonitor(); monitor.has_value())
                        return 1.0 / monitor->refreshRate;

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

            auto frameTimeStart = getTime();

            m_backend->pollEvents();

            {
                const auto properties = m_backend->getWindowProperties();
                ImHexApi::System::impl::setMainWindowPosition(properties.x, properties.y);
                ImHexApi::System::impl::setMainWindowSize(properties.width, properties.height);
            }

            while (!m_backend->isVisible() || m_backend->isMinimized()) {
                // If the application is minimized or not visible, don't render anything
                m_backend->waitEvents();
                std::this_thread::sleep_for(100ms);
            }

            static ImVec2 lastWindowSize = ImHexApi::System::getMainWindowSize();
            if (ImHexApi::System::impl::isWindowResizable()) {
                m_backend->setSizeLimits(480_scaled, 360_scaled, std::nullopt, std::nullopt);
                lastWindowSize = ImHexApi::System::getMainWindowSize();
            } else {
                m_backend->setSizeLimits(lastWindowSize.x, lastWindowSize.y, lastWindowSize.x, lastWindowSize.y);
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

            // Unlock frame rate while additional platform windows are active.
            if (ImGui::GetPlatformIO().Viewports.size() > 1)
                unlockFrameRate();

            // Unlock frame rate if the frame rate was requested to be unlocked
            if (ImHexApi::System::impl::frameRateUnlockRequested()) {
                ImHexApi::System::impl::resetFrameRateUnlockRequested();

                m_backend->wakeEventLoop();
                unlockFrameRate();
            }

            auto frameTime = getTime() - frameTimeStart;

            if (getTime() > returnToIdleTime) {
                targetFrameTime = idleFrameTime;
            }

            while (frameTime < targetFrameTime - longestExceededFrameTime) {
                auto remainingFrameTime = targetFrameTime - frameTime;
                const bool receivedEvent = m_backend->waitEvents(std::min(remainingFrameTime, 1000.0));

                auto newFrameTime = getTime() - frameTimeStart;

                auto elapsedWaitTime = newFrameTime - frameTime;

                // Returned early; did not time out.
                if (receivedEvent && elapsedWaitTime < remainingFrameTime && getTime() > returnToIdleTime && m_shouldUnlockFrameRate) {
                    returnToIdleTime = getTime() + FrameRateUnlockDuration;
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
                frameTime = getTime() - frameTimeStart;
                std::this_thread::sleep_for(100us);
            }

            ImHexApi::System::impl::setLastFrameTime(getTime() - frameTimeStart);
        }

        // Hide the window as soon as the render loop exits to make the window
        // disappear as soon as it's closed
        m_backend->hide();
    }

    void Window::frameBegin() {
        auto &io = ImGui::GetIO();
        ImHexApi::Fonts::getDefaultFont().push();
        io.FontDefault = ImHexApi::Fonts::getDefaultFont();

        #if !defined(OS_WEB)
            {
                static bool lastAnyWindowFocused = false;
                bool anyWindowFocused = m_backend->isFocused();

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
        m_backend->newImGuiFrame();
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

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

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
            if (std::exchange(m_forceRender, false))
                return true;

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


        m_backend->renderImGuiPlatformWindows();

        if (shouldRender) {
            #if !defined(OS_WEB)
                if (m_postProcessingShader.isValid())
                    drawWithShader();
                else
                    drawImGui();
            #else
                drawImGui();
            #endif

            m_backend->swapBuffers();
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
            const auto [displayWidth, displayHeight] = m_backend->getFramebufferSize();
            glViewport(0, 0, displayWidth, displayHeight);
            glClearColor(0.00F, 0.00F, 0.00F, 0.00F);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
    }

    void Window::drawWithShader() {
        #if !defined(OS_WEB)
            const auto [displayWidth, displayHeight] = m_backend->getFramebufferSize();

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

            m_postProcessingShader.setUniform("Time", static_cast<float>(getTime()));
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

    void Window::initWindow() {
        const auto initialWindowProperties = ImHexApi::System::getInitialWindowProperties();

        ImHexApi::System::WindowBackend::Config config {
            .title = "ImHex",
            .width = static_cast<i32>(1280_scaled),
            .height = static_cast<i32>(720_scaled),
            .glMajor = 3,
            .glMinor = 1,
            .coreProfile = false,
            .forwardCompatible = true,
            .resizable = true,
            .decorated = true,
            .transparent = false,
            .visible = false,
            .maximized = initialWindowProperties.has_value() && initialWindowProperties->maximized,
            .highPixelDensity = true,
            .scaleToMonitor = true,
            .applicationId = "imhex",
            .webCanvasSelector = "#canvas",
            .swapInterval = 0,
        };
        this->configureWindowBackend(config);

        m_backend = createWindowBackend();
        if (m_backend == nullptr) {
            log::fatal("Failed to create window backend!");
            std::abort();
        }

        const auto monitor = m_backend->getPrimaryMonitor();
        float maxWindowCreationWidth = monitor.has_value()
            ? monitor->width / 1_scaled
            : std::numeric_limits<float>::max();
        float maxWindowCreationHeight = monitor.has_value()
            ? monitor->height / 1_scaled
            : std::numeric_limits<float>::max();

        // Wayland auto-maximizes windows occupying at least 80% of the monitor.
        if (m_backend->isWayland()) {
            const auto SizeMultiplier = std::sqrt(0.79);
            maxWindowCreationWidth *= SizeMultiplier;
            maxWindowCreationHeight *= SizeMultiplier;
        }

        maxWindowCreationWidth -= 50_scaled;
        maxWindowCreationHeight -= 50_scaled;
        config.width = static_cast<i32>(std::min<float>(1280_scaled, maxWindowCreationWidth));
        config.height = static_cast<i32>(std::min<float>(720_scaled, maxWindowCreationHeight));
        m_windowTitle = config.title;

        ImHexApi::System::WindowBackend::Callbacks callbacks {
            .moved = [this](i32 x, i32 y) {
                m_waitEventsBlocked = true;
                const auto properties = m_backend->getWindowProperties();
                ImHexApi::System::impl::setMainWindowPosition(x, y);
                ImHexApi::System::impl::setMainWindowSize(properties.width, properties.height);
            },
            .resized = [this](i32, i32) {
                m_waitEventsBlocked = true;
                m_forceRender = true;

                if (!m_backend->isMinimized()) {
                    const auto properties = m_backend->getWindowProperties();
                    ImHexApi::System::impl::setMainWindowPosition(properties.x, properties.y);
                    ImHexApi::System::impl::setMainWindowSize(properties.width, properties.height);
                }

                if (m_backend->isMaximized())
                    m_backend->show();

                #if defined(OS_MACOS)
                    // Disable hover effects during a live resize.
                    if (macosIsWindowBeingResizedByUser(m_backend->getNativeWindow().handle))
                        ImGui::GetIO().MousePos = ImVec2();
                #elif defined(OS_WEB)
                    this->fullFrame();
                #endif
            },
            .framebufferResized = [this](i32 width, i32 height) {
                m_forceRender = true;
                this->unlockFrameRate();
                glViewport(0, 0, width, height);
            },
            .focused = [](bool focused) {
                ImHexApi::System::impl::setMainWindowFocusState(focused);
            },
            .keyPressed = [this](Keys key) {
                m_pressedKeys.insert(key);
            },
            .inputActivity = [this] {
                this->unlockFrameRate();
            },
            .closeRequested = [] {
                EventWindowClosing::post();
            },
            .fileDropped = [](const std::fs::path &path) {
                EventFileDropped::post(path);
            },
            .refreshRequested = [this] {
                m_forceRender = true;
                this->fullFrame();
            },
        };

        if (!m_backend->create(config, std::move(callbacks))) {
            log::fatal("Failed to create window!");
            std::abort();
        }

        ImHexApi::System::impl::setWindowBackend(m_backend.get());

        // Force the window opaque and ensure its graphics context is active.
        m_backend->setOpacity(1.0F);
        m_backend->makeContextCurrent();

        // Center the window before applying any persisted position.
        if (monitor.has_value()) {
            const auto properties = m_backend->getWindowProperties();
            m_backend->setPosition(
                monitor->x + (monitor->width - static_cast<i32>(properties.width)) / 2,
                monitor->y + (monitor->height - static_cast<i32>(properties.height)) / 2);
        }

        {
            auto properties = m_backend->getWindowProperties();
            if (initialWindowProperties.has_value()) {
                properties.x = initialWindowProperties->x;
                properties.y = initialWindowProperties->y;
            }

            ImHexApi::System::impl::setMainWindowPosition(properties.x, properties.y);
            m_backend->setPosition(properties.x, properties.y);
        }

        {
            const auto properties = m_backend->getWindowProperties();
            i32 width = static_cast<i32>(properties.width);
            i32 height = static_cast<i32>(properties.height);

            if (monitor.has_value()) {
                width = std::min(width, monitor->width - static_cast<i32>(50_scaled));
                height = std::min(height, monitor->height - static_cast<i32>(100_scaled));
            }

            if (initialWindowProperties.has_value()) {
                width = static_cast<i32>(initialWindowProperties->width);
                height = static_cast<i32>(initialWindowProperties->height);
            }

            ImHexApi::System::impl::setMainWindowSize(width, height);
            m_backend->setSize(width, height);
        }

        m_backend->setSizeLimits(480_scaled, 360_scaled, std::nullopt, std::nullopt);
    }

    void Window::resize(i32 width, i32 height) {
        m_backend->setSize(width, height);
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

        if (m_backend->getPrimaryMonitor().has_value()) {
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


        if (!m_backend->initializeImGui()) {
            log::fatal("Failed to initialize ImGui for the selected window backend!");
            std::abort();
        }

        #if defined(OS_MACOS)
            ImGui_ImplOpenGL3_Init("#version 150");
        #elif defined(OS_WEB)
            ImGui_ImplOpenGL3_Init();
        #else
            if (ImHexApi::System::getGLVersion() >= SemanticVersion(4,1,0)) {
                ImGui_ImplOpenGL3_Init("#version 410");
            } else {
                ImGui_ImplOpenGL3_Init("#version 130");
            }
        #endif

        for (const auto &plugin : PluginManager::getPlugins())
            plugin.setImGuiContext(ImGui::GetCurrentContext());

        RequestInitThemeHandlers::post();
    }

    void Window::exitWindow() {
        if (m_backend != nullptr) {
            m_backend->destroy();
            m_backend.reset();
        }
    }

    void Window::exitImGui() {
        ImGui_ImplOpenGL3_Shutdown();
        m_backend->shutdownImGui();

        ImNodes::DestroyContext();
        ImPlot3D::DestroyContext();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

}
