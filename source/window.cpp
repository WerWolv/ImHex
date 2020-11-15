#include "window.hpp"

#include <iostream>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace hex {

    namespace {

        void *ImHexSettingsHandler_ReadOpenFn(ImGuiContext *ctx, ImGuiSettingsHandler *, const char *) {
            return ctx; // Unused, but the return value has to be non-null
        }

        void ImHexSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler *handler, void *, const char* line) {
            auto *window = reinterpret_cast<Window *>(handler->UserData);

            float scale;
            if (sscanf(line, "Scale=%f", &scale) == 1)            { window->m_globalScale = scale; }
            else if (sscanf(line, "FontScale=%f", &scale) == 1)   { window->m_fontScale   = scale; }
        }

        void ImHexSettingsHandler_ApplyAll(ImGuiContext *ctx, ImGuiSettingsHandler *handler) {
            auto *window = reinterpret_cast<Window *>(handler->UserData);
            auto &style  = ImGui::GetStyle();
            auto &io     = ImGui::GetIO();

            if (window->m_globalScale != 0.0f)
                style.ScaleAllSizes(window->m_globalScale);
            if (window->m_fontScale != 0.0f)
                io.FontGlobalScale = window->m_fontScale;
        }

        void ImHexSettingsHandler_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf) {
            auto *window = reinterpret_cast<Window *>(handler->UserData);

            buf->reserve(buf->size() + 0x20); // Ballpark reserve

            buf->appendf("[%s][General]\n", handler->TypeName);
            buf->appendf("Scale=%.1f\n", window->m_globalScale);
            buf->appendf("FontScale=%.1f\n", window->m_fontScale);
            buf->append("\n");
        }

    }

    Window::Window() {
        this->initGLFW();
        this->initImGui();
    }

    Window::~Window() {
        this->deinitImGui();
        this->deinitGLFW();

        for (auto &view : this->m_views)
            delete view;
    }

    void Window::loop() {
        while (!glfwWindowShouldClose(this->m_window)) {
            this->frameBegin();

            for (const auto &call : View::getDeferedCalls())
                call();
            View::getDeferedCalls().clear();

            for (auto &view : this->m_views) {
                view->createView();
            }

            this->frameEnd();
        }
    }


    void Window::frameBegin() {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetWorkPos());
        ImGui::SetNextWindowSize(viewport->GetWorkSize());
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::Begin("DockSpace", nullptr, windowFlags);
        ImGui::PopStyleVar(2);
        ImGui::DockSpace(ImGui::GetID("MainDock"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        ImGui::BeginMenuBar();

        for (auto &view : this->m_views)
            view->createMenu();

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Display FPS", "", &this->m_fpsVisible);
            ImGui::EndMenu();
        }

        if (this->m_fpsVisible) {
            char buffer[0x20];
            snprintf(buffer, 0x20, "%.1f FPS", ImGui::GetIO().Framerate);

            ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * strlen(buffer) + 20);
            ImGui::TextUnformatted(buffer);
        }


        ImGui::EndMenuBar();

        ImGui::End();

        if (auto &[key, mods] = Window::s_currShortcut; key != -1) {
            for (auto &view : this->m_views) {
                if (view->handleShortcut(key, mods))
                    break;
            }

            Window::s_currShortcut = { -1, -1 };
        }

    }

    void Window::frameEnd() {
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(this->m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        this->m_window = glfwCreateWindow(1280, 720, "ImHex", nullptr, nullptr);

        if (this->m_window == nullptr)
            throw std::runtime_error("Failed to create window!");

        glfwMakeContextCurrent(this->m_window);
        glfwSwapInterval(1);

        glfwSetKeyCallback(this->m_window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS)
                Window::s_currShortcut = { key, mods };
        });

         glfwSetWindowSizeLimits(this->m_window, 720, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);

        if (gladLoadGL() == 0)
            throw std::runtime_error("Failed to initialize OpenGL loader!");
    }

    void Window::initImGui() {
        IMGUI_CHECKVERSION();
        auto *ctx = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // Install custom settings handler
        ImGuiSettingsHandler handler;
        handler.TypeName = "ImHex";
        handler.TypeHash = ImHashStr("ImHex");
        handler.ReadOpenFn = ImHexSettingsHandler_ReadOpenFn;
        handler.ReadLineFn = ImHexSettingsHandler_ReadLine;
        handler.ApplyAllFn = ImHexSettingsHandler_ApplyAll;
        handler.WriteAllFn = ImHexSettingsHandler_WriteAll;
        handler.UserData   = this;
        ctx->SettingsHandlers.push_back(handler);

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(this->m_window, true);
        ImGui_ImplOpenGL3_Init("#version 150");
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

}