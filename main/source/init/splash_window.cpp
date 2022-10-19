#include "init/splash_window.hpp"

#include <hex/api/imhex_api.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>

#include <romfs/romfs.hpp>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <fonts/fontawesome_font.h>
#include <GLFW/glfw3.h>

#include <unistd.h>

#include <chrono>
#include <future>
#include <numeric>
#include <chrono>

using namespace std::literals::chrono_literals;

namespace hex::init {

    WindowSplash::WindowSplash() : m_window(nullptr) {
        this->initGLFW();
        this->initImGui();

        ImHexApi::System::impl::setGPUVendor(reinterpret_cast<const char *>(glGetString(GL_VENDOR)));
    }

    WindowSplash::~WindowSplash() {
        this->exitImGui();
        this->exitGLFW();
    }


    std::future<bool> WindowSplash::processTasksAsync() {
        return std::async(std::launch::async, [this] {
            bool status = true;

            std::atomic<u32> tasksCompleted = 0;
            for (const auto &[name, task, async] : this->m_tasks) {
                auto runTask = [&, task = task, name = name] {
                    {
                        std::lock_guard guard(this->m_progressMutex);
                        this->m_currTaskName = name;
                    }

                    using namespace std::chrono;

                    auto startTime = high_resolution_clock::now();
                    if (!task())
                        status = false;
                    auto endTime = high_resolution_clock::now();

                    log::info("Task {} done in {}ms", name, duration_cast<milliseconds>(endTime-startTime).count());

                    tasksCompleted++;

                    this->m_progress = float(tasksCompleted) / this->m_tasks.size();
                };

                try {
                    if (async) {
                        TaskManager::createBackgroundTask(name, [runTask](auto&){ runTask(); });
                    } else {
                        runTask();
                    }

                } catch (std::exception &e) {
                    log::error("Init task '{}' threw an exception: {}", name, e.what());
                    status = false;
                }
            }

            while (tasksCompleted < this->m_tasks.size()) {
                std::this_thread::sleep_for(100ms);
            }

            // Small extra delay so the last progress step is visible
            std::this_thread::sleep_for(100ms);

            return status;
        });
    }

    bool WindowSplash::loop() {
        auto splash                  = romfs::get("splash.png");
        ImGui::Texture splashTexture = ImGui::Texture(reinterpret_cast<const ImU8 *>(splash.data()), splash.size());

        if (!splashTexture.isValid()) {
            log::fatal("Could not load splash screen image!");
            exit(EXIT_FAILURE);
        }

        auto tasksSucceeded = processTasksAsync();

        auto scale = ImHexApi::System::getGlobalScale();

        while (!glfwWindowShouldClose(this->m_window)) {
            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            {
                std::lock_guard guard(this->m_progressMutex);

                auto drawList = ImGui::GetForegroundDrawList();

                drawList->AddImage(splashTexture, ImVec2(0, 0), splashTexture.getSize() * scale);

                drawList->AddText(ImVec2(15, 120) * scale, ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("WerWolv 2020 - {0}", &__DATE__[7]).c_str());

                #if defined(DEBUG) && defined(GIT_BRANCH) && defined(GIT_COMMIT_HASH)
                    drawList->AddText(ImVec2(15, 140) * scale, ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("{0} : {1} {2}@{3}", IMHEX_VERSION, ICON_FA_CODE_BRANCH, GIT_BRANCH, GIT_COMMIT_HASH).c_str());
                #else
                    drawList->AddText(ImVec2(15, 140) * scale, ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("{0}", IMHEX_VERSION).c_str());
                #endif

                drawList->AddRectFilled(ImVec2(0, splashTexture.getSize().y - 5) * scale, ImVec2(splashTexture.getSize().x * this->m_progress, splashTexture.getSize().y) * scale, 0xFFFFFFFF);
                drawList->AddText(ImVec2(15, splashTexture.getSize().y - 25) * scale, ImColor(0xFF, 0xFF, 0xFF, 0xFF), hex::format("[{}] {}", "|/-\\"[ImU32(ImGui::GetTime() * 15) % 4], this->m_currTaskName).c_str());
            }

            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(this->m_window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.00F, 0.00F, 0.00F, 0.00F);
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
        glfwSetErrorCallback([](int error, const char *desc) {
            log::error("GLFW Error [{}] : {}", error, desc);
        });

        if (!glfwInit()) {
            log::fatal("Failed to initialize GLFW!");
            exit(EXIT_FAILURE);
        }

        #if defined(OS_MACOS)
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        #else
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        #endif

        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);

        this->m_window = glfwCreateWindow(1, 400, "Starting ImHex...", nullptr, nullptr);
        if (this->m_window == nullptr) {
            log::fatal("Failed to create GLFW window!");
            exit(EXIT_FAILURE);
        }

        // Calculate native scale factor for hidpi displays
        {
            float xScale = 0, yScale = 0;
            glfwGetWindowContentScale(this->m_window, &xScale, &yScale);

            auto meanScale = std::midpoint(xScale, yScale);
            if (meanScale <= 0.0)
                meanScale = 1.0;

            ImHexApi::System::impl::setGlobalScale(meanScale);
            ImHexApi::System::impl::setNativeScale(meanScale);

            log::info("Native scaling set to: {:.1f}", meanScale);
        }

        glfwSetWindowSize(this->m_window, 640_scaled, 400_scaled);
        centerWindow(this->m_window);

        glfwMakeContextCurrent(this->m_window);
        glfwSwapInterval(1);
    }

    void WindowSplash::initImGui() {
        IMGUI_CHECKVERSION();
        GImGui = ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(this->m_window, true);

        #if defined(OS_MACOS)
            ImGui_ImplOpenGL3_Init("#version 150");
        #else
            ImGui_ImplOpenGL3_Init("#version 130");
        #endif

        auto &io = ImGui::GetIO();

        ImGui::GetStyle().ScaleAllSizes(ImHexApi::System::getGlobalScale());

        io.Fonts->Clear();

        ImFontConfig cfg;
        cfg.OversampleH = cfg.OversampleV = 1, cfg.PixelSnapH = true;
        cfg.SizePixels = 13.0_scaled;
        io.Fonts->AddFontDefault(&cfg);

        cfg.MergeMode = true;

        ImWchar fontAwesomeRange[] = {
            ICON_MIN_FA, ICON_MAX_FA, 0
        };
        std::uint8_t *px;
        int w, h;
        io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_compressed_data, font_awesome_compressed_size, 11.0_scaled, &cfg, fontAwesomeRange);
        io.Fonts->GetTexDataAsRGBA32(&px, &w, &h);

        // Create new font atlas
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA8, GL_UNSIGNED_INT, px);
        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(tex));
        io.IniFilename = nullptr;
    }

    void WindowSplash::exitGLFW() {
        glfwDestroyWindow(this->m_window);
        glfwTerminate();
    }

    void WindowSplash::exitImGui() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

}
