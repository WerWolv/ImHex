#include "splash_window.hpp"

#include <hex/helpers/utils.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_imhex_extensions.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <unistd.h>

#include <future>
#include <chrono>

using namespace std::literals::chrono_literals;

namespace hex::pre {

    static void centerWindow(GLFWwindow *window) {
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        if (!monitor)
            return;

        const GLFWvidmode *mode = glfwGetVideoMode(monitor);
        if (!mode)
            return;

        int monitorX, monitorY;
        glfwGetMonitorPos(monitor, &monitorX, &monitorY);

        int windowWidth, windowHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);

        glfwSetWindowPos(window, monitorX + (mode->width - windowWidth) / 2, monitorY + (mode->height - windowHeight) / 2);
    }


    WindowSplash::WindowSplash() {
        this->initGLFW();
        this->initImGui();
    }

    WindowSplash::~WindowSplash() {
        this->deinitImGui();
        this->deinitGLFW();
    }


    std::future<bool> WindowSplash::processTasksAsync() {
        return std::async(std::launch::async, [this] {
            bool status = true;

            for (const auto &[name, task] : this->m_tasks) {
                {
                    std::lock_guard guard(this->m_progressMutex);
                    this->m_currTaskName = name;
                }

                status = status && task();

                {
                    std::lock_guard guard(this->m_progressMutex);
                    this->m_progress += 1.0F / m_tasks.size();
                }
            }

            // Small extra delay so the last progress step is visible
            std::this_thread::sleep_for(200ms);

            return status;
        });
    }

    bool WindowSplash::loop() {
        auto [splashTexture, splashWidth, splashHeight] = ImGui::LoadImageFromPath((hex::getPath(hex::ImHexPath::Resources)[0] + "/splash.png").c_str());
        ON_SCOPE_EXIT { ImGui::UnloadImage(splashTexture); };

        if (splashTexture == nullptr)
            exit(EXIT_FAILURE);

        auto done = processTasksAsync();

        while (!glfwWindowShouldClose(this->m_window)) {
            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            {
                std::lock_guard guard(this->m_progressMutex);

                auto drawList = ImGui::GetOverlayDrawList();

                drawList->AddImage(splashTexture, ImVec2(0, 0), ImVec2(splashWidth, splashHeight));
                drawList->AddText(ImVec2(15, 120), ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("WerWolv 2020 - {0}\n{1} : {2}@{3}", &__DATE__[7], IMHEX_VERSION, GIT_BRANCH, GIT_COMMIT_HASH).c_str());
                drawList->AddRectFilled(ImVec2(0, splashHeight - 5), ImVec2(splashWidth * this->m_progress, splashHeight), 0xFFFFFFFF);
                drawList->AddText(ImVec2(15, splashHeight - 22), ImColor(0xFF, 0xFF, 0xFF, 0xFF), this->m_currTaskName.data());
            }

            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(this->m_window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(this->m_window);

            if (done.wait_for(0s) == std::future_status::ready) {
                return done.get();
            }
        }

        return false;
    }

    void WindowSplash::initGLFW() {
        glfwSetErrorCallback([](int error, const char *description) {
            printf("GLFW Error: %d - %s\n", error, description);
            exit(EXIT_FAILURE);
        });

        if (!glfwInit())
            exit(EXIT_FAILURE);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

        this->m_window = glfwCreateWindow(640, 400, "ImHex", nullptr, nullptr);
        if (this->m_window == nullptr)
            exit(EXIT_FAILURE);

        centerWindow(this->m_window);

        glfwMakeContextCurrent(this->m_window);
        glfwSwapInterval(1);

        if (gladLoadGL() == 0)
            exit(EXIT_FAILURE);
    }

    void WindowSplash::initImGui() {
        IMGUI_CHECKVERSION();
        GImGui = ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(this->m_window, true);
        ImGui_ImplOpenGL3_Init("#version 130");
    }

    void WindowSplash::deinitGLFW() {
        glfwDestroyWindow(this->m_window);
        glfwTerminate();
    }

    void WindowSplash::deinitImGui() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

}