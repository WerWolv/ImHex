#include "init/splash_window.hpp"

#include <hex/helpers/utils.hpp>
#include <hex/helpers/shared_data.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_imhex_extensions.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <fontawesome_font.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <unistd.h>

#include <future>
#include <chrono>

using namespace std::literals::chrono_literals;

namespace hex::init {

    WindowSplash::WindowSplash(int &argc, char **&argv) {
        SharedData::mainArgc = argc;
        SharedData::mainArgv = argv;

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

                try {
                    status = task() && status;
                } catch (...) {
                    status = false;
                }

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
        ImTextureID splashTexture;
        u32 splashWidth, splashHeight;

        for (const auto &path : hex::getPath(hex::ImHexPath::Resources)) {
            std::tie(splashTexture, splashWidth, splashHeight) = ImGui::LoadImageFromPath((path + "/splash.png").c_str());
            if (splashTexture != nullptr)
                break;
        }

        if (splashTexture == nullptr) {
            log::fatal("Could not load splash screen image!");
            exit(EXIT_FAILURE);
        }

        ON_SCOPE_EXIT { ImGui::UnloadImage(splashTexture); };

        auto tasksSucceeded = processTasksAsync();

        while (!glfwWindowShouldClose(this->m_window)) {
            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            {
                std::lock_guard guard(this->m_progressMutex);

                auto drawList = ImGui::GetOverlayDrawList();

                drawList->AddImage(splashTexture, ImVec2(0, 0), ImVec2(splashWidth, splashHeight));

                #if defined(DEBUG)
                    drawList->AddText(ImVec2(15, 120), ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("WerWolv 2020 - {0}", &__DATE__[7]).c_str());
                    drawList->AddText(ImVec2(15, 140), ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("{0} : {1} {2}@{3}", IMHEX_VERSION, ICON_FA_CODE_BRANCH, GIT_BRANCH, GIT_COMMIT_HASH).c_str());
                #else
                    drawList->AddText(ImVec2(15, 120), ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("WerWolv 2020 - {0}", &__DATE__[7]).c_str());
                    drawList->AddText(ImVec2(15, 140), ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("{0}", IMHEX_VERSION).c_str());
                #endif
                drawList->AddRectFilled(ImVec2(0, splashHeight - 5), ImVec2(splashWidth * this->m_progress, splashHeight), 0xFFFFFFFF);
                drawList->AddText(ImVec2(15, splashHeight - 25), ImColor(0xFF, 0xFF, 0xFF, 0xFF),
                                  hex::format("[{}] {}", "|/-\\"[ImU32(ImGui::GetTime() * 15) % 4], this->m_currTaskName).c_str());
            }

            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(this->m_window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(this->m_window);

            if (tasksSucceeded.wait_for(0s) == std::future_status::ready) {
                return tasksSucceeded.get();
            }
        }

        return false;
    }

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

    void WindowSplash::initGLFW() {
        glfwSetErrorCallback([](int error, const char *description) {
            log::fatal("GLFW Error: {0} - {0}", error, description);
            exit(EXIT_FAILURE);
        });

        if (!glfwInit()) {
            log::fatal("Failed to initialize GLFW!");
            exit(EXIT_FAILURE);
        }


        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

        this->m_window = glfwCreateWindow(640, 400, "ImHex", nullptr, nullptr);
        if (this->m_window == nullptr) {
            log::fatal("Failed to create GLFW window!");
            exit(EXIT_FAILURE);
        }

        centerWindow(this->m_window);

        glfwMakeContextCurrent(this->m_window);
        glfwSwapInterval(1);

        if (gladLoadGL() == 0) {
            log::fatal("Failed to load OpenGL Context!");
            exit(EXIT_FAILURE);
        }
    }

    void WindowSplash::initImGui() {
        IMGUI_CHECKVERSION();
        GImGui = ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(this->m_window, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        auto &io = ImGui::GetIO();

        io.Fonts->Clear();

        ImFontConfig cfg;
        cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
        cfg.SizePixels = 13.0f;
        io.Fonts->AddFontDefault(&cfg);

        cfg.MergeMode = true;

        ImWchar fontAwesomeRange[] = {
                ICON_MIN_FA, ICON_MAX_FA,
                0
        };
        std::uint8_t *px;
        int w, h;
        io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_compressed_data, font_awesome_compressed_size, 11.0f, &cfg, fontAwesomeRange);
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