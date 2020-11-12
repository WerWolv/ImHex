#include "window.hpp"

#include <iostream>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace hex {

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
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

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