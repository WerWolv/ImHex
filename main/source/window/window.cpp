#include "window.hpp"

#include <hex.hpp>

#include <hex/api/plugin_manager.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/paths.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/file.hpp>

#include <chrono>
#include <csignal>
#include <iostream>
#include <numeric>
#include <set>
#include <thread>
#include <assert.h>

#include <romfs/romfs.hpp>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_freetype.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <implot.h>
#include <implot_internal.h>
#include <imnodes.h>
#include <imnodes_internal.h>

#include <fontawesome_font.h>
#include <codicons_font.h>
#include <unifont_font.h>

#include <hex/helpers/project_file_handler.hpp>
#include "init/tasks.hpp"

#include <GLFW/glfw3.h>

#include <nlohmann/json.hpp>

namespace hex {

    using namespace std::literals::chrono_literals;

    void *ImHexSettingsHandler_ReadOpenFn(ImGuiContext *ctx, ImGuiSettingsHandler *, const char *) {
        return ctx;    // Unused, but the return value has to be non-null
    }

    void ImHexSettingsHandler_ReadLine(ImGuiContext *, ImGuiSettingsHandler *handler, void *, const char *line) {
        for (auto &[name, view] : ContentRegistry::Views::getEntries()) {
            std::string format = std::string(view->getUnlocalizedName()) + "=%d";
            sscanf(line, format.c_str(), &view->getWindowOpenState());
        }
    }

    void ImHexSettingsHandler_WriteAll(ImGuiContext *ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf) {
        buf->reserve(buf->size() + 0x20);    // Ballpark reserve

        buf->appendf("[%s][General]\n", handler->TypeName);

        for (auto &[name, view] : ContentRegistry::Views::getEntries()) {
            buf->appendf("%s=%d\n", name.c_str(), view->getWindowOpenState());
        }

        buf->append("\n");
    }

    Window::Window() {
        {
            for (const auto &[argument, value] : ImHexApi::System::getInitArguments()) {
                if (argument == "no-plugins") {
                    ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("No Plugins"); });
                }
            }
        }

        this->initGLFW();
        this->initImGui();
        this->setupNativeWindow();

        EventManager::subscribe<RequestCloseImHex>(this, [this](bool noQuestions) {
            glfwSetWindowShouldClose(this->m_window, true);

            if (!noQuestions)
                EventManager::post<EventWindowClosing>(this->m_window);
        });

        EventManager::subscribe<EventFileUnloaded>(this, [] {
            EventManager::post<RequestChangeWindowTitle>("");
        });

        EventManager::subscribe<RequestChangeWindowTitle>(this, [this](std::string windowTitle) {
            std::string title = "ImHex";

            if (ImHexApi::Provider::isValid()) {
                if (!windowTitle.empty())
                    title += " - " + windowTitle;

                if (ProjectFile::hasUnsavedChanges())
                    title += " (*)";
            }

            this->m_windowTitle = title;
            glfwSetWindowTitle(this->m_window, title.c_str());
        });

        constexpr auto CrashBackupFileName = "crash_backup.hexproj";

        EventManager::subscribe<EventAbnormalTermination>(this, [CrashBackupFileName](int signal) {
            if (!ProjectFile::hasUnsavedChanges())
                return;

            for (const auto &path : hex::getPath(ImHexPath::Config)) {
                if (ProjectFile::store((fs::path(path) / CrashBackupFileName).string()))
                    break;
            }
        });

        EventManager::subscribe<RequestOpenPopup>(this, [this](auto name) {
            this->m_popupsToOpen.push_back(name);
        });

        auto signalHandler = [](int signalNumber) {
            EventManager::post<EventAbnormalTermination>(signalNumber);

            if (std::uncaught_exceptions() > 0) {
                log::fatal("Uncaught exception thrown!");
            }

            // Let's not loop on this...
            std::signal(signalNumber, nullptr);

            #if defined(DEBUG)
                assert(false);
            #else
                std::raise(signalNumber);
            #endif
        };

        std::signal(SIGTERM, signalHandler);
        std::signal(SIGSEGV, signalHandler);
        std::signal(SIGINT, signalHandler);
        std::signal(SIGILL, signalHandler);
        std::signal(SIGABRT, signalHandler);
        std::signal(SIGFPE, signalHandler);

        auto imhexLogo = romfs::get("logo.png");
        this->m_logoTexture = ImGui::LoadImageFromMemory(reinterpret_cast<const ImU8 *>(imhexLogo.data()), imhexLogo.size());

        ContentRegistry::Settings::store();
        EventManager::post<EventSettingsChanged>();
    }

    Window::~Window() {
        this->exitImGui();
        this->exitGLFW();

        EventManager::unsubscribe<EventFileUnloaded>(this);
        EventManager::unsubscribe<RequestCloseImHex>(this);
        EventManager::unsubscribe<RequestChangeWindowTitle>(this);
        EventManager::unsubscribe<EventAbnormalTermination>(this);
        EventManager::unsubscribe<RequestOpenPopup>(this);
    }

    void Window::loop() {
        this->m_lastFrameTime = glfwGetTime();
        while (!glfwWindowShouldClose(this->m_window)) {
            if (!glfwGetWindowAttrib(this->m_window, GLFW_VISIBLE) || glfwGetWindowAttrib(this->m_window, GLFW_ICONIFIED)) {
                glfwWaitEvents();

            } else {
                double timeout = (1.0 / 5.0) - (glfwGetTime() - this->m_lastFrameTime);
                timeout = timeout > 0 ? timeout : 0;
                glfwWaitEventsTimeout(ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopupId) || Task::getRunningTaskCount() > 0 ? 0 : timeout);
            }


            this->frameBegin();
            this->frame();
            this->frameEnd();
        }
    }

    void Window::frameBegin() {

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        if (ImGui::Begin("DockSpace", nullptr, windowFlags)) {
            auto drawList = ImGui::GetWindowDrawList();
            ImGui::PopStyleVar();
            auto sidebarPos = ImGui::GetCursorPos();
            auto sidebarWidth = ContentRegistry::Interface::getSidebarItems().empty() ? 0 : 30_scaled;

            ImGui::SetCursorPosX(sidebarWidth);

            auto footerHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2 + 1_scaled;
            auto dockSpaceSize = ImVec2(ImHexApi::System::getMainWindowSize().x - sidebarWidth, ImGui::GetContentRegionAvail().y - footerHeight);

            auto dockId = ImGui::DockSpace(ImGui::GetID("MainDock"), dockSpaceSize);
            ImHexApi::System::impl::setMainDockSpaceId(dockId);

            drawList->AddRectFilled(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize() - ImVec2(dockSpaceSize.x, footerHeight - ImGui::GetStyle().FramePadding.y - 1_scaled), ImGui::GetColorU32(ImGuiCol_MenuBarBg));

            ImGui::Separator();
            ImGui::SetCursorPosX(8);
            for (const auto &callback : ContentRegistry::Interface::getFooterItems()) {
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

            {
                ImGui::SetCursorPos(sidebarPos);

                static i32 openWindow = -1;
                u32 index = 0;
                ImGui::PushID("SideBarWindows");
                for (const auto &[icon, callback] : ContentRegistry::Interface::getSidebarItems()) {
                    ImGui::SetCursorPosY(sidebarPos.y + sidebarWidth * index);

                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabActive));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabHovered));

                    ImGui::BeginDisabled(!ImHexApi::Provider::isValid());
                    {
                        if (ImGui::Button(icon.c_str(), ImVec2(sidebarWidth, sidebarWidth))) {
                            if (openWindow == index)
                                openWindow = -1;
                            else
                                openWindow = index;
                        }
                    }
                    ImGui::EndDisabled();

                    ImGui::PopStyleColor(3);

                    bool open = openWindow == index;
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

            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            if (ImGui::BeginMainMenuBar()) {

                auto menuBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight();
                ImGui::SetCursorPosX(5);
                ImGui::Image(this->m_logoTexture, ImVec2(menuBarHeight, menuBarHeight));

                for (const auto &[priority, menuItem] : ContentRegistry::Interface::getMainMenuItems()) {
                    if (ImGui::BeginMenu(LangEntry(menuItem.unlocalizedName))) {
                        ImGui::EndMenu();
                    }
                }

                std::set<std::string> encounteredMenus;
                for (auto &[priority, menuItem] : ContentRegistry::Interface::getMenuItems()) {
                    if (ImGui::BeginMenu(LangEntry(menuItem.unlocalizedName))) {
                        auto [iter, inserted] = encounteredMenus.insert(menuItem.unlocalizedName);
                        if (!inserted)
                            ImGui::Separator();

                        menuItem.callback();
                        ImGui::EndMenu();
                    }
                }

                this->drawTitleBar();

                ImGui::EndMainMenuBar();
            }
            ImGui::PopStyleVar();

            // Draw toolbar
            if (ImGui::BeginMenuBar()) {

                for (const auto &callback : ContentRegistry::Interface::getToolbarItems()) {
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

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        if (ImGui::BeginPopupModal("No Plugins", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::TextUnformatted("No ImHex plugins loaded (including the built-in plugin)!");
            ImGui::TextUnformatted("Make sure you at least got the builtin plugin in your plugins folder.");
            ImGui::TextUnformatted("To find out where your plugin folder is, check ImHex' Readme.");
            ImGui::EndPopup();
        }

        this->m_popupsToOpen.remove_if([](const auto &name) {
            if (ImGui::IsPopupOpen(name.c_str()))
                return true;
            else
                ImGui::OpenPopup(name.c_str());

            return false;
        });

        EventManager::post<EventFrameBegin>();
    }

    void Window::frame() {
        {
            auto &calls = ImHexApi::Tasks::getDeferredCalls();
            for (const auto &callback : calls)
                callback();
            calls.clear();
        }

        View::drawCommonInterfaces();

        for (auto &[name, view] : ContentRegistry::Views::getEntries()) {
            ImGui::GetCurrentContext()->NextWindowData.ClearFlags();

            view->drawAlwaysVisible();

            if (!view->shouldProcess())
                continue;

            if (view->isAvailable()) {
                ImGui::SetNextWindowSizeConstraints(scaled(view->getMinSize()), scaled(view->getMaxSize()));
                view->drawContent();
            }

            if (view->getWindowOpenState()) {
                auto window = ImGui::FindWindowByName(view->getName().c_str());
                bool hasWindow = window != nullptr;
                bool focused = false;


                if (hasWindow && !(window->Flags & ImGuiWindowFlags_Popup)) {
                    ImGui::Begin(View::toWindowName(name).c_str());

                    focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_NoPopupHierarchy);
                    ImGui::End();
                }

                auto &io = ImGui::GetIO();
                for (const auto &key : this->m_pressedKeys) {
                    ShortcutManager::process(view, io.KeyCtrl, io.KeyAlt, io.KeyShift, io.KeySuper, focused, key);
                }
            }
        }

        this->m_pressedKeys.clear();
    }

    void Window::frameEnd() {
        EventManager::post<EventFrameEnd>();

        this->endNativeWindowFrame();
        ImGui::Render();

        int displayWidth, displayHeight;
        glfwGetFramebufferSize(this->m_window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        GLFWwindow *backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);

        glfwSwapBuffers(this->m_window);

        const auto targetFps = ImHexApi::System::getTargetFPS();
        if (targetFps <= 200)
            std::this_thread::sleep_for(std::chrono::milliseconds(u64((this->m_lastFrameTime + 1 / targetFps - glfwGetTime()) * 1000)));

        this->m_lastFrameTime = glfwGetTime();
    }

    void Window::drawWelcomeScreen() {

    }

    void Window::resetLayout() const {

        if (auto &layouts = ContentRegistry::Interface::getLayouts(); !layouts.empty()) {
            auto &[name, function] = layouts[0];

            function(ImHexApi::System::getMainDockSpaceId());
        }
    }

    void Window::initGLFW() {
        glfwSetErrorCallback([](int error, const char *desc) {
            log::error("GLFW Error [{}] : {}", error, desc);
        });

        if (!glfwInit()) {
            log::fatal("Failed to initialize GLFW!");
            std::abort();
        }

#if defined(OS_WINDOWS)
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
#elif defined(OS_MACOS)
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        this->m_windowTitle = "ImHex";
        this->m_window = glfwCreateWindow(1280_scaled, 720_scaled, this->m_windowTitle.c_str(), nullptr, nullptr);

        glfwSetWindowUserPointer(this->m_window, this);

        if (this->m_window == nullptr) {
            log::fatal("Failed to create window!");
            std::abort();
        }

        glfwMakeContextCurrent(this->m_window);
        glfwSwapInterval(1);

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

        {
            int x = 0, y = 0;
            glfwGetWindowPos(this->m_window, &x, &y);

            ImHexApi::System::impl::setMainWindowPosition(x, y);
        }

        {
            int width = 0, height = 0;
            glfwGetWindowSize(this->m_window, &width, &height);
            glfwSetWindowSize(this->m_window, width, height);
            ImHexApi::System::impl::setMainWindowSize(width, height);
        }

        glfwSetWindowPosCallback(this->m_window, [](GLFWwindow *window, int x, int y) {
            ImHexApi::System::impl::setMainWindowPosition(x, y);

            if (auto g = ImGui::GetCurrentContext(); g == nullptr || g->WithinFrameScope) return;

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->frameBegin();
            win->frame();
            win->frameEnd();
        });

        glfwSetWindowSizeCallback(this->m_window, [](GLFWwindow *window, int width, int height) {
            ImHexApi::System::impl::setMainWindowSize(width, height);

            if (auto g = ImGui::GetCurrentContext(); g == nullptr || g->WithinFrameScope) return;

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));
            win->frameBegin();
            win->frame();
            win->frameEnd();
        });

        glfwSetKeyCallback(this->m_window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
            auto keyName = glfwGetKeyName(key, scancode);
            if (keyName != nullptr)
                key = std::toupper(keyName[0]);

            auto win = static_cast<Window *>(glfwGetWindowUserPointer(window));

            if (action == GLFW_PRESS) {
                auto &io = ImGui::GetIO();

                win->m_pressedKeys.push_back(key);
                io.KeysDown[key] = true;
                io.KeyCtrl = (mods & GLFW_MOD_CONTROL) != 0;
                io.KeyShift = (mods & GLFW_MOD_SHIFT) != 0;
                io.KeyAlt = (mods & GLFW_MOD_ALT) != 0;
            } else if (action == GLFW_RELEASE) {
                auto &io = ImGui::GetIO();
                io.KeysDown[key] = false;
                io.KeyCtrl = (mods & GLFW_MOD_CONTROL) != 0;
                io.KeyShift = (mods & GLFW_MOD_SHIFT) != 0;
                io.KeyAlt = (mods & GLFW_MOD_ALT) != 0;
            }
        });

        glfwSetDropCallback(this->m_window, [](GLFWwindow *window, int count, const char **paths) {
            if (count != 1)
                return;

            for (u32 i = 0; i < count; i++) {
                auto path = std::filesystem::path(paths[i]);

                bool handled = false;
                for (const auto &[extensions, handler] : ContentRegistry::FileHandler::getEntries()) {
                    for (const auto &extension : extensions) {
                        if (path.extension() == extension) {
                            if (!handler(path))
                                log::error("Handler for extensions '{}' failed to process file!", extension);

                            handled = true;
                            break;
                        }
                    }
                }

                if (!handled)
                    EventManager::post<RequestOpenFile>(path.string());
            }
        });

        glfwSetWindowCloseCallback(this->m_window, [](GLFWwindow *window) {
            EventManager::post<EventWindowClosing>(window);
        });

        glfwSetWindowSizeLimits(this->m_window, 720_scaled, 480_scaled, GLFW_DONT_CARE, GLFW_DONT_CARE);

        glfwShowWindow(this->m_window);
    }

    void Window::initImGui() {
        IMGUI_CHECKVERSION();

        auto fonts = View::getFontAtlas();

        GImGui = ImGui::CreateContext(fonts);
        GImPlot = ImPlot::CreateContext();
        GImNodes = ImNodes::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        ImGuiStyle &style = ImGui::GetStyle();

        style.Alpha = 1.0F;
        style.WindowRounding = 0.0F;

        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
#if !defined(OS_LINUX)
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif

        for (auto &entry : fonts->ConfigData)
            io.Fonts->ConfigData.push_back(entry);

        io.ConfigViewportsNoTaskBarIcon = false;
        io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
        io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
        io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
        io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
        io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
        io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
        io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
        io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
        io.KeyMap[ImGuiKey_Insert] = GLFW_KEY_INSERT;
        io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
        io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
        io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
        io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
        io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
        io.KeyMap[ImGuiKey_KeyPadEnter] = GLFW_KEY_KP_ENTER;
        io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
        io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
        io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
        io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
        io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
        io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

        ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
        ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkCreationOnSnap);

        {
            static bool always = true;
            ImNodes::GetIO().LinkDetachWithModifierClick.Modifier = &always;
        }

        io.UserData = new ImGui::ImHexCustomData();

        style.ScaleAllSizes(ImHexApi::System::getGlobalScale());

        {
            GLsizei width, height;
            u8 *fontData;

            io.Fonts->GetTexDataAsRGBA32(&fontData, &width, &height);

            // Create new font atlas
            GLuint tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA8, GL_UNSIGNED_INT, fontData);
            io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(tex));
        }

        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.IndentSpacing = 10.0F;

        // Install custom settings handler
        ImGuiSettingsHandler handler;
        handler.TypeName = "ImHex";
        handler.TypeHash = ImHashStr("ImHex");
        handler.ReadOpenFn = ImHexSettingsHandler_ReadOpenFn;
        handler.ReadLineFn = ImHexSettingsHandler_ReadLine;
        handler.WriteAllFn = ImHexSettingsHandler_WriteAll;
        handler.UserData = this;
        ImGui::GetCurrentContext()->SettingsHandlers.push_back(handler);

        static std::string iniFileName;
        for (const auto &dir : hex::getPath(ImHexPath::Config)) {
            if (std::filesystem::exists(dir)) {
                iniFileName = (dir / "interface.ini").string();
                break;
            }
        }
        io.IniFilename = iniFileName.c_str();

        ImGui_ImplGlfw_InitForOpenGL(this->m_window, true);

        ImGui_ImplOpenGL3_Init("#version 150");

        for (const auto &plugin : PluginManager::getPlugins())
            plugin.setImGuiContext(ImGui::GetCurrentContext());
    }

    void Window::exitGLFW() {
        glfwDestroyWindow(this->m_window);
        glfwTerminate();
    }

    void Window::exitImGui() {
        delete static_cast<ImGui::ImHexCustomData *>(ImGui::GetIO().UserData);

        ImNodes::PopAttributeFlag();
        ImNodes::PopAttributeFlag();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImNodes::DestroyContext();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

}
