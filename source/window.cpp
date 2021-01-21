#include "window.hpp"

#include <hex.hpp>
#include <hex/api/content_registry.hpp>

#include <iostream>
#include <numeric>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_freetype.h>

#include "helpers/plugin_handler.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace hex {

    constexpr auto MenuBarItems = { "File", "Edit", "View", "Help" };

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
            buf->appendf("%s=%d\n", view->getName().data(), view->getWindowOpenState());
        }

        buf->append("\n");
    }

    Window::Window(int &argc, char **&argv) {
        hex::SharedData::mainArgc = argc;
        hex::SharedData::mainArgv = argv;

        ContentRegistry::Settings::load();
        View::postEvent(Events::SettingsChanged);

        this->initGLFW();
        this->initImGui();
    }

    Window::~Window() {
        this->deinitImGui();
        this->deinitGLFW();
        ContentRegistry::Settings::store();

        for (auto &view : ContentRegistry::Views::getEntries())
            delete view;
        ContentRegistry::Views::getEntries().clear();

        this->deinitPlugins();
    }

    void Window::loop() {
        while (!glfwWindowShouldClose(this->m_window)) {
            this->frameBegin();

            for (const auto &call : View::getDeferedCalls())
                call();
            View::getDeferedCalls().clear();

            for (auto &view : ContentRegistry::Views::getEntries()) {
                if (!view->getWindowOpenState())
                    continue;

                ImGui::SetNextWindowSizeConstraints(view->getMinSize(), view->getMaxSize());
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
        io.Fonts->AddFontFromFileTTF(path.string().c_str(), std::floor(14.0f * this->m_fontScale)); // Needs conversion to char for Windows
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
                                     | ImGuiWindowFlags_NoNavFocus  | ImGuiWindowFlags_NoBringToFrontOnFocus;

        if (ImGui::Begin("DockSpace", nullptr, windowFlags)) {
            ImGui::PopStyleVar(2);
            ImGui::DockSpace(ImGui::GetID("MainDock"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

            if (ImGui::BeginMenuBar()) {

                for (auto menu : MenuBarItems)
                    if (ImGui::BeginMenu(menu)) ImGui::EndMenu();

                if (ImGui::BeginMenu("View")) {
                    for (auto &view : ContentRegistry::Views::getEntries()) {
                        if (view->hasViewMenuItemEntry())
                            ImGui::MenuItem((std::string(view->getName()) + " View").c_str(), "", &view->getWindowOpenState());
                    }
                    ImGui::EndMenu();
                }

                for (auto &view : ContentRegistry::Views::getEntries()) {
                    view->drawMenu();
                }

                if (ImGui::BeginMenu("View")) {
                    ImGui::Separator();
                    ImGui::MenuItem("Display FPS", "", &this->m_fpsVisible);
                    #ifdef DEBUG
                        ImGui::MenuItem("Demo View", "", &this->m_demoWindowOpen);
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
                    if (view->getWindowOpenState()) {
                        if (view->handleShortcut(key, mods))
                            break;
                    }
                }

                Window::s_currShortcut = { -1, -1 };
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
            if (action == GLFW_PRESS)
                Window::s_currShortcut = { key, mods };
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
        auto *ctx = ImGui::CreateContext();
        GImGui = ctx;

        ImGuiIO& io = ImGui::GetIO();
        ImGuiStyle& style = ImGui::GetStyle();

        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
        io.ConfigViewportsNoTaskBarIcon = true;

        if (this->m_globalScale != 0.0f)
            style.ScaleAllSizes(this->m_globalScale);

#ifdef __MINGW32__
        std::filesystem::path resourcePath = std::filesystem::path((SharedData::mainArgv)[0]).parent_path();
#elif defined(__linux__)
        std::filesystem::path resourcePath = "/usr/share/ImHex";
#else
        std::filesystem::path resourcePath = "";
#   warning "Unsupported OS for custom font support"
#endif

        if (!resourcePath.empty() && this->setFont(resourcePath / "font.ttf"))
            ;
        else if ((this->m_fontScale != 0.0f) && (this->m_fontScale != 1.0f)) {
            io.Fonts->Clear();

            ImFontConfig cfg;
            cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
            cfg.SizePixels = 13.0f * this->m_fontScale;
            io.Fonts->AddFontDefault(&cfg);
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
        ctx->SettingsHandlers.push_back(handler);

        ImGui::StyleColorsDark();

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
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void Window::deinitPlugins() {
        PluginHandler::unload();
    }

}