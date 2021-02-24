#include "window.hpp"

#include <hex.hpp>
#include <hex/api/content_registry.hpp>

#include <iostream>
#include <numeric>
#include <typeinfo>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_freetype.h>
#include <imgui_imhex_extensions.h>

#include <fontawesome_font.h>

#include "helpers/plugin_handler.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace hex {

    void *ImHexSettingsHandler_ReadOpenFn(ImGuiContext *ctx, ImGuiSettingsHandler *, const char *) {
        return ctx; // Unused, but the return value has to be non-null
    }

    void ImHexSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler *handler, void *, const char* line) {
        for (auto &view : ContentRegistry::Views::getEntries()) {
            std::string format = std::string(view->getName()) + "=%d";
            sscanf(line, format.c_str(), &view->getWindowOpenState());
        }
    }

    void ImHexSettingsHandler_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf) {
        buf->reserve(buf->size() + 0x20); // Ballpark reserve

        buf->appendf("[%s][General]\n", handler->TypeName);

        for (auto &view : ContentRegistry::Views::getEntries()) {
            buf->appendf("%s=%d\n", typeid(*view).name(), view->getWindowOpenState());
        }

        buf->append("\n");
    }

    Window::Window(int &argc, char **&argv) {
        hex::SharedData::mainArgc = argc;
        hex::SharedData::mainArgv = argv;

        this->initGLFW();
        this->initImGui();

        EventManager::subscribe(Events::SettingsChanged, this, [](auto) -> std::any {
            {
                auto theme = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.color");

                if (theme.has_value()) {
                    switch (static_cast<int>(theme.value())) {
                        default:
                        case 0: /* Dark theme */
                            ImGui::StyleColorsDark();
                            ImGui::StyleCustomColorsDark();
                            break;
                        case 1: /* Light theme */
                            ImGui::StyleColorsLight();
                            ImGui::StyleCustomColorsLight();
                            break;
                        case 2: /* Classic theme */
                            ImGui::StyleColorsClassic();
                            ImGui::StyleCustomColorsClassic();
                            break;
                    }
                    ImGui::GetStyle().Colors[ImGuiCol_DockingEmptyBg] = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
                }
            }

            {
                auto language = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.language");

                if (language.has_value())
                    LangEntry::loadLanguage(static_cast<std::string>(language.value()));
            }

            return { };
        });

        EventManager::subscribe(Events::FileLoaded, this, [this](auto userData) -> std::any {
            auto path = std::any_cast<std::string>(userData);

            this->m_recentFiles.push_front(path);

            {
                std::list<std::string> uniques;
                for (auto &file : this->m_recentFiles) {

                    bool exists = false;
                    for (auto &unique : uniques) {
                        if (file == unique)
                            exists = true;
                    }

                    if (!exists)
                        uniques.push_back(file);

                    if (uniques.size() > 5)
                        break;
                }
                this->m_recentFiles = uniques;
            }

            {
                std::vector<std::string> recentFilesVector;
                std::copy(this->m_recentFiles.begin(), this->m_recentFiles.end(), std::back_inserter(recentFilesVector));

                ContentRegistry::Settings::write("hex.builtin.setting.imhex", "hex.builtin.setting.imhex.recent_files", recentFilesVector);
            }

            return { };
        });

        EventManager::subscribe(Events::CloseImHex, this, [this](auto) -> std::any {
            glfwSetWindowShouldClose(this->m_window, true);

            return { };
        });

        this->initPlugins();

        ContentRegistry::Settings::load();
        View::postEvent(Events::SettingsChanged);

        for (const auto &path : ContentRegistry::Settings::read("hex.builtin.setting.imhex", "hex.builtin.setting.imhex.recent_files"))
            this->m_recentFiles.push_back(path);
    }

    Window::~Window() {
        this->deinitImGui();
        this->deinitGLFW();
        ContentRegistry::Settings::store();

        this->deinitPlugins();

        EventManager::unsubscribe(Events::SettingsChanged, this);
        EventManager::unsubscribe(Events::FileLoaded, this);
        EventManager::unsubscribe(Events::CloseImHex, this);
    }

    void Window::loop() {
        while (!glfwWindowShouldClose(this->m_window)) {
            this->frameBegin();

            for (const auto &call : View::getDeferedCalls())
                call();
            View::getDeferedCalls().clear();

            for (auto &view : ContentRegistry::Views::getEntries()) {
                view->drawAlwaysVisible();

                if (!view->shouldProcess())
                    continue;

                auto minSize = view->getMinSize();
                minSize.x *= this->m_globalScale;
                minSize.y *= this->m_globalScale;

                ImGui::SetNextWindowSizeConstraints(minSize, view->getMaxSize());
                view->drawContent();
            }

            View::drawCommonInterfaces();

            #ifdef DEBUG
                if (this->m_demoWindowOpen)
                    ImGui::ShowDemoWindow(&this->m_demoWindowOpen);
            #endif

            this->frameEnd();
        }
    }

    bool Window::setFont(const std::filesystem::path &path) {
        if (!std::filesystem::exists(path))
            return false;

        auto &io = ImGui::GetIO();

        // If we have a custom font, then rescaling is unnecessary and will make it blurry
        io.FontGlobalScale = 1.0f;

        // Load font data & build atlas
        std::uint8_t *px;
        int w, h;

        ImVector<ImWchar> ranges;
        ImFontGlyphRangesBuilder glyphRangesBuilder;

        glyphRangesBuilder.AddRanges(io.Fonts->GetGlyphRangesDefault());
        glyphRangesBuilder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
        glyphRangesBuilder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
        glyphRangesBuilder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        glyphRangesBuilder.AddRanges(io.Fonts->GetGlyphRangesKorean());
        glyphRangesBuilder.AddRanges(io.Fonts->GetGlyphRangesThai());
        glyphRangesBuilder.AddRanges(io.Fonts->GetGlyphRangesVietnamese());
        glyphRangesBuilder.BuildRanges(&ranges);

        ImWchar fontAwesomeRange[] = {
                ICON_MIN_FA, ICON_MAX_FA,
                0
        };

        ImFontConfig cfg;
        cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
        cfg.SizePixels = 13.0f * this->m_fontScale;

        io.Fonts->AddFontFromFileTTF(path.string().c_str(), std::floor(14.0f * this->m_fontScale), &cfg, ranges.Data); // Needs conversion to char for Windows
        cfg.MergeMode = true;

        io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_compressed_data, font_awesome_compressed_size, 13.0f * this->m_fontScale, &cfg, fontAwesomeRange);

        ImGuiFreeType::BuildFontAtlas(io.Fonts, ImGuiFreeType::Monochrome);
        io.Fonts->GetTexDataAsRGBA32(&px, &w, &h);

        // Create new font atlas
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA8, GL_UNSIGNED_INT, px);
        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(tex));

        return true;
    }

    void Window::frameBegin() {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetWorkPos());
        ImGui::SetNextWindowSize(viewport->GetWorkSize());
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar     | ImGuiWindowFlags_NoDocking
                                     | ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoCollapse
                                     | ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoResize
                                     | ImGuiWindowFlags_NoNavFocus  | ImGuiWindowFlags_NoBringToFrontOnFocus
                                     | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        if (ImGui::Begin("DockSpace", nullptr, windowFlags)) {
            ImGui::PopStyleVar(2);

            ImGui::DockSpace(ImGui::GetID("MainDock"), ImVec2(0.0f, ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() - 1));

            ImGui::Separator();
            for (const auto &callback : ContentRegistry::Interface::getFooterItems()) {
                auto prevIdx = ImGui::GetWindowDrawList()->_VtxCurrentIdx;
                callback();
                auto currIdx = ImGui::GetWindowDrawList()->_VtxCurrentIdx;

                // Only draw separator if something was actually drawn
                if (prevIdx != currIdx) {
                    ImGui::SameLine();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine();
                }
            }

            if (ImGui::BeginMenuBar()) {

                for (const auto& menu : { "hex.menu.file"_lang, "hex.menu.edit"_lang, "hex.menu.view"_lang, "hex.menu.help"_lang })
                    if (ImGui::BeginMenu(menu)) ImGui::EndMenu();

                if (ImGui::BeginMenu("hex.menu.view"_lang)) {
                    for (auto &view : ContentRegistry::Views::getEntries()) {
                        if (view->hasViewMenuItemEntry())
                            ImGui::MenuItem((std::string(view->getName()) + " " + "hex.menu.view"_lang).c_str(), "", &view->getWindowOpenState());
                    }
                    ImGui::EndMenu();
                }

                for (auto &view : ContentRegistry::Views::getEntries()) {
                    view->drawMenu();
                }

                if (ImGui::BeginMenu("hex.menu.view"_lang)) {
                    ImGui::Separator();
                    ImGui::MenuItem("hex.menu.view.fps"_lang, "", &this->m_fpsVisible);
                    #ifdef DEBUG
                        ImGui::MenuItem("hex.menu.view.demo"_lang, "", &this->m_demoWindowOpen);
                    #endif
                    ImGui::EndMenu();
                }

                if (this->m_fpsVisible) {
                    char buffer[0x20];
                    snprintf(buffer, 0x20, "%.1f FPS", ImGui::GetIO().Framerate);

                    ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * strlen(buffer) + 20);
                    ImGui::TextUnformatted(buffer);
                }

                ImGui::EndMenuBar();
            }

            if (auto &[key, mods] = Window::s_currShortcut; key != -1) {
                for (auto &view : ContentRegistry::Views::getEntries()) {
                    if (view->shouldProcess()) {
                        if (view->handleShortcut(key, mods))
                            break;
                    }
                }

                Window::s_currShortcut = { -1, -1 };
            }

            if (SharedData::currentProvider == nullptr) {
                char title[256];
                ImFormatString(title, IM_ARRAYSIZE(title), "%s/DockSpace_%08X", ImGui::GetCurrentWindow()->Name, ImGui::GetID("MainDock"));
                if (ImGui::Begin(title)) {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10 * this->m_globalScale, 10 * this->m_globalScale));
                    if (ImGui::BeginChild("Welcome Screen", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoDecoration)) {
                        this->drawWelcomeScreen();
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                }
                ImGui::End();
            } else if (!this->m_layoutConfigured) {
                this->m_layoutConfigured = true;
                if (ContentRegistry::Settings::read("hex.builtin.setting.imhex", "hex.builtin.setting.imhex.launched", 0) == 0) {
                    ContentRegistry::Settings::write("hex.builtin.setting.imhex", "hex.builtin.setting.imhex.launched", 1);
                    this->resetLayout();
                }
            }

        }
        ImGui::End();
    }

    void Window::frameEnd() {
        ImGui::Render();

        int displayWidth, displayHeight;
        glfwGetFramebufferSize(this->m_window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);

        glfwSwapBuffers(this->m_window);
    }

    void Window::drawWelcomeScreen() {
        ImGui::UnderlinedText("hex.welcome.header.main"_lang, ImGui::GetStyleColorVec4(ImGuiCol_Text));

        ImGui::NewLine();

        const auto availableSpace = ImGui::GetContentRegionAvail();
        const auto rowHeight = ImGui::GetTextLineHeightWithSpacing() * 6;

        ImGui::Indent();
        if (ImGui::BeginTable("Welcome Left", 1, ImGuiTableFlags_NoBordersInBody, ImVec2(availableSpace.x / 2, availableSpace.y))) {
            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("hex.welcome.header.start"_lang);
            {
                if (ImGui::BulletHyperlink("hex.welcome.start.open_file"_lang))
                    EventManager::post(Events::OpenWindow, "Open File");
                if (ImGui::BulletHyperlink("hex.welcome.start.open_project"_lang))
                    EventManager::post(Events::OpenWindow, "Open Project");
            }

            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("hex.welcome.start.recent"_lang);
            {
                if (!this->m_recentFiles.empty()) {
                    for (auto &path : this->m_recentFiles) {
                        if (ImGui::BulletHyperlink(std::filesystem::path(path).filename().string().c_str())) {
                            EventManager::post(Events::FileDropped, path.c_str());
                            break;
                        }
                    }
                }
            }

            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("hex.welcome.header.help"_lang);
            {
                if (ImGui::BulletHyperlink("hex.welcome.help.repo"_lang)) hex::openWebpage("hex.welcome.help.repo.link"_lang);
                if (ImGui::BulletHyperlink("hex.welcome.help.gethelp"_lang)) hex::openWebpage("hex.welcome.help.gethelp.link"_lang);
            }

            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("hex.welcome.header.plugins"_lang);
            {
                const auto &plugins = PluginHandler::getPlugins();

                if (plugins.empty()) {
                    // Intentionally left untranslated so it will be readable even if no plugin with translations is loaded
                    ImGui::TextColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F),
                        "No plugins loaded! To use ImHex properly, "
                             "make sure at least the builtin plugin is in the /plugins folder next to the executable");
                } else {
                    if (ImGui::BeginTable("plugins", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit, ImVec2((ImGui::GetContentRegionAvail().x * 5) / 6, ImGui::GetTextLineHeightWithSpacing() * 4))) {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableSetupColumn("hex.welcome.plugins.plugin"_lang);
                        ImGui::TableSetupColumn("hex.welcome.plugins.author"_lang);
                        ImGui::TableSetupColumn("hex.welcome.plugins.desc"_lang);

                        ImGui::TableHeadersRow();

                        ImGuiListClipper clipper;
                        clipper.Begin(plugins.size());

                        while (clipper.Step()) {
                            for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted((plugins[i].getPluginName() + "   ").c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted((plugins[i].getPluginAuthor() + "   ").c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(plugins[i].getPluginDescription().c_str());
                            }
                        }

                        clipper.End();

                        ImGui::EndTable();
                    }
                }
            }

            ImGui::EndTable();
        }
        ImGui::SameLine();
        if (ImGui::BeginTable("Welcome Right", 1, ImGuiTableFlags_NoBordersInBody, ImVec2(availableSpace.x / 2, availableSpace.y))) {
            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("hex.welcome.header.customize"_lang);
            {
                if (ImGui::DescriptionButton("hex.welcome.customize.settings.title"_lang, "hex.welcome.customize.settings.desc"_lang, ImVec2(ImGui::GetContentRegionAvail().x * 0.8F, 0)))
                    EventManager::post(Events::OpenWindow, "hex.view.settings.title");
            }
            ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("hex.welcome.header.learn"_lang);
            {
                if (ImGui::DescriptionButton("hex.welcome.learn.latest.title"_lang, "hex.welcome.learn.latest.desc"_lang, ImVec2(ImGui::GetContentRegionAvail().x * 0.8F, 0)))
                    hex::openWebpage("hex.welcome.learn.latest.link"_lang);
                if (ImGui::DescriptionButton("hex.welcome.learn.pattern.title"_lang, "hex.welcome.learn.pattern.desc"_lang, ImVec2(ImGui::GetContentRegionAvail().x * 0.8F, 0)))
                    hex::openWebpage("hex.welcome.learn.pattern.link"_lang);
                if (ImGui::DescriptionButton("hex.welcome.learn.plugins.title"_lang, "hex.welcome.learn.plugins.desc"_lang, ImVec2(ImGui::GetContentRegionAvail().x * 0.8F, 0)))
                    hex::openWebpage("hex.welcome.learn.plugins.link"_lang);
            }

            auto extraWelcomeScreenEntries = ContentRegistry::Interface::getWelcomeScreenEntries();
            if (!extraWelcomeScreenEntries.empty()) {
                ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("hex.welcome.header.various"_lang);
                {
                    for (const auto &callback : extraWelcomeScreenEntries)
                        callback();
                }
            }


            ImGui::EndTable();
        }
    }

    void Window::resetLayout() {
        auto dockId = ImGui::GetID("MainDock");

        ImGui::DockBuilderRemoveNode(dockId);
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockId, ImGui::GetWindowSize());

        ImGuiID mainWindowId, splitWindowId, hexEditorId, utilitiesId, inspectorId, patternDataId;

        ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Left, 0.8, &mainWindowId, &utilitiesId);
        ImGui::DockBuilderSplitNode(mainWindowId, ImGuiDir_Down, 0.3, &patternDataId, &splitWindowId);
        ImGui::DockBuilderSplitNode(splitWindowId, ImGuiDir_Right, 0.3, &inspectorId, &hexEditorId);

        for (auto &view : ContentRegistry::Views::getEntries())

            ImGui::DockBuilderDockWindow(view->getName().data(), utilitiesId);
        ImGui::DockBuilderDockWindow("hex.view.hexeditor.name"_lang, hexEditorId);
        ImGui::DockBuilderDockWindow("hex.view.data_inspector.name"_lang, inspectorId);
        ImGui::DockBuilderDockWindow("hex.view.pattern_data.name"_lang, patternDataId);

        ImGui::DockBuilderFinish(dockId);
    }

     void Window::initGLFW() {
        glfwSetErrorCallback([](int error, const char* desc) {
            fprintf(stderr, "Glfw Error %d: %s\n", error, desc);
        });

        if (!glfwInit())
            throw std::runtime_error("Failed to initialize GLFW!");

        #ifdef __APPLE__
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        #endif

        if (auto *monitor = glfwGetPrimaryMonitor(); monitor) {
            float xscale, yscale;
            glfwGetMonitorContentScale(monitor, &xscale, &yscale);

            // In case the horizontal and vertical scale are different, fall back on the average
            this->m_globalScale = this->m_fontScale = std::midpoint(xscale, yscale);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


        this->m_window = glfwCreateWindow(1280 * this->m_globalScale, 720 * this->m_globalScale, "ImHex", nullptr, nullptr);


        if (this->m_window == nullptr)
            throw std::runtime_error("Failed to create window!");

        glfwMakeContextCurrent(this->m_window);
        glfwSwapInterval(1);

         {
             int x = 0, y = 0;
             glfwGetWindowPos(this->m_window, &x, &y);
             SharedData::windowPos = ImVec2(x, y);
         }

         {
             int width = 0, height = 0;
             glfwGetWindowSize(this->m_window, &width, &height);
             SharedData::windowSize = ImVec2(width, height);
         }

         glfwSetWindowPosCallback(this->m_window, [](GLFWwindow *window, int x, int y) {
             SharedData::windowPos = ImVec2(x, y);
         });

        glfwSetWindowSizeCallback(this->m_window, [](GLFWwindow *window, int width, int height) {
            SharedData::windowSize = ImVec2(width, height);
        });

        glfwSetKeyCallback(this->m_window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS) {
                Window::s_currShortcut = { key, mods };
                auto &io = ImGui::GetIO();
                io.KeysDown[key] = true;
                io.KeyCtrl  = (mods & GLFW_MOD_CONTROL) != 0;
                io.KeyShift = (mods & GLFW_MOD_SHIFT) != 0;
                io.KeyAlt   = (mods & GLFW_MOD_ALT) != 0;
            }
            else if (action == GLFW_RELEASE) {
                auto &io = ImGui::GetIO();
                io.KeysDown[key] = false;
                io.KeyCtrl  = (mods & GLFW_MOD_CONTROL) != 0;
                io.KeyShift = (mods & GLFW_MOD_SHIFT) != 0;
                io.KeyAlt   = (mods & GLFW_MOD_ALT) != 0;
            }
        });

        glfwSetDropCallback(this->m_window, [](GLFWwindow *window, int count, const char **paths) {
            if (count != 1)
                return;

            View::postEvent(Events::FileDropped, paths[0]);
        });

        glfwSetWindowCloseCallback(this->m_window, [](GLFWwindow *window) {
            View::postEvent(Events::WindowClosing, window);
        });


        glfwSetWindowSizeLimits(this->m_window, 720, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);

        if (gladLoadGL() == 0)
            throw std::runtime_error("Failed to initialize OpenGL loader!");
    }

    void Window::initImGui() {
        IMGUI_CHECKVERSION();

        GImGui = ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        ImGuiStyle& style = ImGui::GetStyle();

        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
        #if !defined(OS_LINUX)
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        #endif


        io.ConfigViewportsNoTaskBarIcon = false;
        io.KeyMap[ImGuiKey_Tab]         = GLFW_KEY_TAB;
        io.KeyMap[ImGuiKey_LeftArrow]   = GLFW_KEY_LEFT;
        io.KeyMap[ImGuiKey_RightArrow]  = GLFW_KEY_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow]     = GLFW_KEY_UP;
        io.KeyMap[ImGuiKey_DownArrow]   = GLFW_KEY_DOWN;
        io.KeyMap[ImGuiKey_PageUp]      = GLFW_KEY_PAGE_UP;
        io.KeyMap[ImGuiKey_PageDown]    = GLFW_KEY_PAGE_DOWN;
        io.KeyMap[ImGuiKey_Home]        = GLFW_KEY_HOME;
        io.KeyMap[ImGuiKey_End]         = GLFW_KEY_END;
        io.KeyMap[ImGuiKey_Insert]      = GLFW_KEY_INSERT;
        io.KeyMap[ImGuiKey_Delete]      = GLFW_KEY_DELETE;
        io.KeyMap[ImGuiKey_Backspace]   = GLFW_KEY_BACKSPACE;
        io.KeyMap[ImGuiKey_Space]       = GLFW_KEY_SPACE;
        io.KeyMap[ImGuiKey_Enter]       = GLFW_KEY_ENTER;
        io.KeyMap[ImGuiKey_Escape]      = GLFW_KEY_ESCAPE;
        io.KeyMap[ImGuiKey_KeyPadEnter] = GLFW_KEY_KP_ENTER;
        io.KeyMap[ImGuiKey_A]           = GLFW_KEY_A;
        io.KeyMap[ImGuiKey_C]           = GLFW_KEY_C;
        io.KeyMap[ImGuiKey_V]           = GLFW_KEY_V;
        io.KeyMap[ImGuiKey_X]           = GLFW_KEY_X;
        io.KeyMap[ImGuiKey_Y]           = GLFW_KEY_Y;
        io.KeyMap[ImGuiKey_Z]           = GLFW_KEY_Z;

        io.UserData = new ImGui::ImHexCustomData();

        if (this->m_globalScale != 0.0f)
            style.ScaleAllSizes(this->m_globalScale);

        #if defined(OS_WINDOWS)
            std::filesystem::path resourcePath = std::filesystem::path((SharedData::mainArgv)[0]).parent_path();
        #elif defined(OS_LINUX) || defined(OS_MACOS)
            std::filesystem::path resourcePath = "/usr/share/ImHex";
        #else
            std::filesystem::path resourcePath = "";
            #warning "Unsupported OS for custom font support"
        #endif

        if (!resourcePath.empty() && this->setFont(resourcePath / "font.ttf")) {

        }
        else {
            io.Fonts->Clear();

            ImFontConfig cfg;
            cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
            cfg.SizePixels = 13.0f * this->m_fontScale;
            io.Fonts->AddFontDefault(&cfg);

            cfg.MergeMode = true;

            ImWchar fontAwesomeRange[] = {
                    ICON_MIN_FA, ICON_MAX_FA,
                    0
            };
            std::uint8_t *px;
            int w, h;
            io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_compressed_data, font_awesome_compressed_size, 13.0f * this->m_fontScale, &cfg, fontAwesomeRange);
            io.Fonts->GetTexDataAsRGBA32(&px, &w, &h);

            // Create new font atlas
            GLuint tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA8, GL_UNSIGNED_INT, px);
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
        handler.UserData   = this;
        ImGui::GetCurrentContext()->SettingsHandlers.push_back(handler);

        ImGui_ImplGlfw_InitForOpenGL(this->m_window, true);
        ImGui_ImplOpenGL3_Init("#version 150");
    }

    void Window::initPlugins() {
        try {
            auto pluginFolderPath = std::filesystem::path((SharedData::mainArgv)[0]).parent_path() / "plugins";
            PluginHandler::load(pluginFolderPath.string());
        } catch (std::runtime_error &e) { return; }

        for (const auto &plugin : PluginHandler::getPlugins()) {
            plugin.initializePlugin();
        }
    }

    void Window::deinitGLFW() {
        glfwDestroyWindow(this->m_window);
        glfwTerminate();
    }

    void Window::deinitImGui() {
        delete static_cast<ImGui::ImHexCustomData*>(ImGui::GetIO().UserData);

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void Window::deinitPlugins() {
        PluginHandler::unload();
    }

}