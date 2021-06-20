#include "window.hpp"

#include <hex.hpp>
#include <hex/api/content_registry.hpp>

#include <chrono>
#include <iostream>
#include <numeric>
#include <typeinfo>
#include <thread>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_freetype.h>
#include <imgui_imhex_extensions.h>
#include <implot.h>
#include <implot_internal.h>

#include <fontawesome_font.h>

#include "helpers/plugin_manager.hpp"
#include "init/tasks.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace hex {

    using namespace std::literals::chrono_literals;

    void *ImHexSettingsHandler_ReadOpenFn(ImGuiContext *ctx, ImGuiSettingsHandler *, const char *) {
        return ctx; // Unused, but the return value has to be non-null
    }

    void ImHexSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler *handler, void *, const char* line) {
        for (auto &view : ContentRegistry::Views::getEntries()) {
            std::string format = std::string(view->getUnlocalizedName()) + "=%d";
            sscanf(line, format.c_str(), &view->getWindowOpenState());
        }
    }

    void ImHexSettingsHandler_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf) {
        buf->reserve(buf->size() + 0x20); // Ballpark reserve

        buf->appendf("[%s][General]\n", handler->TypeName);

        for (auto &view : ContentRegistry::Views::getEntries()) {
            buf->appendf("%s=%d\n", view->getUnlocalizedName().data(), view->getWindowOpenState());
        }

        buf->append("\n");
    }

    Window::Window() {
        SharedData::currentProvider = nullptr;

        {
            for (const auto &[argument, value] : init::getInitArguments()) {
                if (argument == "update-available") {
                    this->m_availableUpdate = value;
                } else if (argument == "no-plugins") {
                    View::doLater([]{ ImGui::OpenPopup("No Plugins"); });
                }
            }
        }

        this->initGLFW();
        this->initImGui();

        EventManager::subscribe<EventSettingsChanged>(this, [this]() {
            {
                auto theme = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.color");

                if (theme.is_number()) {
                    switch (static_cast<int>(theme)) {
                        default:
                        case 0: /* Dark theme */
                            ImGui::StyleColorsDark();
                            ImGui::StyleCustomColorsDark();
                            ImPlot::StyleColorsDark();
                            break;
                        case 1: /* Light theme */
                            ImGui::StyleColorsLight();
                            ImGui::StyleCustomColorsLight();
                            ImPlot::StyleColorsLight();
                            break;
                        case 2: /* Classic theme */
                            ImGui::StyleColorsClassic();
                            ImGui::StyleCustomColorsClassic();
                            ImPlot::StyleColorsClassic();
                            break;
                    }
                    ImGui::GetStyle().Colors[ImGuiCol_DockingEmptyBg] = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
                }
            }

            {
                auto language = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.language");

                if (language.is_string()) {
                    LangEntry::loadLanguage(static_cast<std::string>(language));
                } else {
                    // If no language is specified, fall back to English.
                    LangEntry::loadLanguage("en-US");
                }
            }

            {
                auto targetFps = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.fps");

                if (targetFps.is_number())
                    this->m_targetFps = targetFps;
            }

            {
                if (ContentRegistry::Settings::read("hex.builtin.setting.imhex", "hex.builtin.setting.imhex.launched", 0) == 1)
                    this->m_layoutConfigured = true;
                else
                    ContentRegistry::Settings::write("hex.builtin.setting.imhex", "hex.builtin.setting.imhex.launched", 1);
            }
        });

        EventManager::subscribe<EventFileLoaded>(this, [this](const std::string &path){
            SharedData::recentFilePaths.push_front(path);

            {
                std::list<std::string> uniques;
                for (auto &file : SharedData::recentFilePaths) {

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
                SharedData::recentFilePaths = uniques;
            }

            {
                std::vector<std::string> recentFilesVector;
                std::copy(SharedData::recentFilePaths.begin(), SharedData::recentFilePaths.end(), std::back_inserter(recentFilesVector));

                ContentRegistry::Settings::write("hex.builtin.setting.imhex", "hex.builtin.setting.imhex.recent_files", recentFilesVector);
            }
        });

        EventManager::subscribe<RequestCloseImHex>(this, [this]() {
            glfwSetWindowShouldClose(this->m_window, true);
        });

        EventManager::subscribe<RequestChangeWindowTitle>(this, [this](std::string windowTitle) {
            if (windowTitle.empty())
                glfwSetWindowTitle(this->m_window, "ImHex");
            else
                glfwSetWindowTitle(this->m_window, ("ImHex - " + windowTitle).c_str());
        });

        EventManager::post<EventSettingsChanged>();

        for (const auto &path : ContentRegistry::Settings::read("hex.builtin.setting.imhex", "hex.builtin.setting.imhex.recent_files"))
            SharedData::recentFilePaths.push_back(path);
    }

    Window::~Window() {
        delete SharedData::currentProvider;

        this->deinitImGui();
        this->deinitGLFW();

        EventManager::unsubscribe<EventSettingsChanged>(this);
        EventManager::unsubscribe<EventFileLoaded>(this);
        EventManager::unsubscribe<RequestCloseImHex>(this);
        EventManager::unsubscribe<RequestChangeWindowTitle>(this);
    }

    void Window::loop() {
        this->m_lastFrameTime = glfwGetTime();
        while (!glfwWindowShouldClose(this->m_window)) {
            if (!glfwGetWindowAttrib(this->m_window, GLFW_VISIBLE) || glfwGetWindowAttrib(this->m_window, GLFW_ICONIFIED))
                glfwWaitEvents();

            glfwPollEvents();

            this->frameBegin();
            this->frame();
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
                            ImGui::MenuItem((LangEntry(view->getUnlocalizedName()) + " " + "hex.menu.view"_lang).c_str(), "", &view->getWindowOpenState());
                    }
                    ImGui::EndMenu();
                }

                for (auto &view : ContentRegistry::Views::getEntries()) {
                    view->drawMenu();
                }

                if (ImGui::BeginMenu("hex.menu.view"_lang)) {
                    #if defined(DEBUG)
                        ImGui::Separator();
                        ImGui::MenuItem("hex.menu.view.demo"_lang, "", &this->m_demoWindowOpen);
                    #endif
                    ImGui::EndMenu();
                }

                #if defined(DEBUG)
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 2 * ImGui::GetFontSize());
                    ImGui::TextUnformatted(ICON_FA_BUG);
                #endif

                ImGui::EndMenuBar();
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
                this->resetLayout();
            }

        }
        ImGui::End();

        // Popup for when no plugins were loaded. Intentionally left untranslated because localization isn't available
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        if (ImGui::BeginPopupModal("No Plugins", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::TextUnformatted("No ImHex plugins loaded (including the built-in plugin)!");
            ImGui::TextUnformatted("Make sure you at least got the builtin plugin in your plugins folder.");
            ImGui::TextUnformatted("To find out where your plugin folder is, check ImHex' Readme.");
            ImGui::EndPopup();
        }
    }

    void Window::frame() {
        bool pressedKeys[512] = { false };

        std::copy_n(ImGui::GetIO().KeysDown, 512, this->m_prevKeysDown);
        for (u16 i = 0; i < 512; i++)
            pressedKeys[i] = ImGui::GetIO().KeysDown[i] && !this->m_prevKeysDown[i];

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
            view->handleShortcut(pressedKeys, ImGui::GetIO().KeyCtrl, ImGui::GetIO().KeyShift, ImGui::GetIO().KeyAlt);
        }

        View::drawCommonInterfaces();

#ifdef DEBUG
        if (this->m_demoWindowOpen) {
            ImGui::ShowDemoWindow(&this->m_demoWindowOpen);
            ImPlot::ShowDemoWindow(&this->m_demoWindowOpen);
        }
#endif
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

        std::this_thread::sleep_for(std::chrono::milliseconds(u64((this->m_lastFrameTime + 1 / (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) ? this->m_targetFps : 5.0) - glfwGetTime()) * 1000)));
        this->m_lastFrameTime = glfwGetTime();
    }

    void Window::drawWelcomeScreen() {
        ImGui::UnderlinedText("hex.welcome.header.main"_lang, ImGui::GetStyleColorVec4(ImGuiCol_Text));

        ImGui::NewLine();

        const auto availableSpace = ImGui::GetContentRegionAvail();

        ImGui::Indent();
        if (ImGui::BeginTable("Welcome Left", 1, ImGuiTableFlags_NoBordersInBody, ImVec2(availableSpace.x / 2, availableSpace.y))) {
            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("hex.welcome.header.start"_lang);
            {
                if (ImGui::BulletHyperlink("hex.welcome.start.open_file"_lang))
                    EventManager::post<RequestOpenWindow>("Open File");
                if (ImGui::BulletHyperlink("hex.welcome.start.open_project"_lang))
                    EventManager::post<RequestOpenWindow>("Open Project");
            }

            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 9);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("hex.welcome.start.recent"_lang);
            {
                if (!SharedData::recentFilePaths.empty()) {
                    for (auto &path : SharedData::recentFilePaths) {
                        if (ImGui::BulletHyperlink(std::filesystem::path(path).filename().string().c_str())) {
                            EventManager::post<EventFileDropped>(path);
                            break;
                        }
                    }
                }
            }

            if (!this->m_availableUpdate.empty()) {
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("hex.welcome.header.update"_lang);
                {
                    if (ImGui::DescriptionButton("hex.welcome.update.title"_lang, hex::format("hex.welcome.update.desc"_lang, this->m_availableUpdate).c_str(), ImVec2(ImGui::GetContentRegionAvail().x * 0.8F, 0)))
                        hex::openWebpage("hex.welcome.update.link"_lang);
                }
            }

            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("hex.welcome.header.help"_lang);
            {
                if (ImGui::BulletHyperlink("hex.welcome.help.repo"_lang)) hex::openWebpage("hex.welcome.help.repo.link"_lang);
                if (ImGui::BulletHyperlink("hex.welcome.help.gethelp"_lang)) hex::openWebpage("hex.welcome.help.gethelp.link"_lang);
            }

            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("hex.welcome.header.plugins"_lang);
            {
                const auto &plugins = PluginManager::getPlugins();

                if (!plugins.empty()) {
                    if (ImGui::BeginTable("plugins", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit, ImVec2((ImGui::GetContentRegionAvail().x * 5) / 6, ImGui::GetTextLineHeightWithSpacing() * 5))) {
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
            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("hex.welcome.header.customize"_lang);
            {
                if (ImGui::DescriptionButton("hex.welcome.customize.settings.title"_lang, "hex.welcome.customize.settings.desc"_lang, ImVec2(ImGui::GetContentRegionAvail().x * 0.8F, 0)))
                    EventManager::post<RequestOpenWindow>("Settings");
            }
            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
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
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing() * 5);
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
            ImGui::DockBuilderDockWindow(view->getUnlocalizedName().data(), utilitiesId);

        ImGui::DockBuilderDockWindow("hex.view.hexeditor.name", hexEditorId);
        ImGui::DockBuilderDockWindow("hex.view.data_inspector.name", inspectorId);
        ImGui::DockBuilderDockWindow("hex.view.pattern_data.name", patternDataId);

        ImGui::DockBuilderFinish(dockId);
    }

    void Window::initGLFW() {
        glfwSetErrorCallback([](int error, const char* desc) {
            log::error("GLFW Error [{}] : {}", error, desc);
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
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

        this->m_window = glfwCreateWindow(1280 * this->m_globalScale, 720 * this->m_globalScale, "ImHex", nullptr, nullptr);

        glfwSetWindowUserPointer(this->m_window, this);

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

             auto win = static_cast<Window*>(glfwGetWindowUserPointer(window));
             win->frameBegin();
             win->frame();
             win->frameEnd();
         });

        glfwSetWindowSizeCallback(this->m_window, [](GLFWwindow *window, int width, int height) {
            SharedData::windowSize = ImVec2(width, height);

            auto win = static_cast<Window*>(glfwGetWindowUserPointer(window));
            win->frameBegin();
            win->frame();
            win->frameEnd();
        });

        glfwSetKeyCallback(this->m_window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {

            auto keyName = glfwGetKeyName(key, scancode);
            if (keyName != nullptr)
                key = std::toupper(keyName[0]);

            if (action == GLFW_PRESS) {
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

            EventManager::post<EventFileDropped>(paths[0]);
        });

        glfwSetWindowCloseCallback(this->m_window, [](GLFWwindow *window) {
            EventManager::post<EventWindowClosing>(window);
        });


        glfwSetWindowSizeLimits(this->m_window, 720, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);

        if (gladLoadGL() == 0)
            throw std::runtime_error("Failed to initialize OpenGL loader!");
    }

    void Window::initImGui() {
        IMGUI_CHECKVERSION();

        GImGui = ImGui::CreateContext();
        GImPlot = ImPlot::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        ImGuiStyle& style = ImGui::GetStyle();

        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
        #if !defined(OS_LINUX)
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        #endif


        io.ConfigViewportsNoTaskBarIcon = true;
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

        std::string fontFile;
        for (const auto &dir : hex::getPath(ImHexPath::Resources)) {
            fontFile = dir + "/font.ttf";
            if (std::filesystem::exists(fontFile))
                break;
        }

        if (this->setFont(fontFile)) {

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
            io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_compressed_data, font_awesome_compressed_size, 11.0f * this->m_fontScale, &cfg, fontAwesomeRange);
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

        static std::string iniFileName;
        for (const auto &dir : hex::getPath(ImHexPath::Config)) {
            if (std::filesystem::exists(dir)) {
                iniFileName = dir + "/interface.ini";
                break;
            }
        }
        io.IniFilename = iniFileName.c_str();

        ImGui_ImplGlfw_InitForOpenGL(this->m_window, true);

        ImGui_ImplOpenGL3_Init("#version 150");
    }

    void Window::deinitGLFW() {
        glfwDestroyWindow(this->m_window);
        glfwTerminate();
    }

    void Window::deinitImGui() {
        delete static_cast<ImGui::ImHexCustomData*>(ImGui::GetIO().UserData);

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

}
