#include "window.hpp"

#include <hex.hpp>

#include <hex/api/plugin_manager.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/stacktrace.hpp>

#include <chrono>
#include <csignal>
#include <set>
#include <thread>
#include <cassert>

#include <romfs/romfs.hpp>
#include <llvm/Demangle/Demangle.h>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <implot.h>
#include <implot_internal.h>
#include <imnodes.h>
#include <imnodes_internal.h>

#include <fonts/codicons_font.h>

#include <hex/api/project_file_manager.hpp>

#include <GLFW/glfw3.h>

namespace hex {

    using namespace std::literals::chrono_literals;

    // Custom signal handler to print various information and a stacktrace when the application crashes
    static void signalHandler(int signalNumber, const std::string &signalName) {
        log::fatal("Terminating with signal '{}' ({})", signalName, signalNumber);

        // Trigger an event so that plugins can handle crashes
        EventManager::post<EventAbnormalTermination>(signalNumber);

        // Detect if the crash was due to an uncaught exception
        if (std::uncaught_exceptions() > 0) {
            log::fatal("Uncaught exception thrown!");
        }

        // Reset the signal handler to the default handler
        std::signal(signalNumber, SIG_DFL);

        // Print stack trace
        for (const auto &stackFrame : stacktrace::getStackTrace()) {
            if (stackFrame.line == 0)
                log::fatal("  {}", stackFrame.function);
            else
                log::fatal("  ({}:{}) | {}",  stackFrame.file, stackFrame.line, stackFrame.function);
        }
    
        // Trigger a breakpoint if we're in a debug build or raise the signal again for the default handler to handle it
        #if defined(DEBUG)
            assert(!"Debug build, triggering breakpoint");
        #else
            std::raise(signalNumber);
        #endif
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

        auto logoData      = romfs::get("logo.png");
        this->m_logoTexture = ImGui::Texture(reinterpret_cast<const ImU8 *>(logoData.data()), logoData.size());

        ContentRegistry::Settings::impl::store();
        EventManager::post<EventSettingsChanged>();
        EventManager::post<EventWindowInitialized>();
    }

    Window::~Window() {
        EventManager::unsubscribe<EventProviderDeleted>(this);
        EventManager::unsubscribe<RequestCloseImHex>(this);
        EventManager::unsubscribe<RequestUpdateWindowTitle>(this);
        EventManager::unsubscribe<EventAbnormalTermination>(this);
        EventManager::unsubscribe<RequestOpenPopup>(this);

        this->exitImGui();
        this->exitGLFW();
    }

    void Window::registerEventHandlers() {
        // Initialize default theme
        EventManager::post<RequestChangeTheme>("Dark");

        // Handle the close window request by telling GLFW to shut down
        EventManager::subscribe<RequestCloseImHex>(this, [this](bool noQuestions) {
            glfwSetWindowShouldClose(this->m_window, GLFW_TRUE);

            if (!noQuestions)
                EventManager::post<EventWindowClosing>(this->m_window);
        });

        // Handle updating the window title
        EventManager::subscribe<RequestUpdateWindowTitle>(this, [this]() {
            std::string title = "ImHex";

            if (ProjectFile::hasPath()) {
                // If a project is open, show the project name instead of the file name

                title += " - Project " + hex::limitStringLength(ProjectFile::getPath().stem().string(), 32);

                if (ImHexApi::Provider::isDirty())
                    title += " (*)";

            } else if (ImHexApi::Provider::isValid()) {
                auto provider = ImHexApi::Provider::get();
                if (provider != nullptr) {
                    title += " - " + hex::limitStringLength(provider->getName(), 32);

                    if (provider->isDirty())
                        title += " (*)";

                    if (!provider->isWritable())
                        title += " (Read Only)";
                }
            }

            this->m_windowTitle = title;
            glfwSetWindowTitle(this->m_window, title.c_str());
        });

        constexpr static auto CrashBackupFileName = "crash_backup.hexproj";

        // Save a backup project when the application crashes
        EventManager::subscribe<EventAbnormalTermination>(this, [this](int) {
            ImGui::SaveIniSettingsToDisk(wolv::util::toUTF8String(this->m_imguiSettingsPath).c_str());

            if (!ImHexApi::Provider::isDirty())
                return;

            for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Config)) {
                if (ProjectFile::store(path / CrashBackupFileName))
                    break;
            }
        });

        // Handle opening popups
        EventManager::subscribe<RequestOpenPopup>(this, [this](auto name) {
            std::scoped_lock lock(this->m_popupMutex);

            this->m_popupsToOpen.push_back(name);
        });

        // Register signal handlers
        {
            #define HANDLE_SIGNAL(name) \
            std::signal(name, [](int signalNumber){ \
                signalHandler(signalNumber, #name); \
            });

            HANDLE_SIGNAL(SIGSEGV)
            HANDLE_SIGNAL(SIGILL)
            HANDLE_SIGNAL(SIGABRT)
            HANDLE_SIGNAL(SIGFPE)

            #undef HANDLE_SIGNAL
        }

        std::set_terminate([]{
            try {
                std::rethrow_exception(std::current_exception());
            } catch (std::exception &ex) {
                log::fatal(
                        "Program terminated with uncaught exception: {}()::what() -> {}",
                        llvm::itaniumDemangle(typeid(ex).name(), nullptr, nullptr, nullptr),
                        ex.what()
                );
            }
            EventManager::post<EventAbnormalTermination>(0);
        });
    }

    void Window::loop() {
        this->m_lastFrameTime = glfwGetTime();
        while (!glfwWindowShouldClose(this->m_window)) {
            if (!glfwGetWindowAttrib(this->m_window, GLFW_VISIBLE) || glfwGetWindowAttrib(this->m_window, GLFW_ICONIFIED)) {
                // If the application is minimized or not visible, don't render anything
                glfwWaitEvents();
            } else {
                glfwPollEvents();

                // If no events have been received in a while, lower the frame rate
                {
                    // If the mouse is down, the mouse is moving or a popup is open, we don't want to lower the frame rate
                    bool frameRateUnlocked =
                            ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopupId) ||
                            TaskManager::getRunningTaskCount() > 0 ||
                            this->m_mouseButtonDown ||
                            this->m_hadEvent ||
                            !this->m_pressedKeys.empty();

                    // Calculate the time until the next frame
                    const double timeout = std::max(0.0, (1.0 / 5.0) - (glfwGetTime() - this->m_lastFrameTime));

                    // If the frame rate has been unlocked for 5 seconds, lock it again
                    if ((this->m_lastFrameTime - this->m_frameRateUnlockTime) > 5 && this->m_frameRateTemporarilyUnlocked && !frameRateUnlocked) {
                        this->m_frameRateTemporarilyUnlocked = false;
                    }

                    // If the frame rate is locked, wait for events with a timeout
                    if (frameRateUnlocked || this->m_frameRateTemporarilyUnlocked) {
                        if (!this->m_frameRateTemporarilyUnlocked) {
                            this->m_frameRateTemporarilyUnlocked = true;
                            this->m_frameRateUnlockTime = this->m_lastFrameTime;
                        }
                    } else {
                        glfwWaitEventsTimeout(timeout);
                    }

                    this->m_hadEvent = false;
                }
            }

            // Render frame
            this->frameBegin();
            this->frame();
            this->frameEnd();

            // Limit frame rate
            const auto targetFps = ImHexApi::System::getTargetFPS();
            if (targetFps <= 200) {
                auto leftoverFrameTime = i64((this->m_lastFrameTime + 1 / targetFps - glfwGetTime()) * 1000);
                if (leftoverFrameTime > 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(leftoverFrameTime));
            }

            this->m_lastFrameTime = glfwGetTime();
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
            if (ImGui::MenuItem(LangEntry(name), shortcut.toString().c_str(), false, enabledCallback()))
                callback();
        } else {
            if (ImGui::BeginMenu(LangEntry(name))) {
                createNestedMenu({ menuItems.begin() + 1, menuItems.end() }, shortcut, callback, enabledCallback);
                ImGui::EndMenu();
            }
        }
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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Render main dock space
        if (ImGui::Begin("ImHexDockSpace", nullptr, windowFlags)) {
            auto drawList = ImGui::GetWindowDrawList();
            ImGui::PopStyleVar();
            auto sidebarPos   = ImGui::GetCursorPos();
            auto sidebarWidth = ContentRegistry::Interface::impl::getSidebarItems().empty() ? 0 : 30_scaled;

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
            {
                ImGui::SetCursorPos(sidebarPos);

                static i32 openWindow = -1;
                u32 index             = 0;
                ImGui::PushID("SideBarWindows");
                for (const auto &[icon, callback] : ContentRegistry::Interface::impl::getSidebarItems()) {
                    ImGui::SetCursorPosY(sidebarPos.y + sidebarWidth * index);

                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabActive));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabHovered));

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

                    ImGui::PopStyleColor(3);

                    bool open = static_cast<u32>(openWindow) == index;
                    if (open) {
                        ImGui::SetNextWindowPos(ImGui::GetWindowPos() + sidebarPos + ImVec2(sidebarWidth - 2_scaled, 0));
                        ImGui::SetNextWindowSize(ImVec2(250_scaled, dockSpaceSize.y + ImGui::GetStyle().FramePadding.y + 2_scaled));

                        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1);
                        if (ImGui::Begin("Window", &open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
                            callback();
                        }
                        ImGui::End();
                        ImGui::PopStyleVar();
                    }

                    ImGui::NewLine();
                    index++;
                }
                ImGui::PopID();
            }

            // Render main menu
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            if (ImGui::BeginMainMenuBar()) {

                if (ImHexApi::System::isBorderlessWindowModeEnabled()) {
                    auto menuBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight();
                    ImGui::SetCursorPosX(5);
                    ImGui::Image(this->m_logoTexture, ImVec2(menuBarHeight, menuBarHeight));
                }

                for (const auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMainMenuItems()) {
                    if (ImGui::BeginMenu(LangEntry(menuItem.unlocalizedName))) {
                        ImGui::EndMenu();
                    }
                }

                for (auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMenuItems()) {
                    const auto &[unlocalizedNames, shortcut, callback, enabledCallback] = menuItem;
                    createNestedMenu(unlocalizedNames, shortcut, callback, enabledCallback);
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

                ImGui::EndMenuBar();
            }

            this->beginNativeWindowFrame();

            drawList->AddLine(ImGui::GetWindowPos() + ImVec2(sidebarWidth - 2, 0), ImGui::GetWindowPos() + ImGui::GetWindowSize() - ImVec2(dockSpaceSize.x + 2, footerHeight - ImGui::GetStyle().FramePadding.y - 2), ImGui::GetColorU32(ImGuiCol_Separator));
            drawList->AddLine(ImGui::GetWindowPos() + ImVec2(sidebarWidth, ImGui::GetCurrentWindow()->MenuBarHeight()), ImGui::GetWindowPos() + ImVec2(ImGui::GetWindowSize().x, ImGui::GetCurrentWindow()->MenuBarHeight()), ImGui::GetColorU32(ImGuiCol_Separator));
        }
        ImGui::End();
        ImGui::PopStyleVar(2);

        // Plugin load error popups. These are not translated because they should always be readable, no matter if any localization could be loaded or not
        {
            auto drawPluginFolderTable = []() {
                ImGui::UnderlinedText("Plugin folders");
                if (ImGui::BeginTable("plugins", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit)) {
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

        // Run all deferred calls
        TaskManager::runDeferredCalls();

        // Draw main menu popups
        for (auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMenuItems()) {
            const auto &[unlocalizedNames, shortcut, callback, enabledCallback] = menuItem;

            if (ImGui::BeginPopup(unlocalizedNames.front().c_str())) {
                createNestedMenu({ unlocalizedNames.begin() + 1, unlocalizedNames.end() }, shortcut, callback, enabledCallback);
                ImGui::EndPopup();
            }
        }

        EventManager::post<EventFrameBegin>();
    }

    void Window::frame() {
        auto &io = ImGui::GetIO();

        // Loop through all views and draw them
        for (auto &[name, view] : ContentRegistry::Views::impl::getEntries()) {
            ImGui::GetCurrentContext()->NextWindowData.ClearFlags();

            // Draw always visible views
            view->drawAlwaysVisible();

            // Skip views that shouldn't be processed currently
            if (!view->shouldProcess())
                continue;

            // Draw view
            if (view->isAvailable()) {
                ImGui::SetNextWindowSizeConstraints(scaled(view->getMinSize()), scaled(view->getMaxSize()));
                view->drawContent();
            }

            // Handle per-view shortcuts
            if (view->getWindowOpenState()) {
                auto window    = ImGui::FindWindowByName(view->getName().c_str());
                bool hasWindow = window != nullptr;
                bool focused   = false;

                // Get the currently focused view
                if (hasWindow && !(window->Flags & ImGuiWindowFlags_Popup)) {
                    ImGui::Begin(View::toWindowName(name).c_str());

                    focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_NoPopupHierarchy);
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
        EventManager::post<EventFrameEnd>();

        // Clean up all tasks that are done
        TaskManager::collectGarbage();

        this->endNativeWindowFrame();

        // Render UI
        ImGui::Render();

        int displayWidth, displayHeight;
        glfwGetFramebufferSize(this->m_window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(0.00F, 0.00F, 0.00F, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        GLFWwindow *backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);

        glfwSwapBuffers(this->m_window);
    }

    void Window::initGLFW() {
        glfwSetErrorCallback([](int error, const char *desc) {
            log::error("GLFW Error [{}] : {}", error, desc);
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
        #else
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        #endif

        glfwWindowHint(GLFW_DECORATED, ImHexApi::System::isBorderlessWindowModeEnabled() ? GL_FALSE : GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        // Create window
        this->m_windowTitle = "ImHex";
        this->m_window      = glfwCreateWindow(1280_scaled, 720_scaled, this->m_windowTitle.c_str(), nullptr, nullptr);

        glfwSetWindowUserPointer(this->m_window, this);

        if (this->m_window == nullptr) {
            log::fatal("Failed to create window!");
            std::abort();
        }

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

            ImHexApi::System::impl::setMainWindowPosition(x, y);
        }

        // Set up initial window size
        {
            int width = 0, height = 0;
            glfwGetWindowSize(this->m_window, &width, &height);
            glfwSetWindowSize(this->m_window, width, height);
            ImHexApi::System::impl::setMainWindowSize(width, height);
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
                win->m_mouseButtonDown = true;
            else if (action == GLFW_RELEASE)
                win->m_mouseButtonDown = false;
            win->processEvent();
        });

        // Register scrolling callback
        glfwSetScrollCallback(this->m_window, [](GLFWwindow *window, double xOffset, double yOffset) {
            hex::unused(xOffset, yOffset);

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->processEvent();
        });

        // Register key press callback
        glfwSetKeyCallback(this->m_window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
            hex::unused(mods);

            auto keyName = glfwGetKeyName(key, scancode);
            if (keyName != nullptr)
                key = std::toupper(keyName[0]);

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));

            if (action == GLFW_PRESS || action == GLFW_REPEAT) {
                win->m_pressedKeys.push_back(key);
            }
            win->processEvent();
        });

        // Register cursor position callback
        glfwSetCursorPosCallback(this->m_window, [](GLFWwindow *window, double x, double y) {
            hex::unused(x, y);

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->processEvent();
        });

        // Register window close callback
        glfwSetWindowCloseCallback(this->m_window, [](GLFWwindow *window) {
            EventManager::post<EventWindowClosing>(window);
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
                    EventManager::post<RequestOpenFile>(path);
            }
        });

        glfwSetWindowSizeLimits(this->m_window, 720_scaled, 480_scaled, GLFW_DONT_CARE, GLFW_DONT_CARE);

        glfwShowWindow(this->m_window);
    }

    void Window::initImGui() {
        IMGUI_CHECKVERSION();

        auto fonts = View::getFontAtlas();

        // Initialize ImGui and all other ImGui extensions
        GImGui   = ImGui::CreateContext(fonts);
        GImPlot  = ImPlot::CreateContext();
        GImNodes = ImNodes::CreateContext();

        ImGuiIO &io       = ImGui::GetIO();
        ImGuiStyle &style = ImGui::GetStyle();

        // Configure window alpha and rounding to make them not stand out when detached
        style.Alpha          = 1.0F;
        style.WindowRounding = 0.0F;

        ImNodes::GetStyle().Flags = ImNodesStyleFlags_NodeOutline | ImNodesStyleFlags_GridLines;

        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.FontGlobalScale = 1.0F;

        // Disable multi-window support on Wayland since it doesn't support it
        if (glfwGetPrimaryMonitor() != nullptr) {
            auto sessionType = hex::getEnvironmentVariable("XDG_SESSION_TYPE");
            bool multiWindowEnabled = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.multi_windows", 1) != 0;

            if ((!sessionType || !hex::containsIgnoreCase(*sessionType, "wayland")) && multiWindowEnabled)
                io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        }

        for (auto &entry : fonts->ConfigData)
            io.Fonts->ConfigData.push_back(entry);

        io.ConfigViewportsNoTaskBarIcon = false;

        ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
        ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkCreationOnSnap);

        // Allow ImNodes links to always be detached without holding down any button
        {
            static bool always = true;
            ImNodes::GetIO().LinkDetachWithModifierClick.Modifier = &always;
        }

        io.UserData = new ImGui::ImHexCustomData();

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

            handler.ReadLineFn = [](ImGuiContext *, ImGuiSettingsHandler *, void *, const char *line) {
                for (auto &[name, view] : ContentRegistry::Views::impl::getEntries()) {
                    std::string format = view->getUnlocalizedName() + "=%d";
                    sscanf(line, format.c_str(), &view->getWindowOpenState());
                }
                for (auto &[name, function, detached] : ContentRegistry::Tools::impl::getEntries()) {
                    std::string format = name + "=%d";
                    sscanf(line, format.c_str(), &detached);
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

            for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Config)) {
                if (std::fs::exists(dir) && fs::isPathWritable(dir)) {
                    this->m_imguiSettingsPath = dir / "interface.ini";
                    io.IniFilename            = nullptr;
                    break;
                }
            }

            if (!this->m_imguiSettingsPath.empty() && wolv::io::fs::exists(this->m_imguiSettingsPath))
                ImGui::LoadIniSettingsFromDisk(wolv::util::toUTF8String(this->m_imguiSettingsPath).c_str());
        }

        ImGui_ImplGlfw_InitForOpenGL(this->m_window, true);

        #if defined(OS_MACOS)
            ImGui_ImplOpenGL3_Init("#version 150");
        #else
            ImGui_ImplOpenGL3_Init("#version 130");
        #endif

        for (const auto &plugin : PluginManager::getPlugins())
            plugin.setImGuiContext(ImGui::GetCurrentContext());

        EventManager::post<RequestInitThemeHandlers>();
    }

    void Window::exitGLFW() {
        glfwDestroyWindow(this->m_window);
        glfwTerminate();
    }

    void Window::exitImGui() {
        delete static_cast<ImGui::ImHexCustomData *>(ImGui::GetIO().UserData);

        ImGui::SaveIniSettingsToDisk(wolv::util::toUTF8String(this->m_imguiSettingsPath).c_str());

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

}
